#ifndef SQLITE_H
#define SQLITE_H

// SQLite 3.44 Embedded Relational Database Engine for LemonOS
// Author: Dwip (dwip-the-dev) <dwip@dwip.dedyn.io>

int sqlite_main(int argc, char** argv);
int sqlite_exec(const char* db_file, const char* sql);

#endif // SQLITE_H
