/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
 *
 * Quite extensively rewritten by Poul-Henning Kamp of the FreeBSD
 * project, to make these variables more userfriendly.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/sysctl.h>

/*
 * Top level sysctl mib and sysctls that don't belong anywhere specific.
 */
struct sysctl_oid_list sysctl__children;		/* Root list */

SYSCTL_NODE(, 0, sysctl, CTLFLAG_RW, 0, "Sysctl internal magic");
SYSCTL_NODE(, 1, kern, CTLFLAG_RW, 0, "Kernel");

static SYSCTL_STRING(_kern, KERN_OSTYPE, ostype,
    CTLFLAG_RD|CTLFLAG_MPSAFE, (char *)ostype, 0, "Operating system type");
static SYSCTL_STRING(_kern, KERN_OSRELEASE, osrelease,
    CTLFLAG_RD|CTLFLAG_MPSAFE, (char *)osrelease, 0, "Operating system release");
static SYSCTL_INT(_kern, KERN_OSREV, osrevision, CTLFLAG_RD, NULL, OpenBSD,
    "Operating system revision");
static SYSCTL_STRING(_kern, KERN_OSVERSION, osversion,
    CTLFLAG_RD|CTLFLAG_MPSAFE, (char *)osversion, 0, "Operating system version");
static SYSCTL_STRING(_kern, KERN_VERSION, version,
    CTLFLAG_RD|CTLFLAG_MPSAFE, (char *)version, 0, "Kernel version");
static SYSCTL_INT(_kern, KERN_ARGMAX, argmax, CTLFLAG_RD, NULL, ARG_MAX,
    "Max argument size for exec");

char hostname[MAXHOSTNAMELEN];
int hostnamelen;
char domainname[MAXHOSTNAMELEN];
int domainnamelen;

struct hostname_sysctl {
	char *name;
	int *namelen;
	size_t maxlen;
};

int sysctl_hostname(struct sysctl_oid *, void *, __intptr_t,
    struct sysctl_req *);

int
sysctl_hostname(struct sysctl_oid *oidp, void *arg1, __intptr_t arg2,
    struct sysctl_req *req)
{
	struct hostname_sysctl *h = arg1;
	struct sysctl_oid tmpoid = {
		NULL, { NULL }, OID_AUTO,
		CTLTYPE_STRING|CTLFLAG_RW|CTLFLAG_TSTR,
		h->name, h->maxlen,
		NULL,
		sysctl_handle_string,
		NULL, NULL
	};
	int error;

	error = sysctl_handle_string(&tmpoid, h->name, h->maxlen, req);
	*h->namelen = strlen(h->name);
	return error;
}

static struct hostname_sysctl hostname_sysctl = {
	hostname, &hostnamelen, sizeof(hostname)
};
static SYSCTL_PROC(_kern, KERN_HOSTNAME, hostname, CTLTYPE_STRING|CTLFLAG_RW,
    &hostname_sysctl, 0, sysctl_hostname, "A", "Hostname");
static struct hostname_sysctl domainname_sysctl = {
	domainname, &domainnamelen, sizeof(domainname)
};
static SYSCTL_PROC(_kern, KERN_DOMAINNAME, domainname,
    CTLTYPE_STRING|CTLFLAG_RW,
    &domainname_sysctl, 0, sysctl_hostname, "A", "Domain name");

long hostid;
static SYSCTL_INT(_kern, KERN_HOSTID, hostid, CTLFLAG_RW, &hostid, 0,
    "Host ID");
