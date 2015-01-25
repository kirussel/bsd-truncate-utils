# $FreeBSD$

TESTSDIR=	${TESTSBASE}/usr.bin/truncate
ATF_TESTS_SH=	allocate_test truncate_test 
ATF_TESTS_C=	sparse_test
CLEANFILES+=	allocate_test.sh truncate_test.sh

allocate_test.sh: base_test.func allocate_test.func
	cat ${.ALLSRC} > ${.TARGET}

truncate_test.sh: base_test.func truncate_test.func
	cat ${.ALLSRC} > ${.TARGET}

.include <bsd.test.mk>
