#include "conf.h"
#include "objtypes.h"
#include "trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONF_PATH "conf.cfg"

struct conf_st conf;

int
conf_read() {
  FILE *f;
  char key[255], val[255];
  int ret;

  f = fopen(CONF_PATH, "r");
  ASSERT(!f, "fopen");

  memset(&conf, 0, sizeof(struct conf_st));

  while(!feof(f)) {
    ret = fscanf(f, "%s = %s\n", key, val);
    if(ret == 2) {
      if(!strcmp(key, "http_port")) {
        conf.http_port = atoi(val);
      } else if(!strcmp(key, "https_port")) {
        conf.https_port = atoi(val);
      } else if(!strcmp(key, "cert")) {
        strcpy(conf.cert, val);
      } else if(!strcmp(key, "cert_key")) {
        strcpy(conf.cert_key, val);
      } else if(!strcmp(key, "std")) {
        strcpy(conf.std, val);
      } else if(!strcmp(key, "db_path")) {
	strcpy(conf.db_path, val);
      }
    }
  }

  fclose(f);

  return 0;
 error:
  return -1;
}
