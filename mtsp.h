#ifndef MTSP_HEADER
#define MTSP_HEADER

#include "obj.h"
#include <inttypes.h>

typedef struct mtsp_st *mtsp_t;

typedef struct str_st *str_t;

typedef int (*mtsp_ts_cbt) (void *ro, str_t d, uint32_t ts, uint32_t dr);

struct mtsp_st {
  struct obj_st obj;
  void *ro;
  mtsp_ts_cbt cb;
  str_t buf;
  uint8_t fcnt; // fragment counter
  uint32_t sts; // start timestamp
};

int
mtsp_init();

mtsp_t
mtsp_new(void *ro, mtsp_ts_cbt cb);

int
mtsp_feed(mtsp_t psr, str_t pps, str_t sps,
	  uint32_t ts, unsigned char *d, uint32_t ds);

void
mtsp_reset(mtsp_t psr);

void
mtsp_destroy(mtsp_t psr);

#endif
