#include "mem.h"
#include "objtypes.h"
#include "poll.h"
#include "trace.h"
#include "conf.h"
#include "db.h"
#include "cam.h"
#include "st.h"
#include "https.h"
#include <sys/sysinfo.h>
#include <unistd.h>

static uint32_t cc = 0;
static uint8_t mcheck = 0;

void
cacb(int cnt) {
  cc += cnt;
  if(!cc)
    mcheck = 1;
}

int
main(int argc, char **argv) {
  str_t str1;
  time_t ut, st;
  struct sysinfo si;
  /* char *ptr; */
  int ret;

  str1 = str_new();
  ASSERT(!str1, "str_new");

  /* ptr = strrchr(argv[0], '/'); */
  /* if(ptr && ((ptr-argv[0]) > 1)) { */
  /*   *ptr = 0; */
  /*   ret = chdir(argv[0]); */
  /*   ASSERT(ret<0, "chdir"); */
  /*   *ptr = '/'; */
  /* } */

  ret = conf_read();
  ASSERT(ret, "conf_read");

  time(&st);
  sysinfo(&si);
  ut = st - si.uptime;

  TR("-----------------------\n");
  TR("Start: %s", ctime(&st));

  ret = db_open(conf.db_path);
  ASSERT(ret, "db_open");

  str_reset(str1);
  ret = str_addf(str1, "update glc set utp=ut;"
                 "update glc set ut=%jd, st=%jd;", (uint64_t)ut, (uint64_t)st);
  ASSERT(ret, "str_addf");
  ret = sqlite3_exec(db, str1->v, 0, 0, &dberr);
  ASSERT(ret != SQLITE_OK, "sqlite3_exec: %s", dberr);

  OBJ_DESTROY(str1);

  /* START SYSTEM */

  ret = cam_init(cacb);
  ASSERT(ret, "cam_init");

  ret = st_init();
  ASSERT(ret, "st_init");

  ret = https_init(cacb);
  ASSERT(ret, "https_init");

  mem_set_sp();

  ret = cam_start();
  ASSERT(ret, "cam_start");

  while(1) {
    ret = poll_run(500);
    ASSERT(ret, "poll_run");

    if(mcheck) {
      /* TRC("memcheck:\n"); */
      mem_check();
      mcheck = 0;
    }
  }

  return 0;
 error:
  return -1;
}
