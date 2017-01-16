/*
 * stu_lua.c
 *
 *  Created on: 2017-1-13
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

static const stu_str_t STU_LUA_SCRIPT_URI = stu_string("applications/main.lua");

const stu_str_t STU_LUA_INTERFACE_ONAPPSTART = stu_string("onAppStart");
const stu_str_t STU_LUA_INTERFACE_ONAPPSTOP = stu_string("onAppStop");
const stu_str_t STU_LUA_INTERFACE_ONCONNECT = stu_string("onConnect");
const stu_str_t STU_LUA_INTERFACE_ONDISCONNECT = stu_string("onDisconnect");
const stu_str_t STU_LUA_INTERFACE_ONMESSAGE = stu_string("onMessage");

const stu_str_t STU_LUA_INFO_CODE_EXPLAIN_SHUTDOWN = stu_string("Application.Shutdown");
const stu_str_t STU_LUA_INFO_CODE_EXPLAIN_GC = stu_string("Application.GC");

const stu_str_t STU_LUA_INFO_LEVEL_EXPLAIN_STATUS = stu_string("status");

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

	return STU_OK;
}

void
stu_lua_close() {
	if (L)
		lua_close(L);
}


stu_int_t
stu_lua_onappstart() {
	if (L == NULL) {
		return STU_ERROR;
	}

	lua_getglobal(L, (const char *) STU_LUA_INTERFACE_ONAPPSTART.data);

	if (lua_pcall(L, 0, 0, 0)) {
		stu_log_error(0, lua_tostring(L, -1));
		lua_pop(L, 1);

		return STU_ERROR;
	}

	return STU_OK;
}

stu_int_t
stu_lua_onappstop(stu_int_t code, stu_int_t level) {
	if (L == NULL) {
		return STU_ERROR;
	}

	lua_getglobal(L, (const char *) STU_LUA_INTERFACE_ONAPPSTOP.data);

	if (code == STU_LUA_INFO_CODE_SHUTDOWN) {
		lua_pushstring(L, (const char *) STU_LUA_INFO_CODE_EXPLAIN_SHUTDOWN.data);
	} else {
		lua_pushstring(L, (const char *) STU_LUA_INFO_CODE_EXPLAIN_GC.data);
	}

	lua_pushstring(L, (const char *) STU_LUA_INFO_LEVEL_EXPLAIN_STATUS.data);

	if (lua_pcall(L, 2, 0, 0)) {
		stu_log_error(0, lua_tostring(L, -1));
		lua_pop(L, 1);

		return STU_ERROR;
	}

	return STU_OK;
}

stu_int_t
stu_lua_onconnect(stu_connection_t *c, ...) {
	va_list    args;
	u_char    *arg;
	stu_int_t  n;

	if (L == NULL) {
		return STU_ERROR;
	}

	lua_getglobal(L, (const char *) STU_LUA_INTERFACE_ONCONNECT.data);

	lua_newtable(L);
	lua_pushstring(L, "id");
	lua_pushinteger(L, c->user.id);
	lua_settable(L, -3);
	lua_pushstring(L, "name");
	lua_pushstring(L, (const char *) c->user.name.data);
	lua_settable(L, -3);
	lua_pushstring(L, "channel");
	lua_pushstring(L, (const char *) c->user.channel->id.data);
	lua_settable(L, -3);

	va_start(args, c);
	for (arg = va_arg(args, u_char *), n = 0; arg; n++) {
		lua_pushstring(L, (const char *) arg);
	}
	va_end(args);

	if (lua_pcall(L, 1 + n, 2, 0)) {
		stu_log_error(0, lua_tostring(L, -1));
		lua_pop(L, 1);

		return STU_ERROR;
	}

	return STU_OK;
}

stu_int_t
stu_lua_ondisconnect(stu_connection_t *c) {
	if (L == NULL) {
		return STU_ERROR;
	}

	lua_getglobal(L, (const char *) STU_LUA_INTERFACE_ONDISCONNECT.data);

	lua_newtable(L);
	lua_pushstring(L, "id");
	lua_pushinteger(L, c->user.id);
	lua_settable(L, -3);
	lua_pushstring(L, "name");
	lua_pushstring(L, (const char *) c->user.name.data);
	lua_settable(L, -3);
	lua_pushstring(L, "channel");
	lua_pushstring(L, (const char *) c->user.channel->id.data);
	lua_settable(L, -3);

	if (lua_pcall(L, 1, 0, 0)) {
		stu_log_error(0, lua_tostring(L, -1));
		lua_pop(L, 1);

		return STU_ERROR;
	}

	return STU_OK;
}

stu_int_t
stu_lua_onmessage(stu_connection_t *c, u_char *message) {
	if (L == NULL) {
		return STU_ERROR;
	}

	lua_getglobal(L, (const char *) STU_LUA_INTERFACE_ONMESSAGE.data);

	lua_newtable(L);
	lua_pushstring(L, "id");
	lua_pushinteger(L, c->user.id);
	lua_settable(L, -3);

	lua_pushstring(L, (const char *) message);

	if (lua_pcall(L, 2, 2, 0)) {
		stu_log_error(0, lua_tostring(L, -1));
		lua_pop(L, 1);

		return STU_ERROR;
	}

	return STU_OK;
}
