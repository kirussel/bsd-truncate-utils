#
# Copyright 2014, Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#

# Helper function that is always used to create and fill stderr.txt for these
# tests.
_custom_create_file()
{
	# The first argument is a command.
	# The second is just a string.
	case "${1}" in
		creat) ( > stderr.txt ) ;;
		print) [ "${2}" ] && \
		    printf "%s\n" "${2}" >> stderr.txt ;;
	esac
}

# Helper function that create the file stderr.txt that contains the string
# passed in as the first argument.
create_stderr_file()
{
	_custom_create_file creat
	_custom_create_file print "${1}"
}

# Helper function that create the file stderr.txt that contains the expected
# fallocate utility usage message.
create_stderr_usage_file()
{
	_custom_create_file creat
	_custom_create_file print "${1}"
	_custom_create_file print \
	    "usage: fallocate [-o [+|-]offset] -l [+|-]size[K|k|M|m|G|g|T|t|P|p|E|e] file ..."
}

atf_test_case illegal_option
illegal_option_head()
{
	atf_set "descr" "Verifies that fallocate exits >0 when passed an" \
	    "invalid command line option"
}
illegal_option_body()
{
	create_stderr_usage_file 'fallocate: illegal option -- 7'

	# We expect the error message, with no new files.
	atf_check -s not-exit:0 -e file:stderr.txt fallocate -7 -l1 output.txt
	[ ! -e output.txt ] || atf_fail "output.txt should not exist"
}

atf_test_case illegal_size
illegal_size_head()
{
	atf_set "descr" "Verifies that fallocate exits >0 when passed an" \
	    "invalid power of two convention"
}
illegal_size_body()
{
	create_stderr_file "fallocate: invalid length argument \`+1L'"

	# We expect the error message, with no new files.
	atf_check -s not-exit:0 -e file:stderr.txt fallocate -l+1L output.txt
	[ ! -e output.txt ] || atf_fail "output.txt should not exist"
}

atf_test_case too_large_size
too_large_size_head()
{
	atf_set "descr" "Verifies that fallocate exits >0 when passed an" \
	    "a size that is INT64_MAX < size <= UINT64_MAX"
}
too_large_size_body()
{
	create_stderr_file "fallocate: invalid length argument \`8388608t'"

	# We expect the error message, with no new files.
	atf_check -s not-exit:0 -e file:stderr.txt \
	    fallocate -l8388608t output.txt
	[ ! -e output.txt ] || atf_fail "output.txt should not exist"
}

atf_test_case opt_c
opt_c_head()
{
	atf_set "descr" "Verifies that -c prevents creation of new files"
}
opt_c_body()
{
	# No new files and fallocate returns 0 as if this is a success.
	atf_check fallocate -c -l 0 doesnotexist.txt
	[ ! -e output.txt ] || atf_fail "doesnotexist.txt should not exist"

	create_stderr_file

	# The existing file will be altered by fallocate.
	> exists.txt
	atf_check -e file:stderr.txt fallocate -c -l1 exists.txt
	[ -s exists.txt ] || atf_fail "exists.txt be larger than zero bytes"
}

atf_test_case no_files
no_files_head()
{
	atf_set "descr" "Verifies that fallocate needs a list of files on" \
	    "the command line"
}
no_files_body()
{
	create_stderr_usage_file

	# A list of files must be present on the command line.
	atf_check -s not-exit:0 -e file:stderr.txt fallocate -l1
}

atf_test_case bad_fallocate cleanup
bad_fallocate_head()
{
	atf_set "descr" "Verifies that fallocate reports an error during" \
	    "truncation"
}
bad_fallocate_body()
{
	create_stderr_file "fallocate: exists.txt: Operation not permitted"

	# Trying to get the posix_fallocate() call to return -1.
	> exists.txt
        chflags -f simmutable exists.txt
        chflags -f uimmutable exists.txt
        rm -f exists.txt && atf_skip "expected immutable file support"

	atf_check -s not-exit:0 -e file:stderr.txt fallocate -l1 exists.txt
}
bad_fallocate_cleanup()
{
	chflags 0 exists.txt
}

atf_test_case new_absolute_grow
new_absolute_grow_head()
{
	atf_set "descr" "Verifies fallocate can make and grow a new 1m file"
}
new_absolute_grow_body()
{
	create_stderr_file

	# Create a new file and grow it to 1024 bytes.
	atf_check -s exit:0 -e file:stderr.txt fallocate -l1k output.txt
	atf_check -s exit:1 cmp -s output.txt /dev/zero
	eval $(stat -s output.txt)
	[ ${st_size} -eq 1024 ] || atf_fail "expected file size of 1k"

	create_stderr_file

	# Grow the existing file to 1M.  We are using absolute sizes.
	atf_check -s exit:0 -e file:stderr.txt fallocate -c -o512k -l512k output.txt
	atf_check -s exit:1 cmp -s output.txt /dev/zero
	eval $(stat -s output.txt)
	[ ${st_size} -eq 1048576 ] || atf_fail "expected file size of 1m"
}

atf_test_case new_absolute_no_shrink
new_absolute_no_shrink_head()
{
	atf_set "descr" "Verifies that fallocate cannot shrink a new file"
}
new_absolute_no_shrink_body()
{
	create_stderr_file

	# Create a new file and grow it to 1048576 bytes.
	atf_check -s exit:0 -e file:stderr.txt fallocate -o1048575 -l1 output.txt
	atf_check -s exit:1 cmp -s output.txt /dev/zero
	eval $(stat -s output.txt)
	[ ${st_size} -eq 1048576 ] || atf_fail "expected file size of 1m"

	create_stderr_file

	atf_check -s exit:0 -e file:stderr.txt fallocate -l1k output.txt
	atf_check -s exit:1 cmp -s output.txt /dev/zero
	eval $(stat -s output.txt)
	[ ${st_size} -eq 1048576 ] || atf_fail "expected file size of 1m"
}

atf_test_case new_relative_grow
new_relative_grow_head()
{
	atf_set "descr" "Verifies fallocate can make and grow a new 1m file" \
	    "using relative sizes"
}
new_relative_grow_body()
{
	create_stderr_file

	# Create a new file and grow it to 1024 bytes.
	atf_check -s exit:0 -e file:stderr.txt fallocate -l+1k output.txt
	atf_check -s exit:1 cmp -s output.txt /dev/zero
	eval $(stat -s output.txt)
	[ ${st_size} -eq 1024 ] || atf_fail "expected file size of 1k"

	create_stderr_file

	# Grow the existing file to 1M.
	atf_check -s exit:0 -e file:stderr.txt fallocate -o+1k -l1046528 output.txt
	atf_check -s exit:1 cmp -s output.txt /dev/zero
	eval $(stat -s output.txt)
	[ ${st_size} -eq 1048576 ] || atf_fail "expected file size of 1m"
}

atf_test_case new_relative_no_shrink
new_relative_no_shrink_head()
{
	atf_set "descr" "Verifies fallocate cannot shrink a new 1m file"
}
new_relative_no_shrink_body()
{
	create_stderr_file

	# Create a new file and grow it to 1049600 bytes.
	atf_check -s exit:0 -e file:stderr.txt fallocate -o+1049599 -l1 output.txt
	atf_check -s exit:1 cmp -s output.txt /dev/zero
	eval $(stat -s output.txt)
	[ ${st_size} -eq 1049600 ] || atf_fail "expected file size of 1m"

	create_stderr_file

	atf_check -s exit:0 -e file:stderr.txt fallocate -l-1M output.txt
	atf_check -s exit:1 cmp -s output.txt /dev/zero
	eval $(stat -s output.txt)
	[ ${st_size} -eq 1049600 ] || atf_fail "expected file size of 1m"
}

atf_test_case cannot_open
cannot_open_head()
{
	atf_set "descr" "Verifies fallocate handles open failures correctly" \
	    "in a list of files"
	atf_set "require.user" "unprivileged"
}
cannot_open_body()
{
	# Create three files -- the middle file cannot allow writes.
	> before
	> 0000
	> after
	atf_check chmod 0000 0000

	create_stderr_file "fallocate: 0000: Permission denied"

	# Create a new file and grow it to 1024 bytes.
	atf_check -s not-exit:0 -e file:stderr.txt \
	fallocate -c -l1k before 0000 after
	eval $(stat -s before)
	[ ${st_size} -eq 1024 ] || atf_fail "expected file size of 1k"
	eval $(stat -s after)
	[ ${st_size} -eq 1024 ] || atf_fail "expected file size of 1k"
	eval $(stat -s 0000)
	[ ${st_size} -eq 0 ] || atf_fail "expected file size of zero"
}

atf_test_case new_zero
new_zero_head()
{
	atf_set "descr" "Verifies fallocate cannot grow zero byte file"
}
new_zero_body()
{
	create_stderr_file "fallocate: output.txt: Invalid argument"

	# Create a new file but fallocate still fails.
	atf_check -s not-exit:0 -e file:stderr.txt fallocate -l0 output.txt
	eval $(stat -s output.txt)
	[ ${st_size} -eq 0 ] || atf_fail "expected file size of zero"
}

atf_test_case negative
negative_head()
{
	atf_set "descr" "Verifies fallocate treats negative sizes as errors"
}
negative_body()
{
	# Create a 5 byte file.
	printf "abcd\n" > afile
	eval $(stat -s afile)
	[ ${st_size} -eq 5 ] || atf_fail "afile file should be 5 bytes"

	create_stderr_file "fallocate: afile: Invalid argument"

	# Create a new file and do a 100 byte allocate.
	atf_check -s not-exit:0 -e file:stderr.txt fallocate -l-100 afile
	eval $(stat -s afile)
	[ ${st_size} -eq 5 ] || atf_fail "afile file should be 5 bytes"
}

atf_test_case fallocate_removes_sparse
fallocate_removes_sparse_head()
{
	atf_set "descr" "Verifies fallocate moves sparse areas in file"
}
fallocate_removes_sparse_body()
{
	create_stderr_file

	# Create a new file and grow it to 536870912 bytes.
	atf_check -s exit:0 -e file:stderr.txt truncate -s512m output.txt
	eval $(stat -s output.txt)
	[ ${st_size} -eq 536870912 ] || atf_fail "expected file size of 512m"
	[ ${st_blocks} -le 524288 ] || atf_fail "expected less than 524288b"

	# Now force the space to be fully allocated.
	atf_check -s exit:0 -e file:stderr.txt fallocate -l512m output.txt
	eval $(stat -s output.txt)
	[ ${st_size} -eq 536870912 ] || atf_fail "expected file size of 512m"
	[ $((${st_blksize} * ${st_blocks})) -ge 536870912 ] || \
		atf_fail "expected about 512m"
}

atf_init_test_cases()
{
	atf_add_test_case illegal_option
	atf_add_test_case illegal_size
	atf_add_test_case too_large_size
	atf_add_test_case opt_c
	atf_add_test_case no_files
	atf_add_test_case bad_fallocate
	atf_add_test_case cannot_open
	atf_add_test_case new_absolute_grow
	atf_add_test_case new_absolute_no_shrink
	atf_add_test_case new_relative_grow
	atf_add_test_case new_relative_no_shrink
	atf_add_test_case new_zero
	atf_add_test_case negative
	atf_add_test_case fallocate_removes_sparse
}
