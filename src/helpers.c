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
	if (vasprintf(strp, fmt, ap) <= 0) {
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

int system_argv(char *const argv[])
{
	int child_exit_status;
	pid_t pid;
	int status;

	pid = fork();

	if (pid == 0) { /* child */
		execvp(*argv, argv);
		LOG(NULL, "This line must not be reached", "");
		assert(0);
	} else if (pid < 0) {
		LOG(NULL, "Failed to fork a child process", "");
		assert(0);
	} else {
		pid_t ws = waitpid(pid, &child_exit_status, 0);

		if (ws == -1) {
			LOG(NULL, "Failed to wait for child process", "");
			assert(0);
		}

		if (WIFEXITED(child_exit_status)) {
			status = WEXITSTATUS(child_exit_status);
		} else {
			LOG(NULL, "Child process didn't exit", "");
			assert(0);
		}

		if (status != 0) {
			char *cmdline = NULL;

			concat_str_array(&cmdline, argv);
			LOG(NULL, "Failed to run command:", "%s", cmdline);
			free(cmdline);
		}

		return status;
	}
}

int system_argv_fd(char *const argv[], int stdin, int stdout, int stderr)
{
	int child_exit_status;
	pid_t pid;
	int status;

	pid = fork();

	if (pid == 0) { /* child */
		if(stdin >= 0) {
			dup2(stdin, STDIN_FILENO);
			close(stdin);
		}
		if(stdout >= 0) {
			dup2(stdout, STDOUT_FILENO);
			close(stdout);
		}
		if(stderr >= 0) {
			dup2(stderr, STDERR_FILENO);
			close(stderr);
		}

		execvp(*argv, argv);
		LOG(NULL, "This line must not be reached", "");
		assert(0);
	} else if (pid < 0) {
		LOG(NULL, "Failed to fork a child process", "");
		assert(0);
	} else {
		pid_t ws = waitpid(pid, &child_exit_status, 0);

		if (ws == -1) {
			LOG(NULL, "Failed to wait for child process", "");
			assert(0);
		}

		if (WIFEXITED(child_exit_status)) {
			status = WEXITSTATUS(child_exit_status);
		} else {
			LOG(NULL, "Child process didn't exit", "");
			assert(0);
		}

		if (status != 0) {
			char *cmdline = NULL;

			concat_str_array(&cmdline, argv);
			LOG(NULL, "Failed to run command:", "%s", cmdline);
			free(cmdline);
		}

		return status;
	}
}

int system_argv_pipe(char *const argvp1[], int stdinp1, int stderrp1,
					 char *const argvp2[], int stdoutp2, int stderrp2)
{
	int statusp1, statusp2;
	int pipefd[2];

	if (pipe(pipefd)) {
		LOG(NULL, "Failed to create a pipe", "");
		return -1;
	}
	statusp1 = system_argv_fd(argvp1, stdinp1, pipefd[1], stderrp1);
	close(pipefd[1]);
	statusp2 = system_argv_fd(argvp2, pipefd[0], stdoutp2, stderrp2);
	close(pipefd[0]);

	/* Returns the status of the failed process if any
       If both processes failed returns the status of first one */
	return statusp1 ? statusp1 : statusp2;
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
