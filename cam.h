#ifndef CAM_HEADER
#define CAM_HEADER

#include "obj.h"
#include <inttypes.h>

typedef enum cam_media_e cam_media_t;
typedef struct cam_c_st *cam_c_t;
typedef struct cam_l_st *cam_l_t;

typedef struct sm_sock_st *sm_sock_t;
typedef struct httpp_st *httpp_t;
typedef struct hmap_st *hmap_t;
typedef struct chain_st *chain_t;
typedef struct str_st *str_t;
typedef struct h264_psr_st *h264_psr_t;
typedef struct mtsp_st *mtsp_t;

typedef int (*lg_h_fnt) (cam_c_t, hmap_t);
typedef int (*cam_ss_h_fnt) (cam_c_t cam, cam_media_t medias, char f);
typedef int (*cam_h264nal_h_fnt) (cam_c_t cam, str_t sps, str_t pps,
                                  uint32_t ts, unsigned char *d, uint32_t ds);

enum cam_media_e {
  CAM_MEDIA_UNDEFINED = 0x00,
  CAM_MEDIA_H264      = 0x01
};

struct cam_c_st {
  struct obj_st obj;
  sm_sock_t sock;
  httpp_t psr;
  uint32_t id;
  str_t url;
  str_t name;
  str_t dsc;
  // logic attrs
  uint8_t working;
  uint32_t cseq;
  uint64_t sid;
  cam_media_t medias;
  str_t ropts;
  h264_psr_t h264p;
  mtsp_t mtsp;
  sm_sock_t h264s;
  lg_h_fnt lg_h;
};

extern chain_t cam_cc;

int
cam_init(void *cacb);

int
cam_start();

int
cam_add(char *url, char *name, char *dsc);

int
cam_change(uint32_t id, char *url, char *name, char *dsc);

int
cam_remove(uint32_t id);

cam_c_t
cam_get_for_id(uint32_t id);

int
cam_addh_ss(cam_ss_h_fnt h);

int
cam_addh_h264nal(cam_h264nal_h_fnt h);

#endif
