/*
 *   Software Updater - server side
 *
 *      Copyright © 2016 Intel Corporation.
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, version 2 or later of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Authors:
 *         Patrick Ohly <patrick.ohly@intel.com>
 *
 */

#include <glib.h>
#include <curl/curl.h>
#include <archive.h>
#include <archive_entry.h>
#include <stdlib.h>

#include "curl_helper.h"
#include "swupd.h"

static GOnce curl_helper_once = G_ONCE_INIT;
static GThreadPool *curl_helper_pool;

static void curl_helper_perform(gpointer data, gpointer user_data);

static gpointer curl_helper_init_once(gpointer __unused__ unused)
{
	curl_global_init(CURL_GLOBAL_ALL);
	/*
	 * We allow creating as many additional threads as needed to
	 * match the number of active curl_helper_unpack_tar() calls.
	 * That way each call is guaranteed to make progress.
	 * glib will allocate additional threads, even if the other
	 * non-exclusive thread pools have a limit.
	 */
	curl_helper_pool = g_thread_pool_new(curl_helper_perform, NULL,
					     -1, FALSE, NULL);

	return 0;
}

/**
 * Allocate and initialize global state for use of curl. Because curl
 * might not be needed at all, this function may be called more than
 * once and is guaranteed to be thread-safe.
 */
void curl_helper_init()
{
	g_once(&curl_helper_once, curl_helper_init_once, 0);
}

/**
 * Free resources that might (or might not) have been allocated.
 * Not thread-safe.
 */
void curl_helper_free()
{
	if (curl_helper_once.status == G_ONCE_STATUS_READY) {
		curl_global_cleanup();
		g_thread_pool_free(curl_helper_pool, false, false);
		curl_helper_once.status = G_ONCE_STATUS_READY;
	}
}

/** Used for buffering data between threads. */
#define CURL_HELPER_TRANSFER_SIZE (1 * 1024 * 1024)

struct curl_helper_transfer
{
	CURL *curl;

	/* Protects the following struct members. */
	GMutex mutex;
	GCond cond;

	/**
	 * We do zero-copy by letting libarchive process directly the
	 * buffer handed in by libcurl. It is debatable whether zero-copy
	 * with higher overhead for context switching is more efficient
	 * than double-buffering with more memcpy. Zero-copy is probably
	 * a bit less code.
	 *
	 * The buffer is set while we have data ready to be processed.
	 * Writing blocks in libcurl until the data is fully handled.
	 */
	const char *buffer;

	/** Number of bytes in buffer. */
	size_t available;

	/** True while libarchive is working on the buffer. */
	bool processing;

	/** True while data is coming in. */
	bool writing;

	/** True while data is taken out. */
	bool reading;

	/** Result and message only valid when not writing anymore. */
	CURLcode res;
	char message[CURL_ERROR_SIZE];
};

struct curl_helper_transfer *
curl_helper_transfer_new()
{
	struct curl_helper_transfer *transfer;

	transfer = calloc(1, sizeof(*transfer));
	g_mutex_init(&transfer->mutex);
	g_cond_init(&transfer->cond);
	transfer->writing = true;
	transfer->reading = true;
	return transfer;
}

static size_t
curl_helper_transfer_write(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	struct curl_helper_transfer *transfer = userdata;
	size_t written;

	g_mutex_lock(&transfer->mutex);
	if (transfer->reading) {
		static const char empty_buffer[1];
		/*
		 * Hand over new buffer and wait until reader is done
		 * with it. We need a valid buffer pointer even when
		 * no data was coming in from libcurl.
		 */
		transfer->buffer = ptr ? ptr : empty_buffer;
		written = size * nmemb;
		transfer->available = written;
		g_cond_signal(&transfer->cond);
		while (transfer->buffer) {
			g_cond_wait(&transfer->cond, &transfer->mutex);
		}
	} else {
		/* Error, reader is gone but we still have data. */
		written = 0;
	}
	g_mutex_unlock(&transfer->mutex);
	return written;
}

static ssize_t curl_helper_transfer_read(struct archive __unused__ *a, void *client_data, const void **buff)
{
	struct curl_helper_transfer *transfer = client_data;
	ssize_t read;

	g_mutex_lock(&transfer->mutex);
	if (transfer->processing) {
		/* Tell writer that we are done with the previous buffer. */
		transfer->buffer = NULL;
		transfer->processing = false;
		g_cond_signal(&transfer->cond);
	}

	while (!transfer->buffer && transfer->writing) {
		/* Wait for next buffer or end of writing. */
		g_cond_wait(&transfer->cond, &transfer->mutex);
	}

	if (transfer->buffer) {
		/* Process next chunk. */
		*buff = transfer->buffer;
		read = transfer->available;
		transfer->processing = true;
	} else if (transfer->res == CURLE_OK) {
		/* Normal EOF. */
		read = 0;
	} else {
		/* Signal error. */
		read = -1;
	}
	g_mutex_unlock(&transfer->mutex);

	return read;
}

static int curl_helper_transfer_close(struct archive __unused__ *a, void *client_data)
{
	struct curl_helper_transfer *transfer = client_data;

	g_mutex_lock(&transfer->mutex);
	if (transfer->processing) {
		transfer->buffer = NULL;
		transfer->processing = false;
	}
	transfer->reading = false;
	g_cond_signal(&transfer->cond);
	g_mutex_unlock(&transfer->mutex);

	return ARCHIVE_OK;
}

static int curl_helper_copy_data(struct archive *ar, struct archive *aw)
{
	int r;
	const void *buffer;
	size_t size;
	off_t offset;

	for (;;) {
		r = archive_read_data_block(ar, &buffer, &size, &offset);
		if (r == ARCHIVE_EOF) {
			return ARCHIVE_OK;
		} else if (r != ARCHIVE_OK) {
			LOG(NULL, "Error reading data from archive: %s", archive_error_string(ar));
			return r;
		}

		r = archive_write_data_block(aw, buffer, size, offset);
		if (r != ARCHIVE_OK) {
			LOG(NULL, "Error writing data from archive: %s", archive_error_string(aw));
			return r;
		}
	}
}

/**
 * Retrieves the file identified by the url and directly unpacks
 * the archive with libarchive inside the target directory.
 * Thread-safe.
 */
int curl_helper_unpack_tar(const char *url, const char *target_dir)
{
	struct curl_helper_transfer *transfer = NULL;
	int ret = CURL_HELPER_FAILURE;
	const char *cainfo;
	struct archive *a = NULL, *ext = NULL;
	struct archive_entry *entry;
	int r;
	int flags;

	curl_helper_init();

	/*
	 * Both libcurl and libarchive want to be in control. There's
	 * no way how a single thread can get some data out of libcurl
	 * (pull) and hand it over to libarchive (push) for further
	 * processing: libcurl wants to push, and libarchive wants to
	 * pull. To get around this, we put libcurl processing into a
	 * helper thread which copies data into a buffer which gets
	 * drained by the libarchive read callbacks.
	 */
	transfer = curl_helper_transfer_new();
	transfer->curl = curl_easy_init();
	if (!transfer->curl) {
		goto error;
	}
	curl_easy_setopt(transfer->curl, CURLOPT_URL, url);
	curl_easy_setopt(transfer->curl, CURLOPT_ERRORBUFFER, transfer->message);
	curl_easy_setopt(transfer->curl, CURLOPT_WRITEFUNCTION, curl_helper_transfer_write);
	curl_easy_setopt(transfer->curl, CURLOPT_WRITEDATA, transfer);
	/*
	 * Mirror the behavior of curl and check CURL_CA_BUNDLE.
	 * This is relevant for builds under OpenEmbedded, where the
	 * builtin default path becomes invalid when moving the
	 * native binary from one build machine to another (YOCTO #9883),
	 * but may also be useful for pointing swupd to a self-signed
	 * certificate that isn't installed on the system.
	 */
	cainfo = getenv("CURL_CA_BUNDLE");
	if (cainfo && cainfo[0]) {
		curl_easy_setopt(transfer->curl, CURLOPT_CAINFO, cainfo);
	}

	a = archive_read_new();
	if (!a) {
		LOG(NULL, "Failed to allocate archive for reading.", "");
		goto error;
	}

	/* Set which attributes we want to restore. */
	flags = ARCHIVE_EXTRACT_TIME;
	flags |= ARCHIVE_EXTRACT_PERM;
	flags |= ARCHIVE_EXTRACT_OWNER;
	flags |= ARCHIVE_EXTRACT_XATTR;

	/* Set security flags. However, ultimately the server trusts
	 * the content of the archive to be correct. */
	flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS;
	flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;

	/* Limit parsing to tar. */
	r = archive_read_support_format_tar(a);
	if (r != ARCHIVE_OK) {
		LOG(NULL, "Could not initialize tar processing", "%s", archive_error_string(a));
		goto error;
	}

	/* All compression methods. */
	r = archive_read_support_filter_all(a);
	if (r != ARCHIVE_OK) {
		LOG(NULL, "Could not initialize decompression", "%s", archive_error_string(a));
		goto error;
	}

	/* set up write */
	ext = archive_write_disk_new();
	if (!ext) {
		LOG(NULL, "Failed to allocate archive for writing.", "");
		goto error;
	}

	r = archive_write_disk_set_options(ext, flags);
	if (r != ARCHIVE_OK) {
		LOG(NULL, "Failed to set archive write options", "%s", archive_error_string(ext));
		goto error;
	}

	r = archive_write_disk_set_standard_lookup(ext);
	if (r != ARCHIVE_OK) {
		LOG(NULL, "Failed to set archive write options", "%s", archive_error_string(ext));
		goto error;
	}

	/* Start data transfer for real now. */
	g_thread_pool_push(curl_helper_pool, transfer, NULL);
	r = archive_read_open(a, transfer, NULL, curl_helper_transfer_read, curl_helper_transfer_close);
	if (r != ARCHIVE_OK) {
		LOG(NULL, "Failed to initialize archive reading", "%s", archive_error_string(a));
		goto error;
	}
	for (;;) {
		r = archive_read_next_header(a, &entry);
		if (r == ARCHIVE_EOF) {
			/* Reached end of archive without errors. */
			ret = CURL_HELPER_OKAY;
			break;
		} else if (r != ARCHIVE_OK) {
			LOG(NULL, "Error while looking for next archive entry", "%s", archive_error_string(a));
			goto error_writing;
		}

		/* Set output directory. */
		char *fullpath;
		string_or_die(&fullpath, "%s/%s", target_dir, archive_entry_pathname(entry));
		archive_entry_set_pathname(entry, fullpath);
		free(fullpath);

		/* Write archive header, if successful continue to copy data. */
		r = archive_write_header(ext, entry);
		if (r != ARCHIVE_OK) {
			LOG(NULL, "Error creating archive entry", "%s", archive_error_string(ext));
			goto error_writing;
		}

		if (archive_entry_size(entry) > 0) {
			r = curl_helper_copy_data(a, ext);
			if (r != ARCHIVE_OK) {
				LOG(NULL, "Error copying archive data", "%s");
				goto error_writing;
			}
		}

		/* Flush pending file attribute changes. */
		r = archive_write_finish_entry(ext);
		if (r != ARCHIVE_OK) {
			LOG(NULL, "Error closing archive entry", "%s", archive_error_string(ext));
			goto error_writing;
		}
	}
	r = archive_write_close(ext);
	if (r != ARCHIVE_OK) {
		LOG(NULL, "Error closing the write archive", "%s", archive_error_string(ext));
		goto error_writing;
	}

 error_writing:
	/* We started the data transfer, so now we must wait for the libcurl thread to stop writing. */
	g_mutex_lock(&transfer->mutex);
	if (transfer->processing) {
		transfer->buffer = NULL;
		transfer->processing = false;
	}
	transfer->reading = false;
	g_cond_signal(&transfer->cond);
	while (transfer->writing) {
		g_cond_wait(&transfer->cond, &transfer->mutex);
	}
	g_mutex_unlock(&transfer->mutex);
	if (transfer->res != CURLE_OK) {
		LOG(NULL, "Archive download error", "%s", transfer->message);
	}

 error:
	if (ext) {
		archive_write_free(ext);
	}
	if (a) {
		archive_read_free(a);
	}
	if (transfer) {
		free(transfer);
	}
	return ret;
}

static void curl_helper_perform(gpointer data, gpointer __unused__ user_data)
{
	struct curl_helper_transfer *transfer = (struct curl_helper_transfer *)data;
	CURLcode res;

	res = curl_easy_perform(transfer->curl);

	g_mutex_lock(&transfer->mutex);
	transfer->res = res;
	transfer->writing = false;
	g_cond_signal(&transfer->cond);
	g_mutex_unlock(&transfer->mutex);
}
