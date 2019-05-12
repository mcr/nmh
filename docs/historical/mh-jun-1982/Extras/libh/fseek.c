#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "/h/stat.h"
#include "/h/iobuf.h"
/* used for input using getc/getw */
/* does equivalent to requested seek */
/* by doing seek by blocks if necessary, */
/* and then adjusting nleft/nextp in iobuf */
/* first arg is iobuf pointer, others are identical to seek or lseek */
struct { int hiword, loword; };
struct { char lobyte, hibyte; };

fseek(bp, offs, dir)
struct iobuf *bp;
{
        long loff;

        loff = offs;
        if (dir%3 == 0) loff.hiword = 0;
        return(lfseek(bp, loff, dir));
}

lfseek(abp, aloff, adir)
struct iobuf *abp;
long aloff;
{
        register byteoff, blockoff;
        long loff;
        register struct iobuf *bp;
        int dir;
        struct inode statb;
        long lsize;

        bp = abp;
        dir = adir;
        loff = aloff;

        if (dir >= 3) {
                loff =<< 9;
                dir =- 3;
        }
        switch(dir) {
        case 1:
                /* relative seek */
                if (bp->b_nleft >= 0) {
                        if (bp->b_nextp) {
                                byteoff = bp->b_nextp-bp->b_buff;
                                loff =+ byteoff;
                                bp->b_nleft =+ byteoff;
                        }
                        break;
                }
                /* negative nleft means must be at e.o.f */
                /* drop into rel.-to-end seek */
        case 2:
                if (fstat(bp->b_fildes, &statb) < 0)
                        return(-1);
                lsize.hiword.hibyte = 0;
                lsize.hiword.lobyte = statb.i_size0;
                lsize.loword = statb.i_size1;
                /* adjust loff by size of file */
                loff =+ lsize;
                dir = 0;        /* now have offset rel.-to-beg. */
        case 0:
                /* rel.-to-beginning of file seek */
                bp->b_nleft = 0;
                break;
        default:
                return(-1);
        }

        bp->b_nextp = bp->b_buff;
        blockoff = loff>>9;
        byteoff = loff;
        byteoff =& 0777;
        if (dir != 1 || blockoff != 0) {
                /* must do the seek */
                if (seek(bp->b_fildes, blockoff, dir+3) < 0)
                        return(-1);
                bp->b_nleft = read(bp->b_fildes, &bp->b_buff, 512);
        }
        bp->b_nleft =- byteoff;
        bp->b_nextp =+ byteoff;
        retur