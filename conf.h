#ifndef CONF_HEADER
#define CONF_HEADER

#include <inttypes.h>

struct conf_st {
  uint32_t http_port;
  uint32_t https_port;
  char cert[200];
  char cert_key[200];
  char std[200];
  char db_path[300];
};

extern struct conf_st conf;

int
conf_read();

#endif
