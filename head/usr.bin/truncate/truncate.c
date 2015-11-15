/*-
 * Copyright (c) 2000 Sheldon Hearn <sheldonh@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef lint
static const char rcsid[] =
    "$FreeBSD$";
#endif

#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libutil.h>

struct file_offset {
	int	initialized;
	int	relative;
	off_t	sz;
};

static int	offset_init(struct file_offset*, const char*);
static off_t	offset_relative(struct file_offset*, struct stat*);
static void	usage(int);

static int	no_create;
static int	do_allocate;
static int	do_refer;
static struct file_offset	length;
static struct file_offset	offset;

int
main(int argc, char **argv)
{
	struct stat	sb;
	mode_t	omode;
	off_t	tsize, toffset;
	int	ch, error, fd, oflags;
	char   *fname, *rname;
	const char   *optstring;

	/* Are we fallocate(1) or truncate(1)? */
	if (strcmp(basename(argv[0]), "fallocate") == 0) {
		do_allocate = 1;
		optstring = "cl:o:";
	} else {
		do_allocate = 0;
		optstring = "cr:s:";
	}

	fd = -1;
	toffset = tsize = 0;
	error = 0;
	rname = NULL;
	while ((ch = getopt(argc, argv, optstring)) != -1)
		switch (ch) {
		case 'c':
			no_create = 1;
			break;
		case 'l':
			if (offset_init(&length, optarg) != 0)
			    errx(EXIT_FAILURE,
			    "invalid length argument `%s'", optarg);
			break;
		case 'o':
			if (offset_init(&offset, optarg) != 0)
			    errx(EXIT_FAILURE,
			    "invalid offset argument `%s'", optarg);
			break;
		case 'r':
			do_refer = 1;
			rname = optarg;
			break;
		case 's':
			if (offset_init(&length, optarg) != 0)
			    errx(EXIT_FAILURE,
			    "invalid size argument `%s'", optarg);
			break;
		default:
			usage(do_allocate);
			/* NOTREACHED */
		}

	argv += optind;
	argc -= optind;

	/*
	 * Exactly one of do_refer or length.initialized must be specified.
	 * Since length.relative implies length.initialized, length.relative
	 * and do_refer are also mutually exclusive.  See usage() for allowed
	 * invocations.
	 */
	if (do_refer + length.initialized != 1 || argc < 1)
		usage(do_allocate);
	if (do_refer) {
		if (stat(rname, &sb) == -1)
			err(EXIT_FAILURE, "%s", rname);
		tsize = sb.st_size;
	} else
		tsize = length.sz;

	if (do_allocate) {
		if (offset.initialized == 0 &&
		    offset_init(&offset, "0") != 0)
		    err(EXIT_FAILURE, "invalid offset argument `%s'", "0");
		toffset = offset.sz;
	}

	if (no_create)
		oflags = O_WRONLY;
	else
		oflags = O_WRONLY | O_CREAT;
	omode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	while ((fname = *argv++) != NULL) {
		if (fd != -1)
			close(fd);
		if ((fd = open(fname, oflags, omode)) == -1) {
			if (errno != ENOENT) {
				warn("%s", fname);
				error++;
			}
			continue;
		}
		if (length.relative || offset.relative) {
			if (fstat(fd, &sb) == -1) {
				warn("%s", fname);
				error++;
				continue;
			}
			if (length.initialized && length.relative &&
			    (tsize = offset_relative(&length, &sb)) < 0) {
				warn("%s", fname);
				error++;
				continue;
			}
			if (offset.initialized && offset.relative &&
			    (toffset = offset_relative(&offset, &sb)) < 0) {
				warn("%s", fname);
				error++;
				continue;
			}
		}

		if (((do_allocate != 0) ?
		    (errno = posix_fallocate(fd, toffset, tsize)) :
		    ftruncate(fd, tsize)) != 0) {
			warn("%s", fname);
			error++;
			continue;
		}
	}
	if (fd != -1)
		close(fd);

	return error ? EXIT_FAILURE : EXIT_SUCCESS;
}

static off_t
offset_relative(struct file_offset *pfo, struct stat *psb)
{
	off_t oflow;

	if (pfo->initialized == 0 || pfo->relative == 0) {
		errno = EINVAL;
		return -1;
	}

	oflow = psb->st_size + pfo->sz;
	if (oflow < (psb->st_size + pfo->sz)) {
		errno = EFBIG;
		return -1;
	}

	return (oflow < 0 ? 0 : oflow);
}

static int
offset_init(struct file_offset *pfo, const char *number)
{
	uint64_t usz;

	pfo->relative = *number == '+' || *number == '-';
	if (expand_number(pfo->relative ? number + 1 : number,
	    &usz) == -1 || (off_t)usz < 0) {
		errno = EFBIG;
		return -1;
	}

	pfo->sz = (*number == '-') ? -(off_t)usz : (off_t)usz;
	pfo->initialized = 1;

	return 0;
}

static void
usage(int do_allocate)
{
        if (do_allocate)
            fprintf(stderr, "usage: fallocate %s file ...\n",
                "[-o [+|-]offset] -l [+|-]size[K|k|M|m|G|g|T|t|P|p|E|e]");
        else
            fprintf(stderr, "usage: truncate %s\n       truncate %s\n",
                "[-c] -s [+|-]size[K|k|M|m|G|g|T|t|P|p|E|e] file ...",
                "[-c] -r rfile file ...");
	exit(EXIT_FAILURE);
}
