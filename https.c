#include "https.h"
#include "objtypes.h"
#include "poll.h"
#include "sm.h"
#include "httpp.h"
#include "trace.h"
#include "conf.h"
#include "ws.h"
#include "st.h"
#include "cam.h"
#include <string.h>
#include <inttypes.h>

#define HTTPD "httpd"
#define M3U8 "m3u8/"
#define STD "std/"
#define MANC 10

typedef struct manifest_st * manifest_t;

struct manifest_st {
  str_t fpath;
  uint8_t len;
  uint8_t id;
  uint32_t ver;
};

static uint8_t inited = 0;
static void (*cacb)(int);
static chain_t manifest_list;
static str_t str1, str2;

static char *exts[][2] = {
  {"txt", "text/plain"},
  {"html", "text/html"},
  {"htm", "text/html"},
  {"xml", "text/xml"},
  {"js", "application/javascript"},
  {"css", "text/css"},
  {"csv", "text/csv"},
  {"ico", "image/x-icon"},
  {"gif", "image/gif"},
  {"bmp", "image/bmp"},
  {"jpeg", "image/jpeg"},
  {"jpg", "image/jpeg"},
  {"png", "image/png"},
  {NULL, NULL}
};

static int _client_accept_h(sm_sock_t sock);
static int _client_read_h(sm_sock_t sock, char *d, int ds);
static int _client_close_h(sm_sock_t sock);
static int _client_first_req_h(void *ro, hmap_t req);
static int _client_req_h(void *ro, hmap_t req);
static int _m8u3_request(sm_sock_t sock, char *ptr, char *url);
static int _mts_request(sm_sock_t sock, char *ptr, char *url);
static int _mts_h(str_t file,char *fpath, uint32_t cam_id, uint32_t ts, uint32_t dr);
static int _cam_ss_h(cam_c_t cam, cam_media_t medias, char f);

int
https_init(void *_cacb) {
  sm_sock_t lsock;
  int ret;

  if(!inited) {
    cacb = _cacb;

    ret = sm_init();
    ASSERT(ret, "sm_init");

    ret = ws_init(_cacb);
    ASSERT(ret, "ws_init");

    ret = httpp_init();
    ASSERT(ret, "httpp_init");

    ret = st_init();
    ASSERT(ret, "st_init");

    ret = cam_init(cacb);
    ASSERT(ret, "cam_init");

    manifest_list = chain_new();
    ASSERT(!manifest_list, "chain_new");

    str1 = str_new();
    ASSERT(!str1, "str_new");

    str2 = str_new();
    ASSERT(!str2, "str_new");

    ret = cam_addh_ss(_cam_ss_h);
    ASSERT(ret, "cam_addh_ss");

    ret = st_add_mts_h(_mts_h);
    ASSERT(ret, "_mts_h");

    /* lsock = sm_slisten(conf.https_port, conf.cert, conf.cert_key); */
    /* ASSERT(!lsock, "sm_slisten"); */
    lsock = sm_listen(conf.https_port);
    ASSERT(!lsock, "sm_listen");
    lsock->ch = _client_accept_h;

    inited = 1;
  }

  return 0;
 error:
  return -1;
}

static int
_client_accept_h(sm_sock_t sock) {
  httpp_t psr;

  psr = httpp_new(sock, _client_first_req_h);
  ASSERT(!psr, "httpp_new");

  sock->ro = psr;
  sock->rh = _client_read_h;
  sock->eh = _client_close_h;

  cacb(1);

  return 0;
 error:
  return -1;
}

static int
_client_read_h(sm_sock_t sock, char *d, int ds) {
  int ret;

  ret = httpp_feed(sock->ro, d, ds);
  if(ret == 1) {
    ret = sm_send(sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 28);
    ASSERT(ret, "sm_send");
    httpp_reset(sock->ro);
  } else {
    ASSERT(ret, "httpp_feed");
  }

  return 0;
 error:
  return -1;
}

static int
_client_close_h(sm_sock_t sock) {
  httpp_destroy(sock->ro);
  cacb(-1);
  return 0;
}

static int
_client_first_req_h(void *ro, hmap_t req) {
  sm_sock_t sock;
  httpp_t psr;
  obj_t obj;
  int ret;

  sock = (sm_sock_t)ro;
  psr = (httpp_t)sock->ro;

  obj = hmap_get_val_ks(req, "Sec-WebSocket-Key", 0);
  if(obj) {
    sock->ro = NULL;
    sock->rh = NULL;
    sock->eh = NULL;
    ret = ws_accept(sock, req, STR(obj));
    ASSERT(ret, "ws_accept");
    httpp_destroy(psr);
    cacb(-1);
  } else {
    httpp_set_cb(psr, _client_req_h);
    return _client_req_h(ro, req);
  }

  return 0;
 error:
  return -1;
}

static int
_client_req_h(void *ro, hmap_t req) {
  sm_sock_t sock;
  str_t path, url, fdata, rhead;
  char *ptr, *ext;
  int ret, i;

  sock = (sm_sock_t)ro;

  /* OBJ_PRINT(req) TR("\n"); */

  url = STR(hmap_get_val_ks(req, "_h2_", 0));
  ASSERT(!url, "request has not URL");

  path = str_new_wv(HTTPD, 0);
  ASSERT(!path, "str_new_wv");

  //Arthur trogal
  if((ptr = strstr(url->v, M3U8)) > 0) {
    ptr += 5;
    ret = _m8u3_request(sock, ptr, url->v);
    ASSERT(ret, "_m8u3_request");
    return 0;
  }

  if((ptr = strstr(url->v, STD)) > 0) {
    ptr += 4;
    ret = _mts_request(sock, ptr, url->v);
    ASSERT(ret, "_mts_request");
    return 0;
  }
  //Arthur trogal end

  ret = str_add(path, url->v, url->l);
  ASSERT(ret, "str_add");

  if((ptr = strstr(path->v, "?")) > 0) {
    str_cut(path, ptr-path->v);
  }

  if(path->v[path->l-1] == '/') {
    ret = str_add(path, "index.html", 10);
    ASSERT(ret, "str_add");
  }

  /* TRC("https_request: %s\n", path->v); */

  ext = NULL;
  ptr = strrchr(path->v, '.');
  if(ptr) {
    ptr++;
    for(i=0; exts[i][0]; i++) {
      if(!strcmp(ptr, exts[i][0])) {
        ext = exts[i][1];
        break;
      }
    }
  }

  fdata = str_new();
  ASSERT(!fdata, "str_new");
  ret = str_read_file(fdata, path->v);
  ASSERT(ret==-1, "str_read_file");
  if(ret == 1) {
    OBJ_DESTROY(fdata);
    fdata = NULL;
  }

  OBJ_DESTROY(path);

  rhead = str_new();
  ASSERT(!rhead, "str_new");
  if(fdata) {
    ret = str_addf(rhead,
                   "HTTP/1.1 200 OK\r\n"
                   "Server: Mini http server\r\n"
                   "Content-Length: %d\r\n"
                   "Connection: Keep-Alive\r\n"
                   "Content-Type: %s\r\n\r\n",
                   fdata->l, ext ? ext : "");
    ASSERT(ret, "str_addf");
    ret = sm_send(sock, rhead->v, rhead->l);
    ASSERT(ret, "sm_send");
    ret = sm_send(sock, fdata->v, fdata->l);
    ASSERT(ret, "sm_send");
  } else {
    ret = str_add(rhead,
                  "HTTP/1.1 404 Not Found\r\n"
                  "Server: Mini http server\r\n"
                  "Connection: Keep-Alive\r\n"
                  "Content-Length: 9\r\n"
                  "\r\nNot found", 0);
    ASSERT(ret, "str_add");
    ret = sm_send(sock, rhead->v, rhead->l);
    ASSERT(ret, "sm_send");
  }
  OBJ_DESTROY(fdata);
  OBJ_DESTROY(rhead);

  return 0;
 error:
  return -1;
}

//Arthur trogal

static int
_m8u3_request(sm_sock_t sock, char *ptr, char *url) {
  int ret;
  uint32_t cam_id = 1;
  manifest_t mf;
  chain_slot_t slot;


  slot = manifest_list->first;

  while(slot != NULL) {
    mf = (manifest_t)slot->v;
    if(mf->id == cam_id)
      break;
    slot = slot->next;
  }

  str_reset(str2);
  ret = str_addf(str2,"#EXTM3U\r\n"
                      "#EXT-X-VERSION:3\r\n"
                      "#EXT-X-MEDIA-SEQUENCE:%d\r\n"
                      "#EXT-X-TARGETDURATION:5\r\n\r\n", mf->ver);
  ASSERT(ret, "str_addf");

  str_reset(str1);
  ret = str_addf(str1,"HTTP/1.1 200 OK\r\n"
                      "Server: Mini http server\r\n"
                      "Content-Length: %d\r\n"
                      "Connection: Keep-Alive\r\n\r\n", mf->fpath->l + str2->l);
  ASSERT(ret, "str_addf");

  ret = sm_send(sock, str1->v, str1->l);
  ASSERT(ret, "sm_send");
  ret = sm_send(sock, str2->v, str2->l);
  ASSERT(ret, "sm_send");
  ret = sm_send(sock, mf->fpath->v, mf->fpath->l);
  ASSERT(ret, "sm_send");

  return 0;
 error:
  return -1;
}

static int
_mts_request(sm_sock_t sock, char *ptr, char *url) {
  manifest_t mf;
  chain_slot_t slot;
  int ret;
  uint32_t cam_id = 1;

  str_reset(str1);
  str_reset(str2);

  slot = manifest_list->first;

  while(slot != NULL) {
    mf = (manifest_t)slot->v;
    if(mf->id == cam_id)
      break;
    slot = slot->next;
  }

  ret = str_read_file(str1, url+1);
  ASSERT(ret==-1, "str_read_file");

  ret = str_addf(str2,"HTTP/1.1 200 OK\r\n"
                      "Server: Mini http server\r\n"
                      "Content-Length: %d\r\n"
                      "Connection: Keep-Alive\r\n\r\n", str1->l);
  ASSERT(ret, "str_addf");

  ret = sm_send(sock, str2->v, str2->l);
  ASSERT(ret, "sm_send");

  ret = sm_send(sock, str1->v, str1->l);
  ASSERT(ret, "sm_send");

  return 0;
 error:
  return -1;
}

static int
_mts_h(str_t file, char *fpath, uint32_t cam_id, uint32_t ts, uint32_t dr) {
  int ret;
  char *r;
  manifest_t mf;
  chain_slot_t slot;

  slot = manifest_list->first;

  while(slot != NULL) {
    mf = (manifest_t)slot->v;
    if(mf->id == cam_id)
      break;
    slot = slot->next;
  }

  if(mf->len == 5) {
    str_reset(str1);
    r = strchr(mf->fpath->v, '\n') + 1;
    r = strchr(r, '\n') + 1;
    str_set_val(str1, r, mf->fpath->l - (r - mf->fpath->v));
    str_set_val(mf->fpath, str1->v, str1->l);
    mf->len = 4;
    mf->ver++;
  }
  str_reset(str1);
  ret = str_addf(str1, "#EXTINF:%d,\r\n/%s\r\n", dr/90000, fpath);
  ASSERT(ret, "str_addf");
  ret = str_add(mf->fpath, str1->v, str1->l);
  ASSERT(ret, "str_add");
  mf->len++;
  return 0;
 error:
  return -1;
}

static int
_cam_ss_h(cam_c_t cam, cam_media_t medias, char f) {
  manifest_t mf;
  int ret;

  if(f) {
    mf = malloc(sizeof(mf));
    mf->fpath = str_new();
    ASSERT(!mf, "str_new");
    mf->len = 0;
    mf->id = cam->id;
    mf->ver = 0;

    ret = chain_append(manifest_list, OBJ(mf));
    ASSERT(ret, "chain_append");
  }

  return 0;
 error:
  return -1;
}
//Arthur trogal end
