#include "h264.h"
#include "rtpp.h"
#include "mem.h"
#include "objtypes.h"
#include "trace.h"
#include "crypt.h"

static char inited;
static uint16_t psr_mc;
static str_t str1;

int
h264_init() {
  if(!inited) {
    psr_mc = mem_new_ot("h264_psr",
                      sizeof(struct h264_psr_st),
                      100, NULL);
    ASSERT(!psr_mc, "mem_new_ot");

    str1 = str_new();
    ASSERT(!str1, "str_new");

    inited = 1;
  }

  return 0;
 error:
  return -1;
}

h264_psr_t
h264_psr_new(void *ro, h264_psr_nal_cbt cb) {
  h264_psr_t psr;

  ASSERT(!inited, "not inited");

  psr = mem_alloc(psr_mc);
  ASSERT(!psr, "mem_alloc");

  psr->ro = ro;
  psr->cb = cb;
  psr->ctrl = str_new();
  ASSERT(!psr->ctrl, "str_new");
  psr->sps = str_new();
  ASSERT(!psr->sps, "str_new");
  psr->pps = str_new();
  ASSERT(!psr->pps, "str_new");
  psr->nu = str_new();
  ASSERT(!psr->nu, "str_new");
  psr->po = 0;

  return psr;
 error:
  return NULL;
}

int
h264_psr_parse_sdp(h264_psr_t psr, char *d) {
  char *p1, *p2, *p3, *p4, *p5, fnd;
  uint16_t npt;
  int ret;

  fnd = 0;
  while((p1 = strstr(d, "\r\nm=video"))) {
    p1 += 9;
    d = p1;

    p2 = strstr(p1, "m=");
    if(p2)
      *p2 = 0;

    npt = 0;
    p3 = p1;
    while((p4 = strstr(p3, "\r\na=rtpmap:"))) {
      p4 += 11;
      p3 = p4;
      if((p5 = strstr(p4, "\r\n"))) {
        *p5 = 0;
        if((p4 = strstr(p3, " H264/90000"))) {
          fnd = 1;
          *p4 = 0;
          npt = atol(p3);
          *p4 = ' ';
        }
        *p5 = '\r';
      }
    }
    if(!fnd)
      continue;

    if(!(p3 = strstr(p1, "\r\na=control:")))
      goto fail;
    p3 += 12;
    if(!(p4 = strstr(p3, "\r\n")))
      goto fail;
    *p4 = 0;
    ret = str_set_val(psr->ctrl, p3, 0);
    *p4 = '\r';

    str_reset(str1);
    ret = str_addf(str1, "a=fmtp:%d ", npt);
    ASSERT(ret, "str_addf");
    if((p3 = strstr(p1, str1->v))) {
      p3 += 7;
      p4 = strstr(p3, "\r\n");
      if(p4)
        *p4 = 0;
      if((p3 = strstr(p3, "sprop-parameter-sets="))) {
        p3 += 21;
        if((p5 = strpbrk(p3, ",; "))) {
          if(*p5 == ',') {
            *p5 = 0;
            ret = crypt_base64_dec(psr->sps, p3, p5-p3);
            *p5 = ',';
            if(ret)
              goto fail;
            p3 = p5 + 1;
            p5 = strchr(p3, ';');
            if(p5)
              *p5 = 0;
            ret = crypt_base64_dec(psr->pps, p3, 0);
            if(p5)
              *p5 = ';';
            if(ret)
              goto fail;
          }
        }
      }
      if(p4)
        *p4 = '\r';
    }

    if(p2)
      *p2 = 'm';

    break;
  }

  if(!fnd)
    goto fail;

  return 0;
 fail:
  str_reset(psr->ctrl);
  str_reset(psr->sps);
  str_reset(psr->pps);
  return 1;
 error:
  return -1;
}

int
h264_psr_feed(h264_psr_t psr, unsigned char *d, uint32_t ds) {
  struct rtpp_header_st rtph;
  unsigned char is_new, is_next, *p, b, F, NRI, Type;
  str_t tmpstr;
  int ret;

  ret = rtpp_parse_header(&rtph, &p, d, ds);
  if(ret)
    return 0;

  is_new = 0;
  if((rtph.ord>=psr->po) || (psr->po-rtph.ord>65000)) {
    is_new = 1;
  }
  is_next = 0;
  if(is_new)
    if((rtph.ord-psr->po==1) || (psr->po-rtph.ord==65535))
      is_next = 1;

  if(is_new)
    psr->po = rtph.ord;

  if((p-d) == ds)
    return 0;

  b = *p;
  F = (b & 0x80) ? 1 : 0;
  NRI = ((unsigned char)(b << 1)) >> 6;
  Type = (b & 0x1f);

  /* TRC("NAL F: %d, NRI: %d, Type: %d\n", F, NRI, Type); */

  if(NRI > 0) {
    if((Type > 0) && (Type < 24)) { // Single packet
      str_reset(psr->nu);
      if(is_new) {
        if(Type == 7) { // SPS
          ret = str_set_val(psr->sps, (char *)p, ds-(p-d));
          ASSERT(ret, "str_set_val");
        } else if(Type == 8) { // PPS
          ret = str_set_val(psr->pps, (char *)p, ds-(p-d));
          ASSERT(ret, "str_set_val");
        } else {
          ret = psr->cb(psr->ro, rtph.ts, p, ds-(p-d));
          ASSERT(ret, "cb");
        }
      }
    } else if(Type == 28) { // Fragment
      if(psr->sps->l && psr->pps->l) { // Only if we have sps and pps
        p++;
        if((p-d) >= ds)
          return 0;
        b = *p;
        Type = (b & 0x1f);
        p++;
        if(b & 0x80) {
          str_reset(psr->nu);
          b = ((unsigned char)(F << 7)) + ((unsigned char)(NRI << 5)) + Type;
          ret = str_add_one(psr->nu, b);
          ASSERT(ret, "str_add_one");
          if((p-d) < ds) {
            ret = str_add(psr->nu, (char *)p, ds-(p-d));
            ASSERT(ret, "str_set_val");
          }
        } else {
          if(psr->nu->l && is_next) {
            if((p-d) < ds) {
              ret = str_add(psr->nu, (char *)p, ds-(p-d));
              ASSERT(ret, "str_add");
            }
            if(b & 0x40) {
              str_reset(str1);
              tmpstr = psr->nu;
              psr->nu = str1;
              str1 = tmpstr;
              ret = psr->cb(psr->ro, rtph.ts, (unsigned char *)str1->v, str1->l);
              ASSERT(ret, "cb");
            }
          } else {
            str_reset(psr->nu);
          }
        }
      }
    } else {
      TRC("Undefined-NAL F: %d, NRI: %d, Type: %d\n", F, NRI, Type);
    }
  }

  return 0;
 error:
  return -1;
}

void
h264_psr_reset(h264_psr_t psr) {
  str_reset(psr->ctrl);
  str_reset(psr->sps);
  str_reset(psr->pps);
  str_reset(psr->nu);
  psr->po = 0;
}

void
h264_psr_destroy(h264_psr_t psr) {
  if(psr) {
    OBJ_DESTROY(psr->ctrl);
    OBJ_DESTROY(psr->sps);
    OBJ_DESTROY(psr->pps);
    OBJ_DESTROY(psr->nu);
    mem_free(psr_mc, psr, 0);
  }
}
