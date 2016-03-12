#include "cam.h"
#include "mem.h"
#include "objtypes.h"
#include "trace.h"
#include "sm.h"
#include "httpp.h"
#include "db.h"
#include "h264.h"
#include "mtsp.h"
#include "st.h"
#include "ws.h"
#include <string.h>

#define RTSP_PORT 554
//#define RTSP_DEBUG
#define HL_SIZE 10

chain_t cam_cc;

static char inited;
static void (*cacb)(int);
static uint16_t c_mc;
static uint32_t fport = 17000;
static sqlite3_stmt *stm;
static str_t str1;
static cam_ss_h_fnt ss_hl[HL_SIZE];
static cam_h264nal_h_fnt h264nal_hl[HL_SIZE];

static int _connect_h(sm_sock_t sock);
static int _read_h(sm_sock_t sock, char *d, int ds);
static int _close_h(sm_sock_t sock);
static int _h264_udp_read_h(sm_sock_t sock, uint32_t sa, char *d, int ds);
static int _h264_udp_close_h(sm_sock_t sock);
static int _send(cam_c_t c, str_t data);
static cam_c_t _connection_new();
static void _connection_destroy(cam_c_t c);
static int _rtsp_pkt_h(void *ro, hmap_t pkt);
static int _h264_nal_h(void *ro, uint32_t ts, unsigned char *d, uint32_t ds);
static int _mtsp_ts_h(void *ro, str_t d, uint32_t ts, uint32_t dr);
// LOGIC
static int _lg_connected(cam_c_t c);
static int _lg_disconnected(cam_c_t c, uint8_t reconnect);
static int __options(cam_c_t c, hmap_t pkt);
static int __describe(cam_c_t c, hmap_t pkt);
static int __setup_h264(cam_c_t c, hmap_t pkt);
static int __play(cam_c_t c, hmap_t pkt);
/* static int __teardown(cam_c_t c, hmap_t pkt); */

int
cam_init(void *_cacb) {
  int ret;

  if(!inited) {
    cacb = _cacb;

    ret = sm_init();
    ASSERT(ret, "sm_init");

    ret = httpp_init();
    ASSERT(ret, "httpp_init");

    ret = h264_init();
    ASSERT(ret, "h264_init");

    ret = mtsp_init();
    ASSERT(ret, "mtsp_init");

    ret = st_init();
    ASSERT(ret, "st_init");

    c_mc = mem_new_ot("cam_connection",
                      sizeof(struct cam_c_st),
                      100, NULL);
    ASSERT(!c_mc, "mem_new_ot");

    str1 = str_new();
    ASSERT(!str1, "str_new");

    cam_cc = chain_new();
    ASSERT(!cam_cc, "chain_new");

    inited = 1;
  }
  return 0;
 error:
  return -1;
}

int
cam_start() {
  cam_c_t c;
  int ret;

  ASSERT(!inited, "not inited");

  ret = sqlite3_prepare_v2(db, "SELECT id,url,name,dsc FROM cams;", -1, &stm, NULL);
  ASSERT(ret!=SQLITE_OK, "sqlite3_prepare_v2");
  while(sqlite3_step(stm) == SQLITE_ROW) {
    c = _connection_new(sqlite3_column_int64(stm, 0),
			(char *)sqlite3_column_text(stm, 1),
			(char *)sqlite3_column_text(stm, 2),
			(char *)sqlite3_column_text(stm, 3));
    ASSERT(!c, "_connection_new");

    ret = chain_append(cam_cc, OBJ(c));
    ASSERT(ret, "chain_append");

    cacb(1);
  }
  sqlite3_finalize(stm);

  return 0;
 error:
  return -1;
}

int
cam_add(char *url, char *name, char *dsc) {
  cam_c_t c;
  uint32_t cam_id;
  int ret;

  ASSERT(!inited, "not inited");

  ret = sqlite3_prepare_v2(db, "INSERT INTO cams(url,name,dsc) VALUES(?,?,?);",
			   -1, &stm, NULL);
  ASSERT(ret!=SQLITE_OK, "sqlite3_prepare_v2");
  ret = sqlite3_bind_text(stm, 1, url, -1, SQLITE_STATIC);
  ASSERT(ret!=SQLITE_OK, "sqlite3_bind_text");
  ret = sqlite3_bind_text(stm, 2, name, -1, SQLITE_STATIC);
  ASSERT(ret!=SQLITE_OK, "sqlite3_bind_text");
  ret = sqlite3_bind_text(stm, 3, dsc, -1, SQLITE_STATIC);
  ASSERT(ret!=SQLITE_OK, "sqlite3_bind_text");
  ret = sqlite3_step(stm);
  ASSERT(ret!=SQLITE_DONE, "sqlite3_step");
  sqlite3_finalize(stm);

  cam_id = sqlite3_last_insert_rowid(db);

  c = _connection_new(cam_id, url, name, dsc);
  ASSERT(!c, "_connection_new");

  ret = chain_append(cam_cc, OBJ(c));
  ASSERT(ret, "chain_append");

  cacb(1);

  ret = ws_nf_camera__added(NULL, c);
  ASSERT(ret, "ws_nf_camera__added");

  return 0;
 error:
  return -1;
}

int
cam_change(uint32_t id, char *url, char *name, char *dsc) {
  cam_c_t c;
  int ret, need_reconnect;

  ASSERT(!inited, "not inited");

  c = cam_get_for_id(id);
  if(!c)
    return 1;

  str_reset(str1);
  ret = str_addf(str1, "UPDATE cams SET url=?,name=?,dsc=? WHERE id=%u;", id);
  ASSERT(ret, "str_addf");
  ret = sqlite3_prepare_v2(db, str1->v, -1, &stm, NULL);
  ASSERT(ret!=SQLITE_OK, "sqlite3_prepare_v2");
  ret = sqlite3_bind_text(stm, 1, url, -1, SQLITE_STATIC);
  ASSERT(ret!=SQLITE_OK, "sqlite3_bind_text");
  ret = sqlite3_bind_text(stm, 2, name, -1, SQLITE_STATIC);
  ASSERT(ret!=SQLITE_OK, "sqlite3_bind_text");
  ret = sqlite3_bind_text(stm, 3, dsc, -1, SQLITE_STATIC);
  ASSERT(ret!=SQLITE_OK, "sqlite3_bind_text");
  ret = sqlite3_step(stm);
  ASSERT(ret!=SQLITE_DONE, "sqlite3_step");
  sqlite3_finalize(stm);

  need_reconnect = 0;
  if(str_cmp_ws(c->url, url))
    need_reconnect = 1;

  ret = str_set_val(c->url, url, 0);
  ASSERT(ret, "str_set_val");
  ret = str_set_val(c->name, name, 0);
  ASSERT(ret, "str_set_val");
  ret = str_set_val(c->dsc, dsc, 0);
  ASSERT(ret, "str_set_val");

  if(need_reconnect) {
    ret = sm_reconnect(c->sock);
    ASSERT(ret, "sm_reconnect");
  }

  ret = ws_nf_camera__changed(NULL, c);
  ASSERT(ret, "ws_nf_camera__changed");

  return 0;
 error:
  return -1;
}

int
cam_remove(uint32_t id) {
  cam_c_t c;
  int ret;

  ASSERT(!inited, "not inited");

  c = cam_get_for_id(id);
  if(!c)
    return 1;

  str_reset(str1);
  ret = str_addf(str1, "DELETE FROM cams WHERE id=%u;", id);
  ASSERT(ret, "str_addf");
  ret = sqlite3_exec(db, str1->v, 0, 0, &dberr);
  ASSERT(ret != SQLITE_OK, "sqlite3_exec: %s", dberr);

  ret = sm_close(c->sock);
  ASSERT(ret, "sm_close");

  return 0;
 error:
  return -1;
}

cam_c_t
cam_get_for_id(uint32_t id) {
  cam_c_t c;
  chain_slot_t chs;

  c = NULL;
  chs = cam_cc->first;
  while(chs) {
    c = (cam_c_t)chs->v;
    if(c->id == id)
      break;
    c = NULL;
    chs = chs->next;
  }

  return c;
}

int
cam_addh_ss(cam_ss_h_fnt h) {
  uint16_t i;

  ASSERT(!inited, "not inited");

  for(i=0; i<HL_SIZE; i++) {
    if(!ss_hl[i]) {
      ss_hl[i] = h;
      break;
    }
  }

  ASSERT(i==HL_SIZE, "no space");

  return 0;
 error:
  return -1;
}

int
cam_addh_h264nal(cam_h264nal_h_fnt h) {
  uint16_t i;

  ASSERT(!inited, "not inited");

  for(i=0; i<HL_SIZE; i++) {
    if(!h264nal_hl[i]) {
      h264nal_hl[i] = h;
      break;
    }
  }

  ASSERT(i==HL_SIZE, "no space");

  return 0;
 error:
  return -1;
}

static int
_connect_h(sm_sock_t sock) {
  cam_c_t c;
  int ret;

  c = (cam_c_t)sock->ro;

  ret = _lg_connected(c);
  ASSERT(ret, "_lg_connected");

  return 0;
 error:
  return -1;
}

static int
_read_h(sm_sock_t sock, char *d, int ds) {
  cam_c_t c;
  int ret;

  c = (cam_c_t)sock->ro;

#ifdef RTSP_DEBUG
  TR(OTMB "%.*s" OTM "\n", ds, d);
#endif

  ret = httpp_feed(c->psr, d, ds);
  ASSERT(ret, "httpp_feed");

  return 0;
 error:
  return -1;
}

static int
_close_h(sm_sock_t sock) {
  cam_c_t c;
  chain_slot_t chs1, chs2;
  int ret;

  c = (cam_c_t)sock->ro;

  ret = _lg_disconnected(c, c->sock->reconnect);
  ASSERT(ret, "_lg_disconnected");

  httpp_reset(c->psr);

  if(!c->sock->reconnect) {
    chs1 = cam_cc->first;
    while(chs1) {
      chs2 = chs1->next;
      if(chs1->v == OBJ(c))
	chain_remove_slot(cam_cc, chs1);
      chs1 = chs2;
    }

    _connection_destroy(c);

    cacb(-1);
  }

  return 0;
 error:
  return -1;
}

static int
_h264_udp_read_h(sm_sock_t sock, uint32_t sa, char *d, int ds) {
  cam_c_t c;
  int ret;

  c = (cam_c_t)sock->ro;

  ret = h264_psr_feed(c->h264p, (unsigned char *)d, ds);
  ASSERT(ret, "h264_psr_feed");

  return 0;
 error:
  return -1;
}

static int
_h264_udp_close_h(sm_sock_t sock) {
  return -1;
}

static int
_send(cam_c_t c, str_t data) {
  int ret;

#ifdef RTSP_DEBUG
  TR(OTMG "%s" OTM "\n", data->v);
#endif

  ret = sm_send(c->sock, data->v, data->l);
  ASSERT(ret, "sm_send");

  return 0;
 error:
  return -1;
}

static cam_c_t
_connection_new(uint32_t id, char *url, char *name, char *dsc) {
  cam_c_t c;
  char *p1, *p2;
  int ret;

  // extract ip host
  str_reset(str1);
  p1 = strstr(url, "rtsp://");
  if(p1) {
    p2 = p1 + 7;
    p1 = strchr(p2, '/');
    if(p1) {
      *p1 = 0;
      ret = str_add(str1, p2, p1-p2);
      ASSERT(ret, "str_add");
      *p1 = '/';
    }
  }
  ASSERT(!str1->l, "bad rtst-url");

  c = mem_alloc(c_mc);
  ASSERT(!c, "mem_alloc");

  memset(c, 0, sizeof(struct cam_c_st));

  c->sock = sm_connect(str1->v, RTSP_PORT, 3);
  ASSERT(!c->sock, "sm_connect");
  c->sock->ro = c;
  c->sock->ch = _connect_h;
  c->sock->rh = _read_h;
  c->sock->eh = _close_h;

  c->psr = httpp_new(c, _rtsp_pkt_h);
  ASSERT(!c->psr, "httpp_new");

  c->id = id;

  c->url = str_new_wv(url, 0);
  ASSERT(!c->url, "str_new_wv");

  c->name = str_new_wv(name, 0);
  ASSERT(!c->name, "str_new_wv");

  c->dsc = str_new_wv(dsc, 0);
  ASSERT(!c->dsc, "str_new_wv");

  c->ropts = str_new();
  ASSERT(!c->ropts, "str_new");

  c->h264p = h264_psr_new(c, _h264_nal_h);
  ASSERT(!c->h264p, "h264_psr_new");

  c->mtsp = mtsp_new(c, _mtsp_ts_h);
  ASSERT(!c->mtsp, "mtsp_new");

  return c;
 error:
  return NULL;
}

static void
_connection_destroy(cam_c_t c) {
  if(c) {
    httpp_destroy(c->psr);
    OBJ_DESTROY(c->url);
    OBJ_DESTROY(c->name);
    OBJ_DESTROY(c->dsc);
    OBJ_DESTROY(c->ropts);
    h264_psr_destroy(c->h264p);
    mtsp_destroy(c->mtsp);
    mem_free(c_mc, c, 0);
  }
}

static int
_rtsp_pkt_h(void *ro, hmap_t pkt) {
  cam_c_t c;
  int ret;

  c = (cam_c_t)ro;

  ret = c->lg_h(c, pkt);
  ASSERT(ret, "lg_h");

  return 0;
 error:
  return -1;
}

static int
_h264_nal_h(void *ro, uint32_t ts, unsigned char *d, uint32_t ds) {
  cam_c_t c;
  uint16_t i;
  int ret;

  c = (cam_c_t)ro;

  for(i=0; i<HL_SIZE; i++) {
    if(h264nal_hl[i]) {
      ret = h264nal_hl[i](c, c->h264p->sps, c->h264p->pps, ts, d, ds);
      ASSERT(ret, "h264nal_hl");
    }
  }

  ret = mtsp_feed(c->mtsp, c->h264p->sps, c->h264p->pps, ts, d, ds);
  ASSERT(ret, "mtsp_feed");

  return 0;
 error:
  return -1;
}

static int
_mtsp_ts_h(void *ro, str_t d, uint32_t ts, uint32_t dr) {
  cam_c_t c;
  uint16_t ret;

  c = (cam_c_t)ro;

  ret = st_mts_save(d, c->id, ts, dr);
  ASSERT(ret, "st_mts_save");

  return 0;
 error:
  return -1;
}



// LOGIC

static int
_lg_connected(cam_c_t c) {
  int ret;

#ifdef RTSP_DEBUG
  TRC("Connected to %s\n", c->sock->sas);
#endif

  str_reset(str1);
  ret = str_addf(str1,
                 "OPTIONS rtsp://%s RTSP/1.0\r\n"
                 "CSeq: %u\r\n"
                 "\r\n", c->sock->sas, ++c->cseq);
  ASSERT(ret, "str_addf");

  ret = _send(c, str1);
  ASSERT(ret, "_send");

  c->lg_h = __options;

  return 0;
 error:
  return -1;
}

static int
_lg_disconnected(cam_c_t c, uint8_t reconnect) {
  uint16_t i;
  int ret;

#ifdef RTSP_DEBUG
  TRC("Disconnected from %s\n", c->sock->sas);
#endif

  for(i=0; i<HL_SIZE; i++) {
    if(ss_hl[i]) {
      ret = ss_hl[i](c, c->medias, 0);
      ASSERT(ret, "ss_hl");
    }
  }

  c->working = 0;
  c->cseq = 0;
  c->sid = 0;
  c->medias = 0;
  str_reset(c->ropts);
  h264_psr_reset(c->h264p);
  mtsp_reset(c->mtsp);
  if(c->h264s) {
    ret = sm_close(c->h264s);
    ASSERT(ret, "sm_close");
    c->h264s = NULL;
  }
  c->lg_h = NULL;

  if(reconnect) {
    ret = ws_nf_camera__changed(NULL, c);
    ASSERT(ret, "ws_nf_camera__changed");
  } else {
    ret = ws_nf_camera__removed(NULL, c->id);
    ASSERT(ret, "ws_nf_camera__removed");
  }

  return 0;
 error:
  return -1;
}

static int
__options(cam_c_t c, hmap_t pkt) {
  str_t rcode, p;
  int ret;

  rcode = STR(hmap_get_val_ks(pkt, "_h2_", 0));
  p = STR(hmap_get_val_ks(pkt, "Public", 0));
  if(!rcode || str_cmp_ws(rcode, "200") ||
     !p || !strstr(p->v, "DESCRIBE") || !strstr(p->v, "SETUP") ||
     !strstr(p->v, "TEARDOWN") || !strstr(p->v, "PLAY") ||
     !strstr(p->v, "PAUSE")) {
    PWAR("bad reply from %s:\n", c->sock->sas);
    OBJ_PRINTLN(pkt);
    return 0;
  }

  str_reset(str1);
  ret = str_addf(str1,
                 "DESCRIBE %s RTSP/1.0\r\n"
                 "CSeq: %u\r\n"
                 "\r\n", c->url->v, ++c->cseq);
  ASSERT(ret, "str_addf");

  ret = _send(c, str1);
  ASSERT(ret, "_send");

  c->lg_h = __describe;

  return 0;
 error:
  return -1;
}

static int
__describe(cam_c_t c, hmap_t pkt) {
  sm_sock_t sock;
  str_t rcode, ctype, cont;
  int ret;

  rcode = STR(hmap_get_val_ks(pkt, "_h2_", 0));
  ctype = STR(hmap_get_val_ks(pkt, "Content-Type", 0));
  cont = STR(hmap_get_val_ks(pkt, "__content__", 0));
  if(!rcode || str_cmp_ws(rcode, "200") ||
     !ctype || str_cmp_ws(ctype, "application/sdp") || !cont) {
    PWAR("bad reply from %s:\n", c->sock->sas);
    OBJ_PRINTLN(pkt);
    return 0;
  }

  ret = str_copy(c->ropts, cont);
  ASSERT(ret, "str_copy");

  ret = h264_psr_parse_sdp(c->h264p, cont->v);
  ASSERT(ret==-1, "h264_psr_parse_sdp");
  if(ret) {
    PWAR("%s cam is not support h264(or bad format):\n%s\n", c->sock->sas, cont->v);
    return 0;
  }

  if(!c->h264p->sps->l)
    TRC("%s has not sps in sdp\n", c->sock->sas);
  if(!c->h264p->pps->l)
    TRC("%s has not pps in sdp\n", c->sock->sas);

  sock = sm_udp_listen(fport, 4096);
  ASSERT(!sock, "sm_udp_listen");
  sock->ro = c;
  sock->udp_rh = _h264_udp_read_h;
  sock->eh = _h264_udp_close_h;
  c->h264s = sock;

  str_reset(str1);
  ret = str_addf(str1,
                 "SETUP %s/%s RTSP/1.0\r\n"
                 "CSeq: %u\r\n"
                 "Transport: RTP/AVP;unicast;client_port=%u-%u\r\n"
                 "\r\n", c->url->v, c->h264p->ctrl->v, ++c->cseq, fport, fport);
  ASSERT(ret, "str_addf");

  ret = _send(c, str1);
  ASSERT(ret, "_send");

  c->lg_h = __setup_h264;

  fport++;

  return 0;
 error:
  return -1;
}

static int
__setup_h264(cam_c_t c, hmap_t pkt) {
  str_t rcode, session;
  int ret;

  rcode = STR(hmap_get_val_ks(pkt, "_h2_", 0));
  session = STR(hmap_get_val_ks(pkt, "Session", 0));
  if(!rcode || str_cmp_ws(rcode, "200") || !session) {
    PWAR("bad reply for SETUP from %s:\n", c->sock->sas);
    OBJ_PRINTLN(pkt);
    ret = sm_close(c->h264s);
    ASSERT(ret, "sm_close");
    c->h264s = NULL;
    return 0;
  }

  c->medias |= CAM_MEDIA_H264;

  c->sid = atol(session->v);

  str_reset(str1);
  ret = str_addf(str1,
                 "PLAY %s RTSP/1.0\r\n"
                 "CSeq: %u\r\n"
                 "Session: %jd\r\n"
                 "\r\n", c->url->v, ++c->cseq, c->sid);
  ASSERT(ret, "str_addf");

  ret = _send(c, str1);
  ASSERT(ret, "_send");

  c->lg_h = __play;

  return 0;
 error:
  return -1;
}

static int
__play(cam_c_t c, hmap_t pkt) {
  str_t rcode;
  uint16_t i;
  int ret;

  rcode = STR(hmap_get_val_ks(pkt, "_h2_", 0));
  if(!rcode || str_cmp_ws(rcode, "200")) {
    PWAR("bad reply from %s:\n", c->sock->sas);
    OBJ_PRINTLN(pkt);
    return 0;
  }

  c->working = 1;

  for(i=0; i<HL_SIZE; i++) {
    if(ss_hl[i]) {
      ret = ss_hl[i](c, c->medias, 1);
      ASSERT(ret, "ss_hl");
    }
  }

  ret = ws_nf_camera__changed(NULL, c);
  ASSERT(ret, "ws_nf_camera__changed");

  return 0;
 error:
  return -1;
}

/* static int */
/* __teardown(cam_c_t c, hmap_t pkt) { */
/*   str_t rcode; */

/*   rcode = STR(hmap_get_val_ks(pkt, "_h2_", 0)); */
/*   if(!rcode || str_cmp_ws(rcode, "200")) { */
/*     PWAR("bad reply from %s:\n", c->sock->sas); */
/*     OBJ_PRINTLN(pkt); */
/*   } */

/*   exit(0); */
/* } */
