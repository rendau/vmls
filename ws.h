#ifndef WS_HEADER
#define WS_HEADER

#include "obj.h"
#include <inttypes.h>

typedef struct ws_c_st *ws_c_t;

typedef struct sm_sock_st *sm_sock_t;
typedef struct wsp_st *wsp_t;
typedef struct hmap_st *hmap_t;
typedef struct chain_st *chain_t;
typedef struct str_st *str_t;
typedef struct cam_c_st *cam_c_t;

typedef int (*ws_lg_h_ft)(ws_c_t, hmap_t);

struct ws_c_st {
  struct obj_st obj;
  sm_sock_t sock;
  wsp_t psr;
  ws_lg_h_ft lg_h;
  str_t cid;
  char authed;
};

extern chain_t ws_cc;

int
ws_init();

int
ws_accept(sm_sock_t sock, hmap_t req, str_t key);

int
ws_send(ws_c_t c, str_t data);

int
ws_sendAll(str_t data);

int
ws_sendS(ws_c_t c, hmap_t stz);

int
ws_sendSAll(hmap_t stz);

int
ws_sendSErr(ws_c_t c, char *msg, hmap_t _stz);

int
ws_nf_camera__added(ws_c_t c, cam_c_t cam);

int
ws_nf_camera__changed(ws_c_t c, cam_c_t cam);

int
ws_nf_camera__removed(ws_c_t c, uint32_t id);

int
wsl_init();

int
wsl_clientAuthorized(ws_c_t c);

int
wsl_serve(ws_c_t c, hmap_t stz);

#endif
