/*-
 * Copyright 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Google nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <unistd.h>

#include <atf-c.h>

/* Create child process and block for successful termination. */
void
run_truncate(const char *const *argv)
{
	int status;
	pid_t child;
	extern char **environ;

	ATF_REQUIRE(posix_spawnp(&child, argv[0], NULL, NULL,
	    (char *const *)argv, environ) == 0);
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 0);
}

/* Return the number of bytes used for holes or data in this file. */
off_t
get_size(const char *path, int whence)
{
	int fd, opposite;
	off_t hstart, hend, total;

	assert(whence == SEEK_HOLE || whence == SEEK_DATA);

	/* We expect the file to exist and to have SEEK_HOLE support. */
	ATF_REQUIRE((fd = open(path, O_RDONLY)) != -1);
	if (fpathconf(fd, _PC_MIN_HOLE_SIZE) <= 0)
		atf_tc_skip("Require _PC_MIN_HOLE_SIZE support.");

	hstart = hend = total = 0;
	opposite = (whence == SEEK_DATA) ? SEEK_HOLE : SEEK_DATA;
	for (;;) {
		if ((hstart = lseek(fd, hend, whence)) == -1) {
			ATF_REQUIRE(errno == ENXIO);
			break;
		}
		if ((hend = lseek(fd, hstart, opposite)) == -1) {
			ATF_REQUIRE(errno == ENXIO);
			ATF_REQUIRE((hend = lseek(fd, 0, SEEK_END)) != -1);
		}
		total += hend - hstart;
	}

	ATF_REQUIRE(close(fd) == 0);
	return (total);
}

/* Return the number of bytes used for holes in this sparse file. */
off_t
get_hole_size(const char *path)
{

	return (get_size(path, SEEK_HOLE));
}

/* Return the number of bytes actually allocated in this file. */
off_t
get_allocated_size(const char *path)
{

	return (get_size(path, SEEK_DATA));
}

ATF_TC_WITHOUT_HEAD(default_absolute_file_is_sparse);
ATF_TC_BODY(default_absolute_file_is_sparse, tc)
{
	off_t hole, data, expected;
	const char *filename = "afile";
	const char *const cmd[] = { "truncate", "-s", "5m", filename, NULL };

	run_truncate(cmd);

	expected = 5242880;
	hole = get_hole_size(filename);
	data = get_allocated_size(filename);

	if (hole + data != expected)
		atf_tc_fail("Expected size of %jd, but got %jd + %jd.",
		    expected, hole, data);
	else if (hole <= 0)
		atf_tc_fail("Expected a sparse file, but got %jd of data.",
		    data);
}

ATF_TC_WITHOUT_HEAD(default_relative_file_is_sparse);
ATF_TC_BODY(default_relative_file_is_sparse, tc)
{
	off_t hole, data, expected;
	const char *const before[] = { "truncate", "-s", "1", "afile", NULL };
	const char *const after[] = { "truncate", "-cs", "+5242879", "afile",
	    NULL };

	run_truncate(before);
	run_truncate(after);

	expected = 5242880;
	hole = get_hole_size(before[3]);
	data = get_allocated_size(before[3]);

	if (hole + data != expected)
		atf_tc_fail("Expected size of %jd, but got %jd + %jd.",
		    expected, hole, data);
	else if (hole <= 0)
		atf_tc_fail("Expected a sparse file, but got %jd of data.",
		    data);
}

ATF_TC_WITHOUT_HEAD(allocate_absolute_file);
ATF_TC_BODY(allocate_absolute_file, tc)
{
	off_t hole, data, expected;
	const char *filename = "afile";
	const char *const cmd[] = { "truncate", "-as", "5m", filename, NULL };

	run_truncate(cmd);

	expected = 5242880;
	hole = get_hole_size(filename);
	data = get_allocated_size(filename);

	if (hole != 0 || data != expected)
		atf_tc_fail("Expected size of %jd, but got %jd + %jd.",
		    expected, hole, data);
}

ATF_TC_WITHOUT_HEAD(allocate_relative_file);
ATF_TC_BODY(allocate_relative_file, tc)
{
	off_t hole, data, expected;
	const char *filename = "afile";
	const char *const before[] = { "truncate", "-s1m", filename, NULL };
	const char *const after[] = { "truncate", "-acs+4m", filename, NULL };

	run_truncate(before);
	run_truncate(after);

	expected = 5242880;
	hole = get_hole_size(filename);
	data = get_allocated_size(filename);

	if (hole + data != expected)
		atf_tc_fail("Expected size of %jd, but got %jd + %jd.",
		    expected, hole, data);
	else if (hole <= 0 || data < 4194304)
		atf_tc_fail("got hole=%jd and data=%jd.", hole, data);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, default_absolute_file_is_sparse);
	ATF_TP_ADD_TC(tp, default_relative_file_is_sparse);
	ATF_TP_ADD_TC(tp, allocate_absolute_file);
	ATF_TP_ADD_TC(tp, allocate_relative_file);
	return atf_no_error();
}
