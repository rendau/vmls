#include "ws.h"
#include "objtypes.h"
#include "stz.h"
#include "trace.h"
#include "db.h"
#include "cam.h"
#include <time.h>

typedef int (*h_ft)(ws_c_t, str_t, str_t, hmap_t);

static int camera__add(ws_c_t c, str_t ns, str_t type, hmap_t stz);
static int camera__change(ws_c_t c, str_t ns, str_t type, hmap_t stz);
static int camera__remove(ws_c_t c, str_t ns, str_t type, hmap_t stz);

static uint8_t inited = 0;
static str_t str1;

// ADMIN
static h_ft _ss[] = {
  camera__add,
  camera__change,
  camera__remove,
  NULL
};

int
wsl_init() {

  if(!inited) {
    str1 = str_new();
    ASSERT(!str1, "str_new");

    inited = 1;
  }

  return 0;
 error:
  return -1;
}

int
wsl_clientAuthorized(ws_c_t c) {
  int ret;

  ret = ws_nf_camera__added(c, NULL);
  ASSERT(ret, "ws_nf_camera__added");

  return 0;
 error:
  return -1;
}

int
wsl_serve(ws_c_t c, hmap_t stz) {
  h_ft *s;
  str_t ns, type;
  int ret, res;

  ns = STR(hmap_get_val_ks(stz, "ns", 0));
  type = STR(hmap_get_val_ks(stz, "type", 0));

  if(!ns || (ns->obj.type != OBJ_STR) ||
     !type || (type->obj.type != OBJ_STR)) {
    ret = ws_sendSErr(c, "bad_request", stz);
    ASSERT(ret, "ws_sendSErr");
    return 0;
  }

  res = 0;
  s = _ss;
  while(*s) {
    res = (*s)(c, ns, type, stz);
    if(res != 1)
      break;
    s++;
  }

  if(res == 1) {
    ret = ws_sendSErr(c, "not_supported", stz);
    ASSERT(ret, "ws_sendSErr");
    res = 0;
  }

  return res;
 error:
  return -1;
}

static int
camera__add(ws_c_t c, str_t ns, str_t type, hmap_t stz) {
  hmap_t data;
  str_t url, name, dsc;
  int ret;

  if(str_cmp_ws(ns, "camera"))
    return 1;
  if(str_cmp_ws(type, "add"))
    return 1;

  data = HMAP(hmap_get_val_ks(stz, "data", 0));
  if(!data || (data->obj.type != OBJ_HMAP)) {
    ret = ws_sendSErr(c, "bad_request", stz);
    ASSERT(ret, "ws_sendSErr");
    return 0;
  }

  url = STR(hmap_get_val_ks(data, "url", 0));
  name = STR(hmap_get_val_ks(data, "name", 0));
  dsc = STR(hmap_get_val_ks(data, "dsc", 0));
  if(!url || (url->obj.type != OBJ_STR) || !url->l ||
     !name || (url->obj.type != OBJ_STR) ||
     !dsc || (url->obj.type != OBJ_STR)) {
    ret = ws_sendSErr(c, "bad_data_attrs", stz);
    ASSERT(ret, "ws_sendSErr");
    return 0;
  }

  ret = cam_add(url->v, name->v, dsc->v);
  ASSERT(ret==-1, "cam_add");
  if(ret) {
    ret = ws_sendSErr(c, "fail", stz);
    ASSERT(ret, "ws_sendSErr");
  } else {
    hmap_remove_key_ks(stz, "data");
    ret = hmap_set_val_ks_vs(stz, "type", "success");
    ASSERT(ret, "hmap_set_val_ks_vs");
    ret = ws_sendS(c, stz);
    ASSERT(ret, "ws_sendS");
  }

  return 0;
 error:
  return -1;
}

static int
camera__change(ws_c_t c, str_t ns, str_t type, hmap_t stz) {
  hmap_t data;
  str_t url, name, dsc;
  int_t id;
  int ret;

  if(str_cmp_ws(ns, "camera"))
    return 1;
  if(str_cmp_ws(type, "change"))
    return 1;

  data = HMAP(hmap_get_val_ks(stz, "data", 0));
  if(!data || (data->obj.type != OBJ_HMAP)) {
    ret = ws_sendSErr(c, "bad_request", stz);
    ASSERT(ret, "ws_sendSErr");
    return 0;
  }

  id = INT(hmap_get_val_ks(data, "id", 0));
  url = STR(hmap_get_val_ks(data, "url", 0));
  name = STR(hmap_get_val_ks(data, "name", 0));
  dsc = STR(hmap_get_val_ks(data, "dsc", 0));
  if(!id || (id->obj.type != OBJ_INT) || !id->v ||
     !url || (url->obj.type != OBJ_STR) || !url->l ||
     !name || (url->obj.type != OBJ_STR) ||
     !dsc || (url->obj.type != OBJ_STR)) {
    ret = ws_sendSErr(c, "bad_data_attrs", stz);
    ASSERT(ret, "ws_sendSErr");
    return 0;
  }

  ret = cam_change(id->v, url->v, name->v, dsc->v);
  ASSERT(ret==-1, "cam_change");
  if(ret) {
    ret = ws_sendSErr(c, "fail", stz);
    ASSERT(ret, "ws_sendSErr");
  } else {
    hmap_remove_key_ks(stz, "data");
    ret = hmap_set_val_ks_vs(stz, "type", "success");
    ASSERT(ret, "hmap_set_val_ks_vs");
    ret = ws_sendS(c, stz);
    ASSERT(ret, "ws_sendS");
  }

  return 0;
 error:
  return -1;
}

static int
camera__remove(ws_c_t c, str_t ns, str_t type, hmap_t stz) {
  int_t id;
  int ret;

  if(str_cmp_ws(ns, "camera"))
    return 1;
  if(str_cmp_ws(type, "remove"))
    return 1;

  id = INT(hmap_get_val_ks(stz, "id", 0));
  if(!id || (id->obj.type != OBJ_INT) || !id->v) {
    ret = ws_sendSErr(c, "bad_request", stz);
    ASSERT(ret, "ws_sendSErr");
    return 0;
  }

  ret = cam_remove(id->v);
  ASSERT(ret==-1, "cam_remove");
  if(ret) {
    ret = ws_sendSErr(c, "fail", stz);
    ASSERT(ret, "ws_sendSErr");
  } else {
    ret = hmap_set_val_ks_vs(stz, "type", "success");
    ASSERT(ret, "hmap_set_val_ks_vs");
    ret = ws_sendS(c, stz);
    ASSERT(ret, "ws_sendS");
  }

  return 0;
 error:
  return -1;
}
