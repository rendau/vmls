#include "mtsp.h"
#include "mem.h"
#include "objtypes.h"
#include "trace.h"

#define PSIZE 188
#define FRAMESEP "\x00\x00\x01"
#define FRAMESEP_LEN 3
#define TSLEN 3

static char inited;
static uint16_t psr_mc;
static str_t str1;

static unsigned char mts_header[] = {
  /* TS */
  0x47, 0x40, 0x00, 0x10, 0x00,
  /* PSI */
  0x00, 0xb0, 0x0d, 0x00, 0x01, 0xc1, 0x00, 0x00,
  /* PAT */
  0x00, 0x01, 0xf0, 0x01,
  /* CRC */
  0x2e, 0x70, 0x19, 0x05,
  /* stuffing 167 bytes */
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

  /* TS */
  0x47, 0x50, 0x01, 0x10, 0x00,
  /* PSI */
  0x02, 0xb0, 0x17, 0x00, 0x01, 0xc1, 0x00, 0x00,
  /* PMT */
  0xe1, 0x00,
  0xf0, 0x00,
  0x1b, 0xe1, 0x00, 0xf0, 0x00, /* h264 */
  0x0f, 0xe1, 0x01, 0xf0, 0x00, /* aac */
  /* CRC */
  0x2f, 0x44, 0xb9, 0x9b, /* crc for aac */
  /* stuffing 157 bytes */
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

int
mtsp_init() {
  if(!inited) {
    psr_mc = mem_new_ot("mtsp_parser",
                      sizeof(struct mtsp_st),
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

mtsp_t
mtsp_new(void *ro, mtsp_ts_cbt cb) {
  mtsp_t psr;

  ASSERT(!inited, "not inited");

  psr = mem_alloc(psr_mc);
  ASSERT(!psr, "mem_alloc");

  psr->ro = ro;
  psr->cb = cb;

  psr->buf = str_new();
  ASSERT(!psr->buf, "str_new");
  str_2_byte(psr->buf);

  psr->fcnt = 1;
  psr->sts = 0;

  return psr;
 error:
  return NULL;
}

int
mtsp_feed(mtsp_t psr, str_t pps, str_t sps,
	  uint32_t ts, unsigned char *d, uint32_t ds) {
   str_t temp;
   unsigned char pkt[PSIZE], *p, file_start, first, p_rest, pad;
   uint64_t pcr, v;
   uint32_t di, d_rest;
   int ret, type;

   type = (*d & 0x1f);

   if((type != 5) && (type != 1))
     return 0;

   if((type != 5) && !psr->buf->l)
     return 0;

   file_start = 0;
   if(type == 5 && ((ts - psr->sts) / 90000 >= TSLEN)) { // keyframe
     if(psr->buf->l) {
        str_reset(str1);
        temp = psr->buf;
        psr->buf = str1;
        str1 = temp;
        ret = psr->cb(psr->ro, str1, psr->sts, ts - psr->sts);
        ASSERT(ret, "psr->cb");
     }
     psr->sts = ts;
     ret = str_set_val(psr->buf, (char *)mts_header, PSIZE * 2);
     ASSERT(ret, "str_set_val");
     file_start = 1;
   }

   first = 1;
   di = 0;
   while(di < ds) {
     p = pkt;
     *p++ = 0x47;
     *p++ = 0x01;
     *p++ = 0x00;
     *p++ = 0x10 | (psr->fcnt & 0x0f);
     if(++psr->fcnt > 0x0f)
       psr->fcnt = 0;
     if(first) {
       pkt[1] |= 0x40;

       if(type == 5) {
         pkt[3] |= 0x20;
         *p++ = 7;
         *p++ = 0x50;
         pcr = ts - 63000;
         *p++ = (unsigned char) (pcr >> 25);
         *p++ = (unsigned char) (pcr >> 17);
         *p++ = (unsigned char) (pcr >> 9);
         *p++ = (unsigned char) (pcr >> 1);
         *p++ = (unsigned char) (pcr << 7 | 0x7e);
         *p++ = 0;
       }

       // PES header
       *p++ = 0x00;
       *p++ = 0x00;
       *p++ = 0x01;
       *p++ = 0xe0;

       v = (ds - di) + 5 + 3;
       if(file_start)
         v += 11+pps->l + sps->l;
       if(first)
         v += 9;
       if(v > 0xffff)
         v = 0;
       *p++ = (unsigned char) (v >> 8);
       *p++ = (unsigned char) v;
       *p++ = 0x80;
       *p++ = 0x80;
       *p++ = 5;

       pcr = ts + 63000;
       v = 0x20 | (((pcr >> 30) & 0x07) << 1) | 1;
       *p++ = (unsigned char) v;
       v = (((pcr >> 15) & 0x7fff) << 1) | 1;
       *p++ = (unsigned char) (v >> 8);
       *p++ = (unsigned char) v;
       v = (((pcr) & 0x7fff) << 1) | 1;
       *p++ = (unsigned char) (v >> 8);
       *p++ = (unsigned char) v;
     }

     if(first) {
       *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 1; *p++ = 0x09; *p++ = 0xf0;
     }

     if(file_start) {
       ASSERT(pkt+PSIZE-p <= 11+pps->l + sps->l,
              "too big sps and pps, can not place in one(188) packet");
       *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 1;
       memcpy(p, sps->v, sps->l);
       p += sps->l;
       *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 1;
       memcpy(p, pps->v, pps->l);
       p += pps->l;
     }

     if(first) {
       ASSERT(pkt+PSIZE-p <= 3,
              "too big sps and pps, can not place in one(188) packet");
       *p++ = 0; *p++ = 0; *p++ = 1;
     }

     p_rest = pkt + PSIZE - p;
     d_rest = ds - di;

     if(p_rest <= d_rest) {
       ret = str_add(psr->buf, (char *)pkt, p-pkt);
       ASSERT(ret, "str_add");
       ret = str_add(psr->buf, (char *)(d+di), p_rest);
       ASSERT(ret, "str_add");
       di += p_rest;
     } else {
       pad = p_rest - d_rest;
       if(pkt[3] & 0x20) { /* has adaptation */
         ret = str_add(psr->buf, (char *)pkt, 5);
         ASSERT(ret, "str_add");
         psr->buf->v[psr->buf->l-1] += pad;
         ret = str_expand(psr->buf, psr->buf->l+pad);
         ASSERT(ret, "str_expand");
         memset(psr->buf->v+psr->buf->l-pad, 0xff, pad);
         ret = str_add(psr->buf, (char *)(pkt+5), p-pkt-5);
         ASSERT(ret, "str_add");
       } else { /* no adaptation */
         pkt[3] |= 0x20;
         ret = str_add(psr->buf, (char *)pkt, 5);
         ASSERT(ret, "str_add");
         pad--;
         psr->buf->v[psr->buf->l-1] = pad;
         ret = str_expand(psr->buf, psr->buf->l+pad);
         ASSERT(ret, "str_expand");
         if(pad > 0) {
           memset(psr->buf->v+psr->buf->l-pad, 0xff, pad);
           psr->buf->v[psr->buf->l-pad] = 0;
         }
         if(p-pkt > 4) {
           ret = str_add(psr->buf, (char *)(pkt+4), p-pkt-4);
           ASSERT(ret, "str_add");
         }
       }
       ret = str_add(psr->buf, (char *)(d+di), d_rest);
       ASSERT(ret, "str_add");
       di = ds;
     }

     first = 0;
     file_start = 0;
   }

   return 0;
  error:
   return -1;
}

void
mtsp_reset(mtsp_t psr) {
  str_reset(psr->buf);
  psr->fcnt = 1;
  psr->sts = 0;
}

void
mtsp_destroy(mtsp_t psr) {
  if(psr)
    OBJ_DESTROY(psr->buf);
    mem_free(psr_mc, psr, 0);
}