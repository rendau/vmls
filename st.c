#include "st.h"
#include "mem.h"
#include "objtypes.h"
#include "trace.h"
#include "conf.h"

#define HL_SIZE 10

// TEMP
static uint32_t fid;

static char inited;
static st_mts_h mts_hs[HL_SIZE];
static str_t str1;

int
st_init() {
  if(!inited) {

    str1 = str_new();
    ASSERT(!str1, "str_new");

    fid = 0;

    inited = 1;
  }

  return 0;
 error:
  return -1;
}

int
st_mts_save(str_t mts, uint32_t cam_id, uint32_t ts, uint32_t dr) {
  uint16_t i;
  int ret;

  str_reset(str1);
  ret = str_addf(str1, "%s/%u.ts", conf.std, ++fid);
  ASSERT(ret, "str_addf");

  ret = str_write_file(mts, str1->v);
  ASSERT(ret, "str_write_file");

  for(i=0; i<HL_SIZE; i++) {
    if(mts_hs[i]) {
      ret = mts_hs[i](mts, str1->v, cam_id, ts, dr);
      ASSERT(ret, "mts_hs");
    }
  }

  return 0;
 error:
  return -1;
}

int
st_add_mts_h(st_mts_h h) {
  uint16_t i;

  ASSERT(!inited, "not inited");

  for(i=0; i<HL_SIZE; i++) {
    if(!mts_hs[i]) {
      mts_hs[i] = h;
      break;
    }
  }

  ASSERT(i==HL_SIZE, "no space");

  return 0;
 error:
  return -1;
}
