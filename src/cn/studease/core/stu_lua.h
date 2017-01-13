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

stu_int_t stu_lua_init();
void stu_lua_close();

int stu_broadcast(lua_State *L);

#endif /* STU_LUA_H_ */
