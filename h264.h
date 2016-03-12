#ifndef H264_HEADER
#define H264_HEADER

#include "obj.h"
#include <inttypes.h>

typedef struct h264_psr_st *h264_psr_t;

typedef struct str_st *str_t;

typedef int (*h264_psr_nal_cbt) (void *ro, uint32_t ts, unsigned char *d, uint32_t ds);

struct h264_psr_st {
  struct obj_st obj;
  void *ro;
  h264_psr_nal_cbt cb;
  str_t ctrl;
  str_t sps;
  str_t pps;
  str_t nu;
  uint16_t po;
};

int
h264_init();

h264_psr_t
h264_psr_new(void *ro, h264_psr_nal_cbt cb);

int
h264_psr_parse_sdp(h264_psr_t psr, char *d);

int
h264_psr_feed(h264_psr_t psr, unsigned char *d, uint32_t ds);

void
h264_psr_reset(h264_psr_t psr);

void
h264_psr_destroy(h264_psr_t psr);

#endif
