#ifndef RTPP_HEADER
#define RTPP_HEADER

#include <inttypes.h>

struct rtpp_header_st {
  unsigned char v;
  unsigned char p;
  unsigned char x;
  unsigned char cc;
  unsigned char m;
  unsigned char pt;
  uint16_t ord;
  uint32_t ts;
  uint32_t ssrc;
};

int
rtpp_parse_header(struct rtpp_header_st *h, unsigned char **nptr,
                  unsigned char *d, uint16_t ds);

#endif
