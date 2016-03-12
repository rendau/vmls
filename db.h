#ifndef DB_HEADER
#define DB_HEADER

#include <sqlite3.h>

extern sqlite3 *db;
extern char *dberr;

int
db_open(char *path);

#endif
