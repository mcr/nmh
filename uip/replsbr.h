/* replsbr.h -- routines to help repl along...
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

void replout(FILE *inb, char *msg, char *drft, struct msgs *mp,
    int outputlinelen, int mime, char *form, char *filter, char *fcc,
    int fmtproc);

extern short ccto;
extern short cccc;
extern short ccme;
extern short querysw;
