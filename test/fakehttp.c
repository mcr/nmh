/*
 * fakehttp - A fake HTTP server used by the nmh test suite
 *
 * This code is Copyright (c) 2014, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LINESIZE 1024
#define PIDFN "/tmp/fakehttp.pid"

int serve(const char *, const char *);
void putcrlf(int, char *);

static void
strip_cr(char *buf, ssize_t *len)
{
    ssize_t src, dst;
    for (src = dst = 0; src < *len; src++) {
        buf[dst] = buf[src];
        if (buf[src] != '\r') {
            dst++;
        }
    }
    *len -= src - dst;
}

static void
save_req(int conn, FILE *req)
{
    char buf[BUFSIZ];
    ssize_t r;
    int e;                      /* used to save errno */
    int started = 0;            /* whether the request has started coming in */

    if (fcntl(conn, F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr, "Unable to make socket non-blocking: %s\n",
                strerror(errno));
        exit(1);
    }

    for (;;) {
        r = read(conn, buf, sizeof buf);
        if (!started) {
            /* First keep trying until some data is ready; for testing, don't
             * bother with using select to wait for input. */
            if (r < 0) {
                e = errno;
                if (e == EAGAIN || e == EWOULDBLOCK) {
                    continue;   /* keep waiting */
                }
                fclose(req);
                fprintf(stderr, "Unable to read socket: %s\n", strerror(e));
                exit(1);
            }
            /* Request is here.  Fall through to the fwrite below and keep
             * reading. */
            started = 1;
        }
        if (r < 0) {
            e = errno;
            fputs("\n", req);   /* req body usually has no newline */
            fclose(req);
            if (e != EAGAIN && e != EWOULDBLOCK) {
                fprintf(stderr, "Unable to read socket: %s\n", strerror(e));
                exit(1);
            }
            /* For testing, we can get away without understand the HTTP request
             * and just treating the would-block case as meaning the request is
             * all done. */
            return;
        }
        strip_cr(buf, &r);
        fwrite(buf, 1, r, req);
    }
}

static void
send_res(int conn, FILE *res)
{
    size_t size;
    ssize_t len;
    char *res_line = NULL;

    while ((len = getline(&res_line, &size, res)) > 0) {
        res_line[len - 1] = '\0';
        putcrlf(conn, res_line);
    }
    free(res_line);
    if (!feof(res)) {
        fprintf(stderr, "read response failed: %s\n", strerror(errno));
        exit(1);
    }
}

int
main(int argc, char *argv[])
{
    struct st;
    int conn;
    FILE *req, *res;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s output-filename port response\n",
                argv[0]);
        exit(1);
    }

    if (!(req = fopen(argv[1], "w"))) {
        fprintf(stderr, "Unable to open output file \"%s\": %s\n",
                argv[1], strerror(errno));
        exit(1);
    }

    if (!(res = fopen(argv[3], "r"))) {
        fprintf(stderr, "Unable to open response \"%s\": %s\n",
                argv[3], strerror(errno));
        exit(1);
    }

    conn = serve(PIDFN, argv[2]);

    save_req(conn, req);

    send_res(conn, res);

    close(conn);

    return 0;
}
