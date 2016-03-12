#ifndef ST_HEADER
#define ST_HEADER

#include <inttypes.h>

typedef struct str_st *str_t;

typedef int (*st_mts_h) (str_t d, char *path, uint32_t cam_id,
			 uint32_t ts, uint32_t dr);

int
st_init();

int
st_mts_save(str_t d, uint32_t cam_id, uint32_t ts, uint32_t dr);

int
st_add_mts_h(st_mts_h h);


#endif
