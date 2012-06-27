#	$OpenBSD: Makefile,v 1.10 2010/01/10 03:37:50 guenther Exp $

PROG=	sysctl
MAN=	sysctl.8
SRCS= sysctl.c dyn_sysctl.c

CPPFLAGS+=	-DINET6 -D_LIBKVM

afterinstall:
	ln -sf ../../sbin/sysctl ${DESTDIR}/usr/sbin

.include <bsd.prog.mk>
