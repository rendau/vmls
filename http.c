#include "http.h"
#include "ws.h"
#include "objtypes.h"
#include "poll.h"
#include "sm.h"
#include "httpp.h"
#include "trace.h"
#include "conf.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

static uint8_t inited = 0;
static void (*cacb)(int);
static str_t str1;

static int _client_accept_h(sm_sock_t sock);
static int _client_read_h(sm_sock_t sock, char *d, int ds);
static int _client_close_h(sm_sock_t sock);
static int _client_req_h(void *ro, hmap_t req);

int
http_init(void *_cacb) {
  sm_sock_t lsock;
  int ret;

  if(!inited) {
    cacb = _cacb;

    str1 = str_new();
    ASSERT(!str1, "str_new");

    ret = sm_init();
    ASSERT(ret, "sm_init");

    ret = httpp_init();
    ASSERT(ret, "httpp_init");

    lsock = sm_listen(conf.http_port);
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

  psr = httpp_new(sock, _client_req_h);
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
_client_req_h(void *ro, hmap_t req) {
  sm_sock_t sock;
  str_t host, part;
  char *ptr;
  int ret;

  sock = (sm_sock_t)ro;

  host = STR(hmap_get_val_ks(req, "Host", 0));
  if(!host) {
    ret = sm_send(sock, "HTTP/1.1 400 Bad Request\r\n\r\n", 0);
    ASSERT(ret, "sm_send");
    return 0;
  }

  ptr = strstr(host->v, ":");
  if(ptr) *ptr = 0;

  str_reset(str1);
  ret = str_addf(str1, "HTTP/1.1 301 Moved Permanently\r\n"
                 "Location: https://%s:%d\r\n\r\n", host->v, conf.https_port);

  ret = sm_send(sock, str1->v, str1->l);
  ASSERT(ret, "sm_send");

  return 0;
 error:
  return -1;
}
