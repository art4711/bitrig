/*	$OpenBSD: inet.c,v 1.11 1997/02/26 03:08:43 angelos Exp $	*/
/*	$NetBSD: inet.c,v 1.14 1995/10/03 21:42:37 thorpej Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)inet.c	8.4 (Berkeley) 4/20/94";
#else
static char *rcsid = "$OpenBSD: inet.c,v 1.11 1997/02/26 03:08:43 angelos Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_debug.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ip4.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>

struct	inpcb inpcb;
struct	tcpcb tcpcb;
struct	socket sockb;

char	*inetname __P((struct in_addr *));
void	inetprint __P((struct in_addr *, int, char *, int));

/*
 * Print a summary of connections related to an Internet
 * protocol.  For TCP, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */
void
protopr(off, name)
	u_long off;
	char *name;
{
	struct inpcbtable table;
	register struct inpcb *head, *next, *prev;
	struct inpcb inpcb;
	int istcp;
	static int first = 1;

	if (off == 0)
		return;
	istcp = strcmp(name, "tcp") == 0;
	kread(off, (char *)&table, sizeof table);
	prev = head =
	    (struct inpcb *)&((struct inpcbtable *)off)->inpt_queue.cqh_first;
	next = table.inpt_queue.cqh_first;

	while (next != head) {
		kread((u_long)next, (char *)&inpcb, sizeof inpcb);
		if (inpcb.inp_queue.cqe_prev != prev) {
			printf("???\n");
			break;
		}
		prev = next;
		next = inpcb.inp_queue.cqe_next;

		if (!aflag &&
		    inet_lnaof(inpcb.inp_laddr) == INADDR_ANY)
			continue;
		kread((u_long)inpcb.inp_socket, (char *)&sockb, sizeof (sockb));
		if (istcp) {
			kread((u_long)inpcb.inp_ppcb,
			    (char *)&tcpcb, sizeof (tcpcb));
		}
		if (first) {
			printf("Active Internet connections");
			if (aflag)
				printf(" (including servers)");
			putchar('\n');
			if (Aflag)
				printf("%-8.8s ", "PCB");
			printf(Aflag ?
				"%-5.5s %-6.6s %-6.6s  %-18.18s %-18.18s %s\n" :
				"%-5.5s %-6.6s %-6.6s  %-22.22s %-22.22s %s\n",
				"Proto", "Recv-Q", "Send-Q",
				"Local Address", "Foreign Address", "(state)");
			first = 0;
		}
		if (Aflag)
			if (istcp)
				printf("%8x ", inpcb.inp_ppcb);
			else
				printf("%8x ", prev);
		printf("%-5.5s %6d %6d ", name, sockb.so_rcv.sb_cc,
			sockb.so_snd.sb_cc);
		inetprint(&inpcb.inp_laddr, (int)inpcb.inp_lport, name, 1);
		inetprint(&inpcb.inp_faddr, (int)inpcb.inp_fport, name, 0);
		if (istcp) {
			if (tcpcb.t_state < 0 || tcpcb.t_state >= TCP_NSTATES)
				printf(" %d", tcpcb.t_state);
			else
				printf(" %s", tcpstates[tcpcb.t_state]);
		}
		putchar('\n');
	}
}

/*
 * Dump TCP statistics structure.
 */
void
tcp_stats(off, name)
	u_long off;
	char *name;
{
	struct tcpstat tcpstat;

	if (off == 0)
		return;
	printf ("%s:\n", name);
	kread(off, (char *)&tcpstat, sizeof (tcpstat));

#define	p(f, m) if (tcpstat.f || sflag <= 1) \
    printf(m, tcpstat.f, plural(tcpstat.f))
#define	p2(f1, f2, m) if (tcpstat.f1 || tcpstat.f2 || sflag <= 1) \
    printf(m, tcpstat.f1, plural(tcpstat.f1), tcpstat.f2, plural(tcpstat.f2))
#define	p3(f, m) if (tcpstat.f || sflag <= 1) \
    printf(m, tcpstat.f, plurales(tcpstat.f))

	p(tcps_sndtotal, "\t%d packet%s sent\n");
	p2(tcps_sndpack,tcps_sndbyte,
		"\t\t%d data packet%s (%d byte%s)\n");
	p2(tcps_sndrexmitpack, tcps_sndrexmitbyte,
		"\t\t%d data packet%s (%d byte%s) retransmitted\n");
	p2(tcps_sndacks, tcps_delack,
		"\t\t%d ack-only packet%s (%d delayed)\n");
	p(tcps_sndurg, "\t\t%d URG only packet%s\n");
	p(tcps_sndprobe, "\t\t%d window probe packet%s\n");
	p(tcps_sndwinup, "\t\t%d window update packet%s\n");
	p(tcps_sndctrl, "\t\t%d control packet%s\n");
	p(tcps_rcvtotal, "\t%d packet%s received\n");
	p2(tcps_rcvackpack, tcps_rcvackbyte, "\t\t%d ack%s (for %d byte%s)\n");
	p(tcps_rcvdupack, "\t\t%d duplicate ack%s\n");
	p(tcps_rcvacktoomuch, "\t\t%d ack%s for unsent data\n");
	p2(tcps_rcvpack, tcps_rcvbyte,
		"\t\t%d packet%s (%d byte%s) received in-sequence\n");
	p2(tcps_rcvduppack, tcps_rcvdupbyte,
		"\t\t%d completely duplicate packet%s (%d byte%s)\n");
	p(tcps_pawsdrop, "\t\t%d old duplicate packet%s\n");
	p2(tcps_rcvpartduppack, tcps_rcvpartdupbyte,
		"\t\t%d packet%s with some dup. data (%d byte%s duped)\n");
	p2(tcps_rcvoopack, tcps_rcvoobyte,
		"\t\t%d out-of-order packet%s (%d byte%s)\n");
	p2(tcps_rcvpackafterwin, tcps_rcvbyteafterwin,
		"\t\t%d packet%s (%d byte%s) of data after window\n");
	p(tcps_rcvwinprobe, "\t\t%d window probe%s\n");
	p(tcps_rcvwinupd, "\t\t%d window update packet%s\n");
	p(tcps_rcvafterclose, "\t\t%d packet%s received after close\n");
	p(tcps_rcvbadsum, "\t\t%d discarded for bad checksum%s\n");
	p(tcps_rcvbadoff, "\t\t%d discarded for bad header offset field%s\n");
	p(tcps_rcvshort, "\t\t%d discarded because packet too short\n");
	p(tcps_connattempt, "\t%d connection request%s\n");
	p(tcps_accepts, "\t%d connection accept%s\n");
	p(tcps_connects, "\t%d connection%s established (including accepts)\n");
	p2(tcps_closed, tcps_drops,
		"\t%d connection%s closed (including %d drop%s)\n");
	p(tcps_conndrops, "\t%d embryonic connection%s dropped\n");
	p2(tcps_rttupdated, tcps_segstimed,
		"\t%d segment%s updated rtt (of %d attempt%s)\n");
	p(tcps_rexmttimeo, "\t%d retransmit timeout%s\n");
	p(tcps_timeoutdrop, "\t\t%d connection%s dropped by rexmit timeout\n");
	p(tcps_persisttimeo, "\t%d persist timeout%s\n");
	p(tcps_keeptimeo, "\t%d keepalive timeout%s\n");
	p(tcps_keepprobe, "\t\t%d keepalive probe%s sent\n");
	p(tcps_keepdrops, "\t\t%d connection%s dropped by keepalive\n");
	p(tcps_predack, "\t%d correct ACK header prediction%s\n");
	p(tcps_preddat, "\t%d correct data packet header prediction%s\n");
	p3(tcps_pcbhashmiss, "\t%d PCB cache miss%s\n");
#undef p
#undef p2
#undef p3
}

/*
 * Dump UDP statistics structure.
 */
void
udp_stats(off, name)
	u_long off;
	char *name;
{
	struct udpstat udpstat;
	u_long delivered;

	if (off == 0)
		return;
	kread(off, (char *)&udpstat, sizeof (udpstat));
	printf("%s:\n", name);
#define	p(f, m) if (udpstat.f || sflag <= 1) \
    printf(m, udpstat.f, plural(udpstat.f))
	p(udps_ipackets, "\t%u datagram%s received\n");
	p(udps_hdrops, "\t%u with incomplete header\n");
	p(udps_badlen, "\t%u with bad data length field\n");
	p(udps_badsum, "\t%u with bad checksum\n");
	p(udps_noport, "\t%u dropped due to no socket\n");
	p(udps_noportbcast, "\t%u broadcast/multicast datagram%s dropped due to no socket\n");
	p(udps_fullsock, "\t%u dropped due to full socket buffers\n");
	delivered = udpstat.udps_ipackets -
		    udpstat.udps_hdrops -
		    udpstat.udps_badlen -
		    udpstat.udps_badsum -
		    udpstat.udps_noport -
		    udpstat.udps_noportbcast -
		    udpstat.udps_fullsock;
	if (delivered || sflag <= 1)
		printf("\t%u delivered\n", delivered);
	p(udps_opackets, "\t%u datagram%s output\n");
#undef p
}

/*
 * Dump IP statistics structure.
 */
void
ip_stats(off, name)
	u_long off;
	char *name;
{
	struct ipstat ipstat;

	if (off == 0)
		return;
	kread(off, (char *)&ipstat, sizeof (ipstat));
	printf("%s:\n", name);

#define	p(f, m) if (ipstat.f || sflag <= 1) \
    printf(m, ipstat.f, plural(ipstat.f))

	p(ips_total, "\t%u total packet%s received\n");
	p(ips_badsum, "\t%u bad header checksum%s\n");
	p(ips_toosmall, "\t%u with size smaller than minimum\n");
	p(ips_tooshort, "\t%u with data size < data length\n");
	p(ips_badhlen, "\t%u with header length < data size\n");
	p(ips_badlen, "\t%u with data length < header length\n");
	p(ips_badoptions, "\t%u with bad options\n");
	p(ips_badvers, "\t%u with incorrect version number\n");
	p(ips_fragments, "\t%u fragment%s received\n");
	p(ips_fragdropped, "\t%u fragment%s dropped (dup or out of space)\n");
	p(ips_badfrags, "\t%u malformed fragment%s dropped\n");
	p(ips_fragtimeout, "\t%u fragment%s dropped after timeout\n");
	p(ips_reassembled, "\t%u packet%s reassembled ok\n");
	p(ips_delivered, "\t%u packet%s for this host\n");
	p(ips_noproto, "\t%u packet%s for unknown/unsupported protocol\n");
	p(ips_forward, "\t%u packet%s forwarded\n");
	p(ips_cantforward, "\t%u packet%s not forwardable\n");
	p(ips_redirectsent, "\t%u redirect%s sent\n");
	p(ips_localout, "\t%u packet%s sent from this host\n");
	p(ips_rawout, "\t%u packet%s sent with fabricated ip header\n");
	p(ips_odropped, "\t%u output packet%s dropped due to no bufs, etc.\n");
	p(ips_noroute, "\t%u output packet%s discarded due to no route\n");
	p(ips_fragmented, "\t%u output datagram%s fragmented\n");
	p(ips_ofragments, "\t%u fragment%s created\n");
	p(ips_cantfrag, "\t%u datagram%s that can't be fragmented\n");
#undef p
}

static	char *icmpnames[] = {
	"echo reply",
	"#1",
	"#2",
	"destination unreachable",
	"source quench",
	"routing redirect",
	"#6",
	"#7",
	"echo",
	"router advertisement",
	"router solicitation",
	"time exceeded",
	"parameter problem",
	"time stamp",
	"time stamp reply",
	"information request",
	"information request reply",
	"address mask request",
	"address mask reply",
};

/*
 * Dump ICMP statistics.
 */
void
icmp_stats(off, name)
	u_long off;
	char *name;
{
	struct icmpstat icmpstat;
	register int i, first;

	if (off == 0)
		return;
	kread(off, (char *)&icmpstat, sizeof (icmpstat));
	printf("%s:\n", name);

#define	p(f, m) if (icmpstat.f || sflag <= 1) \
    printf(m, icmpstat.f, plural(icmpstat.f))

	p(icps_error, "\t%u call%s to icmp_error\n");
	p(icps_oldicmp,
	    "\t%u error%s not generated 'cuz old message was icmp\n");
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat.icps_outhist[i] != 0) {
			if (first) {
				printf("\tOutput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %u\n", icmpnames[i],
				icmpstat.icps_outhist[i]);
		}
	p(icps_badcode, "\t%u message%s with bad code fields\n");
	p(icps_tooshort, "\t%u message%s < minimum length\n");
	p(icps_checksum, "\t%u bad checksum%s\n");
	p(icps_badlen, "\t%u message%s with bad length\n");
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat.icps_inhist[i] != 0) {
			if (first) {
				printf("\tInput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %u\n", icmpnames[i],
				icmpstat.icps_inhist[i]);
		}
	p(icps_reflect, "\t%u message response%s generated\n");
#undef p
}

/*
 * Dump IGMP statistics structure.
 */
void
igmp_stats(off, name)
	u_long off;
	char *name;
{
	struct igmpstat igmpstat;

	if (off == 0)
		return;
	kread(off, (char *)&igmpstat, sizeof (igmpstat));
	printf("%s:\n", name);

#define	p(f, m) if (igmpstat.f || sflag <= 1) \
    printf(m, igmpstat.f, plural(igmpstat.f))
#define	py(f, m) if (igmpstat.f || sflag <= 1) \
    printf(m, igmpstat.f, igmpstat.f != 1 ? "ies" : "y")
	p(igps_rcv_total, "\t%u message%s received\n");
        p(igps_rcv_tooshort, "\t%u message%s received with too few bytes\n");
        p(igps_rcv_badsum, "\t%u message%s received with bad checksum\n");
        py(igps_rcv_queries, "\t%u membership quer%s received\n");
        py(igps_rcv_badqueries, "\t%u membership quer%s received with invalid field(s)\n");
        p(igps_rcv_reports, "\t%u membership report%s received\n");
        p(igps_rcv_badreports, "\t%u membership report%s received with invalid field(s)\n");
        p(igps_rcv_ourreports, "\t%u membership report%s received for groups to which we belong\n");
        p(igps_snd_reports, "\t%u membership report%s sent\n");
#undef p
#undef py
}

struct rpcnams {
	struct rpcnams *next;
	int	port;
	char	*rpcname;
};

char *
getrpcportnam(port)
	int port;
{
	struct sockaddr_in server_addr;
	register struct hostent *hp;
	static struct pmaplist *head;
	int socket = RPC_ANYSOCK;
	struct timeval minutetimeout;
	register CLIENT *client;
	struct rpcent *rpc;
	static int first;
	static struct rpcnams *rpcn;
	struct rpcnams *n;
	char num[10];
	
	if (first == 0) {
		first = 1;
		memset((char *)&server_addr, 0, sizeof server_addr);
		server_addr.sin_family = AF_INET;
		if ((hp = gethostbyname("localhost")) != NULL)
			memmove((caddr_t)&server_addr.sin_addr, hp->h_addr,
			    hp->h_length);
		else
			(void) inet_aton("0.0.0.0", &server_addr.sin_addr);

		minutetimeout.tv_sec = 60;
		minutetimeout.tv_usec = 0;
		server_addr.sin_port = htons(PMAPPORT);
		if ((client = clnttcp_create(&server_addr, PMAPPROG,
		    PMAPVERS, &socket, 50, 500)) == NULL)
			return (NULL);
		if (clnt_call(client, PMAPPROC_DUMP, xdr_void, NULL,
		    xdr_pmaplist, &head, minutetimeout) != RPC_SUCCESS) {
			clnt_destroy(client);
			return (NULL);
		}
		for (; head != NULL; head = head->pml_next) {
			n = (struct rpcnams *)malloc(sizeof(struct rpcnams));
			if (n == NULL)
				continue;
			n->next = rpcn;
			rpcn = n;
			n->port = head->pml_map.pm_port;

			rpc = getrpcbynumber(head->pml_map.pm_prog);
			if (rpc)
				n->rpcname = strdup(rpc->r_name);
			else {
				sprintf(num, "%d", head->pml_map.pm_prog);
				n->rpcname = strdup(num);
			}
		}
		clnt_destroy(client);
	}

	for (n = rpcn; n; n = n->next)
		if (n->port == port)
			return (n->rpcname);
	return (NULL);
}

/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */
void
inetprint(in, port, proto, local)
	register struct in_addr *in;
	int port;
	char *proto;
	int local;
{
	struct servent *sp = 0;
	char line[80], *cp, *nam;
	int width;

	sprintf(line, "%.*s.", (Aflag && !nflag) ? 12 : 16, inetname(in));
	cp = strchr(line, '\0');
	if (!nflag && port)
		sp = getservbyport((int)port, proto);
	if (sp || port == 0)
		sprintf(cp, "%.8s", sp ? sp->s_name : "*");
	else if (local && !nflag && (nam = getrpcportnam(ntohs((u_short)port))))
		sprintf(cp, "%d[%.8s]", ntohs((u_short)port), nam);
	else
		sprintf(cp, "%d", ntohs((u_short)port));
	width = Aflag ? 18 : 22;
	printf(" %-*.*s", width, width, line);
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */
char *
inetname(inp)
	struct in_addr *inp;
{
	register char *cp;
	static char line[50];
	struct hostent *hp;
	struct netent *np;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = strchr(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}
	cp = 0;
	if (!nflag && inp->s_addr != INADDR_ANY) {
		int net = inet_netof(*inp);
		int lna = inet_lnaof(*inp);

		if (lna == INADDR_ANY) {
			np = getnetbyaddr(net, AF_INET);
			if (np)
				cp = np->n_name;
		}
		if (cp == 0) {
			hp = gethostbyaddr((char *)inp, sizeof (*inp), AF_INET);
			if (hp) {
				if ((cp = strchr(hp->h_name, '.')) &&
				    !strcmp(cp + 1, domain))
					*cp = 0;
				cp = hp->h_name;
			}
		}
	}
	if (inp->s_addr == INADDR_ANY)
		strcpy(line, "*");
	else if (cp)
		strcpy(line, cp);
	else {
		inp->s_addr = ntohl(inp->s_addr);
#define C(x)	((x) & 0xff)
		sprintf(line, "%u.%u.%u.%u", C(inp->s_addr >> 24),
		    C(inp->s_addr >> 16), C(inp->s_addr >> 8), C(inp->s_addr));
	}
	return (line);
}

/*
 * Dump AH statistics structure.
 */
void
ah_stats(off, name)
        u_long off;
        char *name;
{
        struct ahstat ahstat;

        if (off == 0)
                return;
        kread(off, (char *)&ahstat, sizeof (ahstat));
        printf("%s:\n", name);

#define p(f, m) if (ahstat.f || sflag <= 1) \
    printf(m, ahstat.f, plural(ahstat.f))

	p(ahs_input, "\t%u input AH packets\n");
	p(ahs_output, "\t%u output AH packets\n");
        p(ahs_hdrops, "\t%u packet%s shorter than header shows\n");
        p(ahs_notdb, "\t%u packet%s for which no TDB was found\n");
        p(ahs_badkcr, "\t%u input packet%s that failed to be processed\n");
        p(ahs_badauth, "\t%u packet%s that failed verification received\n");
        p(ahs_noxform, "\t%u packet%s for which no XFORM was set in TDB received\n");
        p(ahs_qfull, "\t%u packet%s were dropeed due to full output queue\n");
        p(ahs_wrap, "\t%u packet%s where counter wrapping was detected\n");
        p(ahs_replay, "\t%u possibly replayed packet%s received\n");
        p(ahs_badauthl, "\t%u packet%s with bad authenticator length received\n");
#undef p
}

/*
 * Dump ESP statistics structure.
 */
void
esp_stats(off, name)
        u_long off;
        char *name;
{
        struct espstat espstat;

        if (off == 0)
                return;
        kread(off, (char *)&espstat, sizeof (espstat));
        printf("%s:\n", name);

#define p(f, m) if (espstat.f || sflag <= 1) \
    printf(m, espstat.f, plural(espstat.f))

        p(esps_hdrops, "\t%u packet%s shorter than header shows\n");
        p(esps_notdb, "\t%u packet%s for which no TDB was found\n");
        p(esps_badkcr, "\t%u input packet%s that failed to be processed\n");
        p(esps_badauth, "\t%u packet%s that failed verification received\n");
        p(esps_noxform, "\t%u packet%s for which no XFORM was set in TDB received\n");   
        p(esps_qfull, "\t%u packet%s were dropeed due to full output queue\n");
        p(esps_wrap, "\t%u packet%s where counter wrapping was detected\n");
        p(esps_replay, "\t%u possibly replayed packet%s received\n"); 
        p(esps_badilen, "\t%u packet%s with payload not a multiple of 8 received\n");

#undef p
}

/*
 * Dump ESP statistics structure.
 */
void
ip4_stats(off, name)
        u_long off;
        char *name;
{
        struct ip4stat ip4stat;

        if (off == 0)
                return;
        kread(off, (char *)&ip4stat, sizeof (ip4stat));
        printf("%s:\n", name);

#define p(f, m) if (ip4stat.f || sflag <= 1) \
    printf(m, ip4stat.f, plural(ip4stat.f))

        p(ip4s_ipackets, "\t%u total input packet%s\n");
        p(ip4s_opackets, "\t%u total output packet%s\n");
        p(ip4s_hdrops, "\t%u packet%s shorter than header shows\n");
        p(ip4s_notip4, "\t%u packet%s with internal header not IPv4 received\n");
        p(ip4s_qfull, "\t%u packet%s were dropeed due to full output queue\n");

#undef p
}
