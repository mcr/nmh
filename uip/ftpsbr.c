
/*
 * ftpsbr.c -- simple FTP client library
 *
 * $Id$
 */

#include <h/mh.h>
#include <h/mime.h>

#ifdef HAVE_ARPA_FTP_H
# include <arpa/ftp.h>
#endif

#define	v_debug	debugsw
#define	v_verbose verbosw

static int ftp_fd = NOTOK;
static int data_fd = NOTOK;

static int v_noise;

extern int v_debug;
extern int v_verbose;

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#if defined(BIND) && !defined(h_addr)
# define h_addr	h_addr_list[0]
#endif

#define	inaddr_copy(hp,sin) \
    memcpy((char *) &((sin)->sin_addr), (hp)->h_addr, (hp)->h_length)

#define	start_tcp_client(sock,priv) \
    	socket (AF_INET, SOCK_STREAM, 0)

#define	join_tcp_server(fd, sock) \
    	connect ((fd), (struct sockaddr *) (sock), sizeof *(sock))

/*
 * prototypes
 */
struct hostent *gethostbystring ();
int ftp_get (char *, char *, char *, char *, char *, char *, int, int);
int ftp_trans (char *, char *, char *, char *, char *, char *, char *, int, int);

/*
 * static prototypes
 */
static int start_tcp_server (struct sockaddr_in *, int, int, int);
static void _asnprintf (char *, int, char *, va_list);
static int ftp_quit (void);
static int ftp_read (char *, char *, char *, int);
static int initconn (void);
static int dataconn (void);
static int command (int arg1, ...);
static int vcommand (int, va_list);
static int getreply (int, int);


static int
start_tcp_server (struct sockaddr_in *sock, int backlog, int opt1, int opt2)
{
    int eindex, sd;

    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == NOTOK)
	return NOTOK;

    if (bind (sd, (struct sockaddr *) sock, sizeof *sock) == NOTOK) {
	eindex = errno;
	close (sd);
	errno = eindex;
    } else {
	listen (sd, backlog);
    }

    return sd;
}


static int __len__;

#define	join_tcp_client(fd,sock) \
    	accept ((fd), (struct sockaddr *) (sock), \
		(__len__ = sizeof *(sock), &__len__))

#define	read_tcp_socket	 read
#define	write_tcp_socket write
#define	close_tcp_socket close

static void
_asnprintf (char *bp, int len_bp, char *what, va_list ap)
{
    int eindex, len;
    char *fmt;

    eindex = errno;

    *bp = '\0';
    fmt = va_arg (ap, char *);

    if (fmt) {
	vsnprintf(bp, len_bp, fmt, ap);
	len = strlen(bp);
	bp += len;
	len_bp -= len;
    }

    if (what) {
	char *s;

	if (*what) {
	    snprintf (bp, len_bp, " %s: ", what);
	    len = strlen (bp);
	    bp += len;
	    len_bp -= len;
	}
	if ((s = strerror(eindex)))
	    strncpy (bp, s, len_bp);
	else
	    snprintf (bp, len_bp, "Error %d", eindex);
	bp += strlen (bp);
    }

    errno = eindex;
}


int
ftp_get (char *host, char *user, char *password, char *cwd,
         char *remote, char *local, int ascii, int stayopen)
{
    return ftp_trans (host, user, password, cwd, remote, local,
                      "RETR", ascii, stayopen);
}


int
ftp_trans (char *host, char *user, char *password, char *cwd, char *remote,
           char *local, char *cmd, int ascii, int stayopen)
{
    int	result;

    if (stayopen <= 0) {
	result = ftp_quit ();
	if (host == NULL)
	    return result;
    }

    if (ftp_fd == NOTOK) {
	struct sockaddr_in in_socket;
	register struct hostent *hp;
	register struct servent *sp;

	if ((sp = getservbyname ("ftp", "tcp")) == NULL) {
	    fprintf (stderr, "tcp/ftp: unknown service");
	    return NOTOK;
	}
	if ((hp = gethostbystring (host)) == NULL) {
	    fprintf (stderr, "%s: unknown host\n", host);
	    return NOTOK;
	}
	in_socket.sin_family = hp->h_addrtype;
	inaddr_copy (hp, &in_socket);
	in_socket.sin_port = sp->s_port;

	if ((ftp_fd = start_tcp_client ((struct sockaddr_in *) NULL, 0))
	        == NOTOK) {
	    perror (host);
	    return NOTOK;
	}
	if (join_tcp_server (ftp_fd, &in_socket) == NOTOK) {
	    perror (host);
	    close_tcp_socket (ftp_fd), ftp_fd = NOTOK;
	    return NOTOK;
	}
	getreply (1, 0);

	if (v_verbose) {
	    fprintf (stdout, "Connected to %s\n", host);
	    fflush (stdout);
	}

	if (user) {
	    if ((result = command (0, "USER %s", user)) == CONTINUE)
		result = command (1, "PASS %s", password);
	    if (result != COMPLETE) {
		result = NOTOK;
		goto out;
	    }
	}

	if (remote == NULL)
	    return OK;
    }

    if (cwd && ((result = command (0, "CWD %s", cwd)) != COMPLETE
		    && result != CONTINUE)) {
	result = NOTOK;
	goto out;
    }

    if (command (1, ascii ? "TYPE A" : "TYPE I") != COMPLETE) {
	result = NOTOK;
	goto out;
    }

    result = ftp_read (remote, local, cmd, ascii);

out: ;
    if (result != OK || !stayopen)
	ftp_quit ();

    return result;
}


static int
ftp_quit (void)
{
    int	n;

    if (ftp_fd == NOTOK)
	return OK;

    n = command (1, "QUIT");
    close_tcp_socket (ftp_fd), ftp_fd = NOTOK;
    return (n == 0 || n == COMPLETE ? OK : NOTOK);
}

static int
ftp_read (char *remote, char *local, char *cmd, int ascii)
{
    int	istdio = 0, istore;
    register int cc;
    int	expectingreply = 0;
    char buffer[BUFSIZ];
    FILE *fp = NULL;

    if (initconn () == NOTOK)
	goto bad;

    v_noise = v_verbose;
    if (command (-1, *remote ? "%s %s" : "%s", cmd, remote) != PRELIM)
	goto bad;

    expectingreply++;
    if (dataconn () == NOTOK) {
bad: ;
        if (fp && !istdio)
	    fclose (fp);
	if (data_fd != NOTOK)
	    close_tcp_socket (data_fd), data_fd = NOTOK;
	if (expectingreply)
	    getreply (-2, 0);

	return NOTOK;
    }

    istore = !strcmp (cmd, "STOR");

    if ((istdio = !strcmp (local, "-")))
	fp = istore ? stdin : stdout;
    else
	if ((fp = fopen (local, istore ? "r" : "w")) == NULL) {
	    perror (local);
	    goto bad;
	}

    if (istore) {
	if (ascii) {
	    int	c;
	    FILE *out;

	    if (!(out = fdopen (data_fd, "w"))) {
		perror ("fdopen");
		goto bad;
	    }

	    while ((c = getc (fp)) != EOF) {
		if (c == '\n')
		    putc ('\r', out);
		if (putc (c, out) == EOF) {
		    perror ("putc");
		    fclose (out);
		    data_fd = NOTOK;
		    goto bad;
		}
	    }

	    fclose (out);
	    data_fd = NOTOK;
	}
	else {
	    while ((cc = fread (buffer, sizeof *buffer, sizeof buffer, fp)) > 0)
		if (write_tcp_socket (data_fd, buffer, cc) != cc) {
		    perror ("write_tcp_socket");
		    goto bad;
		}

	    close_tcp_socket (data_fd), data_fd = NOTOK;
	}
    }
    else {
	if (ascii) {
	    int	c;
	    FILE *in;

	    if (!(in = fdopen (data_fd, "r"))) {
		perror ("fdopen");
		goto bad;
	    }

	    while ((c = getc (in)) != EOF) {
		if (c == '\r')
		    switch (c = getc (in)) {
		        case EOF:
		        case '\0':
		            c = '\r';
			    break;

			case '\n':
			    break;

			default:
			    putc ('\r', fp);
			    break;
			}

		if (putc (c, fp) == EOF) {
		    perror ("putc");
		    fclose (in);
		    data_fd = NOTOK;
		    goto bad;
		}
	    }

	    fclose (in);
	    data_fd = NOTOK;
	}
	else {
	    while ((cc = read_tcp_socket (data_fd, buffer, sizeof buffer)) > 0)
		if (fwrite (buffer, sizeof *buffer, cc, fp) == 0) {
		    perror ("fwrite");
		    goto bad;
		}
	    if (cc < 0) {
		perror ("read_tcp_socket");
		goto bad;
	    }

	    close_tcp_socket (data_fd), data_fd = NOTOK;
	}
    }

    if (!istdio)
	fclose (fp);

    v_noise = v_verbose;
    return (getreply (1, 0) == COMPLETE ? OK : NOTOK);
}


#define	UC(b) (((int) b) & 0xff)

static int
initconn (void)
{
    int	len;
    register char *a, *p;
    struct sockaddr_in in_socket;

    if (getsockname (ftp_fd, (struct sockaddr *) &in_socket,
		     (len = sizeof(in_socket), &len)) == NOTOK) {
	perror ("getsockname");
	return NOTOK;
    }
    in_socket.sin_port = 0;
    if ((data_fd = start_tcp_server (&in_socket, 1, 0, 0)) == NOTOK) {
	perror ("start_tcp_server");
	return NOTOK;
    }

    if (getsockname (data_fd, (struct sockaddr *) &in_socket,
		     (len = sizeof in_socket, &len)) == NOTOK) {
	perror ("getsockname");
	return NOTOK;
    }

    a = (char *) &in_socket.sin_addr;
    p = (char *) &in_socket.sin_port;

    if (command (1, "PORT %d,%d,%d,%d,%d,%d",
		      UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
		      UC(p[0]), UC(p[1])) == COMPLETE)
	return OK;

    return NOTOK;
}

static int
dataconn (void)
{
    int	fd;
    struct sockaddr_in in_socket;
    
    if ((fd = join_tcp_client (data_fd, &in_socket)) == NOTOK) {
	perror ("join_tcp_client");
	return NOTOK;
    }
    close_tcp_socket (data_fd);
    data_fd = fd;

    return OK;
}


static int
command (int arg1, ...)
{
    int	val;
    va_list ap;

    va_start (ap, arg1);
    val = vcommand (arg1, ap);
    va_end (ap);

    return val;
}

static int
vcommand (int complete, va_list ap)
{
    int	len;
    char buffer[BUFSIZ];

    if (ftp_fd == NOTOK)
	return NOTOK;

    _asnprintf (buffer, sizeof(buffer), NULL, ap);
    if (v_debug)
	fprintf (stderr, "<--- %s\n", buffer);

    strcat (buffer, "\r\n");
    len = strlen (buffer);

    if (write_tcp_socket (ftp_fd, buffer, len) != len) {
	perror ("write_tcp_socket");
	return NOTOK;
    }

    return (getreply (complete, !strcmp (buffer, "QUIT")));
}


static int
getreply (int complete, int expecteof)
{
    for (;;) {
	register int code, dig, n;
	int continuation;
	register char *bp;
	char buffer[BUFSIZ];

	code = dig = n = continuation = 0;
	bp = buffer;

	for (;;) {
	    char c;

	    if (read_tcp_socket (ftp_fd, &c, 1) < 1) {
		if (expecteof)
		    return OK;

		perror ("read_tcp_socket");
		return DONE;
	    }
	    if (c == '\n')
		break;
	    *bp++ = c != '\r' ? c : '\0';

	    dig++;
	    if (dig < 4) {
		if (isdigit(c))
		    code = code * 10 + (c - '0');
		else				/* XXX: naughty FTP... */
		    if (isspace (c))
			continuation++;
	    }
	    else
		if (dig == 4 && c == '-')
		    continuation++;
	    if (n == 0)
		n = c;
	}

	if (v_debug)
	    fprintf (stderr, "---> %s\n", buffer);
	if (continuation)
	    continue;

	n -= '0';

	if (v_noise) {
	    fprintf (stdout, "%s\n", buffer);
	    fflush (stdout);
	    v_noise = 0;
	}
	else
	    if ((complete == -1 && n != PRELIM)
		    || (complete == 0 && n != CONTINUE && n != COMPLETE)
		    || (complete == 1 && n != COMPLETE))
		fprintf (stderr, "%s\n", buffer);

	return n;
    }
}
