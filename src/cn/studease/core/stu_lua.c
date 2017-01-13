/*
 * stu_lua.c
 *
 *  Created on: 2017-1-13
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

static const stu_str_t STU_LUA_SCRIPT_URI = stu_string("applications/main.lua");

static const stu_str_t STU_LUA_INFO_CODE_SHUTDOWN = stu_string("Application.Shutdown");
static const stu_str_t STU_LUA_INFO_CODE_GC = stu_string("Application.GC");

static const stu_str_t STU_LUA_INFO_LEVEL_STATUS = stu_string("status");

lua_State *L;


stu_int_t
stu_lua_init() {
	L = luaL_newstate();
	luaL_openlibs(L);

	lua_register(L, "stu_broadcast", stu_broadcast);

	luaL_loadfile(L, (const char *) STU_LUA_SCRIPT_URI.data);
	if (lua_resume(L, NULL, 0)) {
		stu_log_error(0, lua_tostring(L, -1));
		return STU_ERROR;
	}

	lua_getglobal(L, "stu_log");
	lua_pushfstring(L, "Message from %s/%s.", __NAME, __VERSION);
	if (lua_pcall(L, 1, 0, 0)) {
		stu_log_error(0, lua_tostring(L, -1));
		return STU_ERROR;
	}

	/*if (!lua_isstring(L, -1)) {
		stu_log_error(0, "log should return a string.");
		return STU_ERROR;
	}

	const char *str = lua_tostring(L, -1);
	stu_log("%s", str);

	lua_pop(L, -1);*/

	return STU_OK;
}

void
stu_lua_close() {
	if (L)
		lua_close(L);
}
