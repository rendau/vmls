#include "db.h"
#include "objtypes.h"
#include "trace.h"

sqlite3 *db = NULL;
char *dberr = NULL;

int
db_open(char *path) {
  sqlite3_stmt *stm;
  int ret;

  ret = sqlite3_open(path, &db);
  ASSERT(ret, "sqlite3_open");

  ret = sqlite3_prepare_v2(db,
                           "SELECT name FROM sqlite_master "
                           "WHERE type='table' AND name='glc';",
                           -1, &stm, NULL);
  ASSERT(ret!=SQLITE_OK, "sqlite3_prepare_v2");

  ret = sqlite3_step(stm);
  if(ret == SQLITE_ROW) {
    sqlite3_finalize(stm);
  } else {
    sqlite3_finalize(stm);
    ret = sqlite3_exec(db,
                       "begin;"
                       "create table glc(id unsigned bigint, owner unsigned bigint,"
                       "ut unsigned bigint, utp unsigned bigint, st unsigned bigint);"
                       "insert into glc(id, owner, ut, utp, st) "
		       "values(0, 0, 0, 0, 0);"
                       "create table cams(id integer primary key, url text,"
		       "name text, dsc text);"

                       "insert into cams(url, name, dsc) "
		       "values('rtsp://192.168.1.250/user=adm_password=admin_"
		       "channel=1_stream=1.sdp?real_stream', 'Стоянка',"
		       "'Камера стоянки');"
                       "insert into cams(url, name, dsc) "
		       "values('rtsp://192.168.1.251/user=adm_password=admin_"
		       "channel=1_stream=1.sdp?real_stream', 'Офис',"
		       "'Камера офиса');"

                       "commit;",
                       0, 0, &dberr);
    ASSERT(ret != SQLITE_OK, "sqlite3_exec: %s", dberr);
  }

  return 0;
 error:
  return -1;
}
