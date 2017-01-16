/*
 * stu_lua.h
 *
 *  Created on: 2017-1-13
 *      Author: Tony Lau
 */

#ifndef STU_LUA_H_
#define STU_LUA_H_

#include "stu_config.h"
#include "stu_core.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define STU_LUA_INFO_CODE_SHUTDOWN 0x1
#define STU_LUA_INFO_CODE_GC       0x2

#define STU_LUA_INFO_LEVEL_STATUS  0x1

stu_int_t stu_lua_init();
void stu_lua_close();

stu_int_t stu_lua_onappstart();
stu_int_t stu_lua_onappstop(stu_int_t code, stu_int_t level);

stu_int_t stu_lua_onconnect(stu_connection_t *c, ...);
stu_int_t stu_lua_ondisconnect(stu_connection_t *c);

stu_int_t stu_lua_onmessage(stu_connection_t *c, u_char *message);

int stu_broadcast(lua_State *L);

#endif /* STU_LUA_H_ */
