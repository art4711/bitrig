/*
 * Copyright (c) 1995 - 1999 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Tiny program to help debug popper */

#include "popper.h"
RCSID("$KTH: pop_debug.c,v 1.16 1999/12/02 16:58:33 joda Exp $");

static void
loop(int s)
{
    char cmd[1024];
    char buf[1024];
    fd_set fds;
    while(1){
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	FD_SET(s, &fds);
	if(select(s+1, &fds, 0, 0, 0) < 0)
	    err(1, "select");
	if(FD_ISSET(0, &fds)){
	    fgets(cmd, sizeof(cmd), stdin);
	    cmd[strlen(cmd) - 1] = '\0';
	    strlcat (cmd, "\r\n", sizeof(cmd));
	    write(s, cmd, strlen(cmd));
	}
	if(FD_ISSET(s, &fds)){
	    int n = read(s, buf, sizeof(buf));
	    if(n == 0)
		exit(0);
	    fwrite(buf, n, 1, stdout);
	}
    }
}

static int
get_socket (const char *hostname, int port)
{
    struct hostent *hostent = NULL;
    char **h;
    int error;
    int af;

#ifdef HAVE_IPV6    
    if (hostent == NULL)
	hostent = getipnodebyname (hostname, AF_INET6, 0, &error);
#endif
    if (hostent == NULL)
	hostent = getipnodebyname (hostname, AF_INET, 0, &error);

    if (hostent == NULL)
	errx(1, "gethostbyname '%s' failed: %s", hostname, hstrerror(error));

    af = hostent->h_addrtype;

    for (h = hostent->h_addr_list; *h != NULL; ++h) {
	struct sockaddr_storage sa_ss;
	struct sockaddr *sa = (struct sockaddr *)&sa_ss;
	int s;

	sa->sa_family = af;
	socket_set_address_and_port (sa, *h, port);

	s = socket (af, SOCK_STREAM, 0);
	if (s < 0)
	    err (1, "socket");
	if (connect (s, sa, socket_sockaddr_size(sa)) < 0) {
	    warn ("connect(%s)", hostname);
	    close (s);
	    continue;
	}
	freehostent (hostent);
	return s;
    }
    freehostent (hostent);
    exit (1);
}

#ifdef KRB4
static int
doit_v4 (char *host, int port)
{
    KTEXT_ST ticket;
    MSG_DAT msg_data;
    CREDENTIALS cred;
    des_key_schedule sched;
    int ret;
    int s = get_socket (host, port);

    ret = krb_sendauth(0,
		       s,
		       &ticket, 
		       "pop",
		       host,
		       krb_realmofhost(host),
		       getpid(),
		       &msg_data,
		       &cred,
		       sched,
		       NULL,
		       NULL,
		       "KPOPV0.1");
    if(ret) {
	warnx("krb_sendauth: %s", krb_get_err_text(ret));
	return 1;
    }
    loop(s);
    return 0;
}
#endif

#ifdef KRB5
static int
doit_v5 (char *host, int port)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_auth_context auth_context = NULL;
    krb5_principal server;
    int s = get_socket (host, port);

    krb5_init_context (&context);
    
    ret = krb5_sname_to_principal (context,
				   host,
				   "pop",
				   KRB5_NT_SRV_HST,
				   &server);
    if (ret) {
	warnx ("krb5_sname_to_principal: %s",
	       krb5_get_err_text (context, ret));
	return 1;
    }
    ret = krb5_sendauth (context,
			 &auth_context,
			 &s,
			 "KPOPV1.0",
			 NULL,
			 server,
			 0,
			 NULL,
			 NULL,
			 NULL,
			 NULL,
			 NULL,
			 NULL);
     if (ret) {
	 warnx ("krb5_sendauth: %s",
		krb5_get_err_text (context, ret));
	 return 1;
     }
     loop (s);
     return 0;
}
#endif


#ifdef KRB4
static int use_v4 = -1;
#endif
static int use_v5 = -1;
static char *port_str;
static int do_version;
static int do_help;

struct getargs args[] = {
#ifdef KRB4
    { "krb4",	'4', arg_flag,		&use_v4,	"Use Kerberos V4",
      NULL },
#endif
    { "krb5",	'5', arg_flag,		&use_v5,	"Use Kerberos V5",
      NULL },
    { "port",	'p', arg_string,	&port_str,	"Use this port",
      "number-or-service" },
    { "version", 0,  arg_flag,		&do_version,	"Print version",
      NULL },
    { "help",	 0,  arg_flag,		&do_help,	NULL,
      NULL }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args) / sizeof(args[0]),
		    NULL,
		    "hostname");
    exit (ret);
}

int
main(int argc, char **argv)
{
    int port = 0;
    int ret = 1;
    int optind = 0;

    if (getarg (args, sizeof(args) / sizeof(args[0]), argc, argv,
		&optind))
	usage (1);

    argc -= optind;
    argv += optind;

    if (do_help)
	usage (0);

    if (do_version) {
	print_version (NULL);
	return 0;
    }
	
    if (argc < 1)
	usage (1);

    if (port_str) {
	struct servent *s = roken_getservbyname (port_str, "tcp");

	if (s)
	    port = s->s_port;
	else {
	    char *ptr;

	    port = strtol (port_str, &ptr, 10);
	    if (port == 0 && ptr == port_str)
		errx (1, "Bad port `%s'", port_str);
	    port = htons(port);
	}
    }

#if defined(KRB4) && defined(KRB5)
    if(use_v4 == -1 && use_v5 == 1)
	use_v4 = 0;
    if(use_v5 == -1 && use_v4 == 1)
	use_v5 = 0;
#endif    

#ifdef KRB5
    if (ret && use_v5) {
	ret = doit_v5 (argv[0], port);
    }
#endif
#ifdef KRB4
    if (ret && use_v4) {
	ret = doit_v4 (argv[0], port);
    }
#endif
    return ret;
}
