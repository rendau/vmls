#include "rtpp.h"
#include "trace.h"
#include "ns.h"

int
rtpp_parse_header(struct rtpp_header_st *h, unsigned char **nptr,
                  unsigned char *d, uint16_t ds) {
  uint16_t ehl;
  unsigned char b, *p;

  if(ds < 13)
    return 1;

  p = d;
  b = *p;
  h->v = b >> 6;
  h->p = (b & 0x20) ? 1 : 0;
  h->x = (b & 0x10) ? 1 : 0;
  h->cc = ((unsigned char)(b << 4)) >> 4;
  b = *(++p);
  h->m = (b & 0x80) ? 1 : 0;
  h->pt = ((unsigned char)(b << 1)) >> 1;
  p++;
  h->ord = ns_csu16(*((uint16_t *)p));
  p += 2;
  h->ts = ns_csu32(*((uint32_t *)p));
  p += 4;
  h->ssrc = ns_csu32(*((uint32_t *)p));
  p += 4;
  if(p-d+h->cc*4 >= ds)
    return 1;
  p += h->cc*4;
  if(h->x) {
    if(p-d+4 >= ds)
      return 1;
    p += 2;
    ehl = ns_csu16(*((uint16_t *)p));
    p += 2;
    if(p-d+ehl*4 >= ds)
      return 1;
    p += ehl*4;
  }

  *nptr = p;

  return 0;
}
