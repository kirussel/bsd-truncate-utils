# $FreeBSD$

.include <src.opts.mk>

PROG=	truncate
LIBADD=	util
LINKS=	${BINDIR}/truncate ${BINDIR}/fallocate

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
