#include "ws.h"
#include "mem.h"
#include "objtypes.h"
#include "sm.h"
#include "wsp.h"
#include "stz.h"
#include "crypt.h"
#include "trace.h"
#include "conf.h"
#include "db.h"
#include "cam.h"

#define PSWFILE "admpsw"
#define DEFAULT_PSW "d033e22ae348aeb5660fc2140aec35850c4da997"
//#define WS_DEBUG

chain_t ws_cc = NULL;

static uint8_t inited = 0;
static void (*cacb)(int);
static uint16_t c_mc = 0;
/* static sqlite3_stmt *stm; */
static str_t str1;

static int _read_h(sm_sock_t sock, char *d, int ds);
static int _close_h(sm_sock_t sock);
static int _ws_msg_h(void *ro, int b1, int b2, str_t data);
static int _ss_h(cam_c_t cam, cam_media_t medias, char f);
// LOGIC
static int _lg_connected(ws_c_t c);
static int _lg_disconnected(ws_c_t c);
static int _lg_auth(ws_c_t c, hmap_t stz);

int
ws_init(void *_cacb) {
  int ret;

  if(!inited) {
    cacb = _cacb;

    c_mc = mem_new_ot("ws_connection",
                      sizeof(struct ws_c_st),
                      50, NULL);
    ASSERT(!c_mc, "mem_new_ot");

    ws_cc = chain_new();
    ASSERT(!ws_cc, "chain_new");

    str1 = str_new();
    ASSERT(!str1, "str_new");

    ret = sm_init();
    ASSERT(ret, "sm_init");

    ret = wsp_init();
    ASSERT(ret, "wsp_init");

    ret = cam_addh_ss(_ss_h);
    ASSERT(ret, "cam_addh_ss");

    inited = 1;
  }

  return 0;
 error:
  return -1;
}

int
ws_accept(sm_sock_t sock, hmap_t req, str_t key) {
  ws_c_t c;
  int ret;

  c = mem_alloc(c_mc);
  ASSERT(!c, "mem_alloc");
  c->sock = sock;
  c->psr = wsp_new(c, _ws_msg_h);
  ASSERT(!c->psr, "wsp_new");
  c->cid = str_new();
  ASSERT(!c->cid, "str_new");

  ret = _lg_connected(c);
  ASSERT(ret, "_lg_connected");

  sock->ro = c;
  sock->rh = _read_h;
  sock->eh = _close_h;

  cacb(1);

  ret = wsp_get_accept_key(key);
  ASSERT(ret, "wsp_get_accept_key");
  ret = sm_send(sock,
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: ", 97);
  ASSERT(ret, "sm_send");
  ret = sm_send(sock, key->v, key->l);
  ASSERT(ret, "sm_send");
  ret = sm_send(sock, "\r\n\r\n", 4);
  ASSERT(ret, "sm_send");

  return 0;
 error:
  return -1;
}

static int
_read_h(sm_sock_t sock, char *d, int ds) {
  ws_c_t c;
  int ret;

  c = (ws_c_t)sock->ro;

  ret = wsp_feed(c->psr, d, ds);
  ASSERT(ret, "wsp_feed");

  return 0;
 error:
  return -1;
}

static int
_close_h(sm_sock_t sock) {
  ws_c_t c;
  int ret;

  c = (ws_c_t)sock->ro;

  ret = _lg_disconnected(c);
  ASSERT(ret, "_lg_disconnected");

  OBJ_DESTROY(c->cid);
  wsp_destroy(c->psr);
  mem_free(c_mc, c, 0);

  cacb(-1);

  return 0;
 error:
  return -1;
}

static int
_ws_msg_h(void *ro, int b1, int b2, str_t data) {
  ws_c_t c;
  hmap_t stz;
  int ret;

  c = (ws_c_t)ro;

  if(b1 == 8) { // close frame
    ret = sm_close(c->sock);
    ASSERT(ret, "sm_sock");
    return 0;
  }

  if((b1 != 1) && (b1 != 2))
    return 0;

  ret = stz_unpack(data, (obj_t *)&stz);
  ASSERT(ret==-1, "stz_unpack");
  if(ret == 1) {
    ret = ws_sendSErr(c, "stanza parse fail", NULL);
    ASSERT(ret, "agent_sendSErr");
    OBJ_DESTROY(stz);
    return 0;
  }

#ifdef WS_DEBUG
  TR(OTMB "ws: "); OBJ_PRINT(stz); TR(OTM "\n");
#endif

  ret = c->lg_h(c, stz);
  ASSERT(ret, "lg_h");

  OBJ_DESTROY(stz);

  return 0;
 error:
  return -1;
}

static int
_ss_h(cam_c_t cam, cam_media_t medias, char f) {
  return 0;
}

int
ws_send(ws_c_t c, str_t data) {
  char *wsh;
  int ret, wshl;

#ifdef WS_DEBUG
  hmap_t stz;
  ret = stz_unpack(data, (obj_t *)&stz);
  ASSERT(ret, "stz_unpack");
  TR(OTMG "ws: "); OBJ_PRINT(stz); TR(OTM "\n");
  OBJ_DESTROY(stz);
#endif

  wshl = wsp_get_header(&wsh, data->l);
  ret = sm_send(c->sock, wsh, wshl);
  ASSERT(ret, "sm_send");
  ret = sm_send(c->sock, data->v, data->l);
  ASSERT(ret, "sm_send");

  return 0;
 error:
  return -1;
}

int
ws_sendAll(str_t data) {
  ws_c_t c;
  chain_slot_t chs;
  int ret;

  chs = ws_cc->first;
  while(chs) {
    c = (ws_c_t)chs->v;
    if(c->authed) {
      ret = ws_send(c, data);
      ASSERT(ret, "ws_send");
    }
    chs = chs->next;
  }

  return 0;
 error:
  return -1;
}

int
ws_sendS(ws_c_t c, hmap_t stz) {
  str_t data;
  int ret;

  data = str_new();
  ASSERT(!data, "str_new");
  str_2_byte(data);

  ret = stz_pack(data, OBJ(stz));
  ASSERT(ret, "stz_pack");
  
  ret = ws_send(c, data);
  ASSERT(ret, "ws_send");

  OBJ_DESTROY(data);

  return 0;
 error:
  return -1;
}

int
ws_sendSAll(hmap_t stz) {
  str_t data;
  int ret;

  data = str_new();
  ASSERT(!data, "str_new");
  str_2_byte(data);

  ret = stz_pack(data, OBJ(stz));
  ASSERT(ret, "stz_pack");
  
  ret = ws_sendAll(data);
  ASSERT(ret, "ws_sendAll");

  OBJ_DESTROY(data);

  return 0;
 error:
  return -1;
}

int
ws_sendSErr(ws_c_t c, char *msg, hmap_t _stz) {
  hmap_t stz;
  int ret;

  stz = _stz;

  if(!stz) {
    stz = hmap_new();
    ASSERT(!stz, "hmap_new");
  }

  ret = hmap_set_val_ks_vs(stz, "type", "error");
  ASSERT(ret, "hmap_set_val_ks_vs");
  ret = hmap_set_val_ks_vs(stz, "data", msg);
  ASSERT(ret, "hmap_set_val_ks_vs");

  ret = ws_sendS(c, stz);
  ASSERT(ret, "ws_sendS");

  if(!_stz)
    OBJ_DESTROY(stz);

  return 0;
 error:
  return -1;
}

int
ws_nf_camera__added(ws_c_t c, cam_c_t cam) {
  hmap_t stz, data;
  chain_slot_t chs;
  int ret;

  ASSERT((!c && !cam), "bad arguments");

  stz = hmap_new();
  ASSERT(!stz, "hmap_new");

  ret = hmap_set_val_ks_vs(stz, "ns", "camera");
  ASSERT(ret, "hmap_set_val_ks_vs");
  ret = hmap_set_val_ks_vs(stz, "type", "added");
  ASSERT(ret, "hmap_set_val_ks_vs");
  data = hmap_new();
  ASSERT(!data, "hmap_new");
  ret = hmap_set_val_ks(stz, "data", OBJ(data));
  ASSERT(ret, "hmap_set_val_ks");
  if(cam) {
    chs = NULL;
  } else {
    chs = cam_cc->first;
    cam = (chs ? ((cam_c_t)chs->v) : NULL);
  }
  while(cam) {
    ret = hmap_set_val_ks_vi(data, "id", cam->id);
    ASSERT(ret, "hmap_set_val_ks_vi");
    ret = hmap_set_val_ks_vs(data, "url", cam->url->v);
    ASSERT(ret, "hmap_set_val_ks_vs");
    ret = hmap_set_val_ks_vs(data, "name", cam->name->v);
    ASSERT(ret, "hmap_set_val_ks_vs");
    ret = hmap_set_val_ks_vs(data, "dsc", cam->dsc->v);
    ASSERT(ret, "hmap_set_val_ks_vs");
    ret = hmap_set_val_ks_vb(data, "working", cam->working);
    ASSERT(ret, "hmap_set_val_ks_vb");
    if(c) {
      ret = ws_sendS(c, stz);
      ASSERT(ret, "ws_sendS");
    } else {
      ret = ws_sendSAll(stz);
      ASSERT(ret, "ws_sendSAll");
    }
    chs = (chs ? chs->next : NULL);
    cam = (chs ? ((cam_c_t)chs->v) : NULL);
  }

  OBJ_DESTROY(stz);

  return 0;
 error:
  return -1;
}

int
ws_nf_camera__changed(ws_c_t c, cam_c_t cam) {
  hmap_t stz, data;
  int ret;

  ASSERT(!cam, "bad camera");

  stz = hmap_new();
  ASSERT(!stz, "hmap_new");

  ret = hmap_set_val_ks_vs(stz, "ns", "camera");
  ASSERT(ret, "hmap_set_val_ks_vs");
  ret = hmap_set_val_ks_vs(stz, "type", "changed");
  ASSERT(ret, "hmap_set_val_ks_vs");
  data = hmap_new();
  ASSERT(!data, "hmap_new");
  ret = hmap_set_val_ks(stz, "data", OBJ(data));
  ASSERT(ret, "hmap_set_val_ks");
  ret = hmap_set_val_ks_vi(data, "id", cam->id);
  ASSERT(ret, "hmap_set_val_ks_vi");
  ret = hmap_set_val_ks_vs(data, "url", cam->url->v);
  ASSERT(ret, "hmap_set_val_ks_vs");
  ret = hmap_set_val_ks_vs(data, "name", cam->name->v);
  ASSERT(ret, "hmap_set_val_ks_vs");
  ret = hmap_set_val_ks_vs(data, "dsc", cam->dsc->v);
  ASSERT(ret, "hmap_set_val_ks_vs");
  ret = hmap_set_val_ks_vb(data, "working", cam->working);
  ASSERT(ret, "hmap_set_val_ks_vb");
  if(c) {
    ret = ws_sendS(c, stz);
    ASSERT(ret, "ws_sendS");
  } else {
    ret = ws_sendSAll(stz);
    ASSERT(ret, "ws_sendSAll");
  }

  OBJ_DESTROY(stz);

  return 0;
 error:
  return -1;
}

int
ws_nf_camera__removed(ws_c_t c, uint32_t id) {
  hmap_t stz;
  int ret;

  ASSERT(!id, "bad id");

  stz = hmap_new();
  ASSERT(!stz, "hmap_new");

  ret = hmap_set_val_ks_vs(stz, "ns", "camera");
  ASSERT(ret, "hmap_set_val_ks_vs");
  ret = hmap_set_val_ks_vs(stz, "type", "removed");
  ASSERT(ret, "hmap_set_val_ks_vs");
  ret = hmap_set_val_ks_vi(stz, "id", id);
  ASSERT(ret, "hmap_set_val_ks_vi");
  if(c) {
    ret = ws_sendS(c, stz);
    ASSERT(ret, "ws_sendS");
  } else {
    ret = ws_sendSAll(stz);
    ASSERT(ret, "ws_sendSAll");
  }

  OBJ_DESTROY(stz);

  return 0;
 error:
  return -1;
}

static int
_lg_connected(ws_c_t c) {
  int ret;

  ret = sm_generate_clients_uid(c->sock, c->cid);
  ASSERT(ret, "sm_generate_clients_uid");

  ret = chain_append(ws_cc, OBJ(c));
  ASSERT(ret, "chain_append");

  c->authed = 0;
  c->lg_h = _lg_auth;

  return 0;
 error:
  return -1;
}

static int
_lg_disconnected(ws_c_t c) {
  chain_slot_t chs;

  chs = ws_cc->first;
  while(chs) {
    if(chs->v == OBJ(c)) {
      chain_remove_slot(ws_cc, chs);
      break;
    }
    chs = chs->next;
  }

  return 0;
}

static int
_lg_auth(ws_c_t c, hmap_t stz) {
  str_t ns, t, p;
  int ret;

  ns = STR(hmap_get_val_ks(stz, "ns", 0));
  t = STR(hmap_get_val_ks(stz, "type", 0));
  p = STR(hmap_get_val_ks(stz, "pass", 0));

  if(!ns || !t || !p ||
     (ns->obj.type != OBJ_STR) ||
     (t->obj.type != OBJ_STR) ||
     (p->obj.type != OBJ_STR) ||
     str_cmp_ws(ns, "authorization") ||
     str_cmp_ws(t, "authorize")) {
    ret = ws_sendSErr(c, "bad_auth_request", stz);
    ASSERT(ret, "ws_sendSErr");
    return 0;
  }

  str_reset(str1);
  ret = str_read_file(str1, PSWFILE);
  ASSERT(ret==-1, "str_read_file");
  if(ret) {
    ret = str_set_val(str1, DEFAULT_PSW, 0);
    ASSERT(ret, "str_set_val");
    ret = str_write_file(str1, PSWFILE);
    ASSERT(ret, "str_write_file");
  }

  if(str_cmp(p, str1)) {
    ret = ws_sendSErr(c, "fail", stz);
    ASSERT(ret, "ws_sendSErr");
  } else {
    ret = hmap_set_val_ks_vs(stz, "type", "success");
    ASSERT(ret, "hmap_set_val_ks_vs");
    ret = ws_sendS(c, stz);
    ASSERT(ret, "ws_sendS");
    c->authed = 1;
    c->lg_h = wsl_serve;
    ret = wsl_clientAuthorized(c);
    ASSERT(ret, "_lg_authorized");
  }

  return 0;
 error:
  return -1;
}
