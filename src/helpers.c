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
 *         Arjan van de Ven <arjan@linux.intel.com>
 *
 */

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "swupd.h"

FILE *fopen_exclusive(const char *filename) /* no mode, opens for write only */
{
	int fd;

	fd = open(filename, O_CREAT | O_EXCL | O_RDWR, 00644);
	if (fd < 0) {
		printf("exclusive open failed, filename=\"%s\",err=\"%s\"\n",
		       filename, strerror(errno));
		LOG(NULL, "exclusive open failed", "\\*filename=\"%s\",err=\"%s\"*\\",
		    filename, strerror(errno));
		return NULL;
	}
	return fdopen(fd, "w");
}

void dump_file_info(struct file *file)
{
	printf("%s:\n", file->filename);
	printf("\t%s\n", file->hash);
	printf("\t%d\n", file->last_change);

	if (file->use_xattrs) {
		printf("\tuse_xattrs\n");
	}
	if (file->is_dir) {
		printf("\tis_dir\n");
	}
	if (file->is_file) {
		printf("\tis_file\n");
	}
	if (file->is_link) {
		printf("\tis_link\n");
	}
	if (file->is_deleted) {
		printf("\tis_deleted\n");
	}
	if (file->is_manifest) {
		printf("\tis_manifest\n");
	}
	if (file->is_config) {
		printf("\tis_config\n");
	}
	if (file->is_state) {
		printf("\tis_state\n");
	}
	if (file->is_boot) {
		printf("\tis_boot\n");
	}
	if (file->is_rename) {
		printf("\tis_rename\n");
	}

	if (file->peer) {
		printf("\tpeer %s(%s)\n", file->peer->filename, file->peer->hash);
	}
}

/* Tiny memory allocations don't fail or it's the end of the world.  But
 * source checkers like to see checked returns.  But repeatedly checking
 * returns introduces bugs.  So here's the asprintf checking helper. */
void string_or_die(char **strp, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (vasprintf(strp, fmt, ap) < 0) {
		assert(0);
	}
	va_end(ap);
}

/* "step" should be a descriptive string for the process that completed before
 * this function is called */
void print_elapsed_time(const char *step, struct timeval *previous_time,
			struct timeval *current_time)
{
	char *elapsed;

	assert(strlen(step) > 0);

	gettimeofday(current_time, NULL);
	elapsed = get_elapsed_time(previous_time, current_time);
	if (strcmp(elapsed, " ") == 0) {
		printf("\t~0.0 elapsed for '%s' step\n", step);
	} else {
		printf("\t%s elapsed for '%s' step\n", elapsed, step);
	}
	previous_time->tv_sec = current_time->tv_sec;
	previous_time->tv_usec = current_time->tv_usec;

	free(elapsed);
}

void concat_str_array(char **output, char *const argv[])
{
	int size = 0;

	for (int i = 0; argv[i]; i++) {
		size += strlen(argv[i]) + 1;
	}

	*output = malloc(size + 1);
	if (!*output) {
		LOG(NULL, "Failed to allocate", "%i bytes", size);
		assert(0);
	}
	strcpy(*output, "");
	for (int i = 0; argv[i]; i++) {
		strcat(*output, argv[i]);
		strcat(*output, " ");
	}
}

int system_argv_pipe(char *const lhscmd[], char *const rhscmd[])
{
	return system_argv_pipe_fd(-1, -1, lhscmd, -1, -1, rhscmd);
}

int system_argv_pipe_fd(int lnewstdinfd, int lnewstderrfd, char *const lhscmd[],
			int rnewstdoutfd, int rnewstderrfd, char *const rhscmd[])
{
	pid_t monitorpid = fork();
	if (monitorpid == -1) {
		LOG(NULL, "Failed to create child process to monitor pipe between", "command %s and command %s", lhscmd[0], rhscmd[0]);
		return -1;
	} else if (monitorpid == 0) {
		pipe_monitor(lnewstdinfd, lnewstderrfd, lhscmd, rnewstdoutfd, rnewstderrfd, rhscmd);
	}
	return wait_process_terminate(monitorpid);
}

void pipe_monitor(int lnewstdinfd, int lnewstderrfd, char *const lhscmd[],
		  int rnewstdoutfd, int rnewstderrfd, char *const rhscmd[])
{
	int pipefd[2];
	if (pipe(pipefd) == -1) {
		LOG(NULL, "Failed to create a pipe between", "command %s and command %s", lhscmd[0], rhscmd[0]);
		assert(0);
	}

	pid_t lhspid = system_argv_fd_nowait(lnewstdinfd, pipefd[1], lnewstderrfd, pipefd[0], lhscmd);
	pid_t rhspid = system_argv_fd_nowait(pipefd[0], rnewstdoutfd, rnewstderrfd, pipefd[1], rhscmd);

	if (close(pipefd[1]) == -1) {
		LOG(NULL, "Could not close write end of pipe file descriptor", "%d", pipefd[1]);
		assert(0);
	}
	if (close(pipefd[0]) == -1) {
		LOG(NULL, "Could not close read end of pipe file descriptor", "%d", pipefd[0]);
		assert(0);
	}

	int lhsresult = wait_process_terminate(lhspid);
	int rhsresult = wait_process_terminate(rhspid);
	exit(rhsresult != EXIT_SUCCESS ? rhsresult : lhsresult);
}

int system_argv(char *const argv[])
{
	return system_argv_fd(-1, -1, -1, argv);
}

int system_argv_fd(int newstdinfd, int newstdoutfd, int newstderrfd, char *const cmd[])
{
	pid_t cmdpid = system_argv_fd_nowait(newstdinfd, newstdoutfd, newstderrfd, -1, cmd);
	return wait_process_terminate(cmdpid);
}

pid_t system_argv_fd_nowait(int newstdinfd, int newstdoutfd, int newstderrfd, int closefd, char *const cmd[])
{
	pid_t cmdpid = fork();
	if (cmdpid == -1) {
		LOG(NULL, "Failed to fork to execute command", "%s", cmd[0]);
		assert(0);
	} else if (cmdpid == 0) {
		exec_cmd_fd(newstdinfd, newstdoutfd, newstderrfd, closefd, cmd);
	}
	return cmdpid;
}

void exec_cmd_fd(int newstdinfd, int newstdoutfd, int newstderrfd, int closefd, char *const cmd[])
{
	move_fd(newstdinfd, STDIN_FILENO);
	move_fd(newstdoutfd, STDOUT_FILENO);
	move_fd(newstderrfd, STDERR_FILENO);
	if (closefd >= 0 && close(closefd) == -1) {
		LOG(NULL, "Could not close file descriptor", "%d", closefd);
		assert(0);
	}
	execvp(*cmd, cmd);
	LOG(NULL, "Command", "%s failed", cmd[0]);
	assert(0);
}

void move_fd(int oldfd, int newfd)
{
	if (oldfd < 0 || newfd < 0 || oldfd == newfd) {
		return;
	}
	if (dup2(oldfd, newfd) == -1) {
		LOG(NULL, "Could not create duplicate file descriptor", "%d from %d", newfd, oldfd);
		assert(0);
	}
	if (close(oldfd) == -1) {
		LOG(NULL, "Could not close file descriptor", "%d", oldfd);
		assert(0);
	}
}

int wait_process_terminate(pid_t pid)
{
	int status;
	do {
		if (waitpid(pid, &status, 0) == -1) {
			LOG(NULL, "Failed to wait for PID", "%d", pid);
			return -1;
		}
	} while (!WIFEXITED(status) && !WIFSIGNALED(status));
	// Exit statuses fall in the range of [0, 255].  Make signal statuses fall in a non-overlapping range starting with 256.
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	} else {
		return 256 + WTERMSIG(status);
	}
}

void check_root(void)
{
	if (getuid() != 0) {
		printf(PACKAGE_NAME " code really wants to be run as root at this point.\n");
		printf("However, currently it's not being run as root.. exiting\n");
		printf("\n");
		exit(EXIT_FAILURE);
	}
}

int num_threads(float scaling)
{
	const char *var = getenv("SWUPD_NUM_THREADS");
	int result = sysconf(_SC_NPROCESSORS_ONLN) * scaling;

	if (var && *var) {
		char *endptr;
		long int value;

		errno = 0;
		value = strtol(var, &endptr, 0);

		if ((errno != 0 && value == 0) || *endptr) {
			LOG(NULL, "SWUPD_NUM_THREADS must be an integer", "%s", var);
		} else if ((errno == ERANGE && (value == LONG_MAX || value == LONG_MIN)) ||
			   value < 1 || value > INT_MAX) {
			LOG(NULL, "SWUPD_NUM_THREADS out of range", "%s", var);
		} else {
			result = (int)value;
		}
	}

	return result;
}

/* This function is called when configuration specifies a ban on debuginfo files
 * from manifests. Returns true if the passed file path matches the src or lib
 * debuginfo configuration. */
bool file_is_debuginfo(const char *path)
{
	bool ret = false;
	char *lib;
	char *src;

	lib = config_debuginfo_path("lib");
	src = config_debuginfo_path("src");

	if (lib && (strncmp(path, lib, strlen(lib)) == 0)) {
		ret = true;
		goto out;
	}

	if (src && (strncmp(path, src, strlen(src)) == 0)) {
		ret = true;
		goto out;
	}

out:
	free(lib);
	free(src);
	return ret;
}
