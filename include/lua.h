#ifndef LUA_H
#define LUA_H

// Lua 5.4 Lightweight Runtime & Interactive REPL for LemonOS
// Author: Dwip (dwip-the-dev) <dwip@dwip.dedyn.io>

int lua_main(int argc, char** argv);
int lua_eval_string(const char* code);
int lua_run_file(const char* filename);

#endif // LUA_H
