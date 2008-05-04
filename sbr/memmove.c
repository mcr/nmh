/* public domain function from Jan Wolter Unix Incompatibility Notes */
/* http://unixpapa.com/incnote/ */
char *memmove(char *dst, char *src, int n)
{
  if (src > dst)
    for ( ; n > 0; n--)
      *(dst++)= *(src++);
  else
    for (dst+= n-1, src+= n-1; n > 0; n--)
      *(dst--)= *(src--);
}
