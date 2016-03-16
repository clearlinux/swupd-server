/*
 *   Software Updater - server side
 *
 *      Copyright © 2012-2016 Intel Corporation.
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
 *         Tom Keel <thomas.keel@intel.com>
 *
 */

#define _GNU_SOURCE
#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "swupd.h"

static char *make_filename(const char *, const char *, const char *);

static const char *CMD_FMT = "openssl smime -sign -in %s -binary "
			     "-out %s.signed -outform PEM -md sha256 -inkey %s -signer %s "
			     "-certfile %s -passin file:%s";

static char *leaf_key = NULL;
static char *leaf_cert = NULL;
static char *ca_chain_cert = NULL;
static char *passphrase = NULL;

static bool initialized = false;

/*
 * Initialize this module.
 * @return true <=> success
 */
bool signature_initialize(void)
{
	if (!enable_signing) {
		return true;
	}

	char *cdir;
	char *pphr;
	struct stat s;

	if (initialized) {
		return true;
	}
	cdir = getenv("SWUPD_CERTS_DIR");
	if (cdir == NULL || cdir[0] == '\0') {
		printf("No certificates directory specified\n");
		goto err;
	}
	if (stat(cdir, &s)) {
		printf("Can't stat certificates directory '%s' (%s)\n", cdir,
		       strerror(errno));
		goto err;
	}
	leaf_key = make_filename(cdir, "LEAF_KEY", "leaf key");
	if (leaf_key == NULL) {
		goto err;
	}
	leaf_cert = make_filename(cdir, "LEAF_CERT", "leaf certificate");
	if (leaf_cert == NULL) {
		goto err;
	}
	ca_chain_cert = make_filename(cdir, "CA_CHAIN_CERT", "CA chain certificate");
	if (ca_chain_cert == NULL) {
		goto err;
	}
	pphr = getenv("PASSPHRASE");
	if (pphr == NULL || (passphrase = strdup(pphr)) == NULL) {
		goto err;
	}
	if (stat(passphrase, &s)) {
		printf("Can't stat '%s' (%s)\n", passphrase,
		       strerror(errno));
		goto err;
	}
	initialized = true;
	return true;
err:
	signature_terminate();
	return false;
}

/* Make filename from dir name and env variable containing basename */
static char *make_filename(const char *dir, const char *env, const char *desc)
{
	char *fn = getenv(env);
	char *result = NULL;
	struct stat s;

	if (fn == NULL || fn[0] == '\0') {
		printf("No %s file specified\n", desc);
		return NULL;
	}
	string_or_die(&result, "%s/%s", dir, fn);
	if (stat(result, &s)) {
		printf("Can't stat %s '%s' (%s)\n", desc, result, strerror(errno));
		free(result);
		return NULL;
	}
	return result;
}

/*
 * Terminate this module, free resources.
 */
void signature_terminate(void)
{
	if (!enable_signing) {
		return;
	}

	free(leaf_key);
	free(leaf_cert);
	free(ca_chain_cert);
	free(passphrase);

	leaf_key = NULL;
	leaf_cert = NULL;
	ca_chain_cert = NULL;
	passphrase = NULL;

	initialized = false;
}

/*
 * Write the signature file corresponding to the given data file.
 * The name of the signature file is the name of the data file with suffix
 * ".signed" appended.
 */
bool signature_sign(const char *filename)
{
	char *cmd = NULL;
	int status;

	if (!enable_signing) {
		return true;
	}

	if (!initialized) {
		return false;
	}
	string_or_die(&cmd, CMD_FMT, filename, filename, leaf_key, leaf_cert,
		      ca_chain_cert, passphrase);
	status = system(cmd);
	if (status) {
		printf("Bad status %d from signing command:%s\n", status, cmd);
	}
	free(cmd);
	return status == 0;
}
