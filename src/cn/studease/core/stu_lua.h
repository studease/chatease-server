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

typedef struct {
	u_char    *code;
	stu_int_t  level;
} stu_lua_information_t;

typedef struct {
	stu_int_t  id;
} stu_lua_client_t;

typedef void (*stu_lua_appstart_handler_pt)();
typedef void (*stu_lua_appstop_handler_pt)(stu_lua_information_t *info);

typedef void (*stu_lua_connect_handler_pt)(stu_lua_client_t *client);
typedef void (*stu_lua_disconnect_handler_pt)(stu_lua_client_t *client);

typedef void (*stu_lua_message_handler_pt)(stu_lua_client_t *client, u_char *message);

typedef void (*stu_lua_accept_pt)(stu_lua_client_t *client);
typedef void (*stu_lua_reject_pt)(stu_lua_client_t *client, u_char *description);

typedef struct {
	stu_lua_appstart_handler_pt    onAppStart;
	stu_lua_appstop_handler_pt     onAppStop;

	stu_lua_connect_handler_pt     onConnect;
	stu_lua_disconnect_handler_pt  onDisonnect;

	stu_lua_message_handler_pt     onMessage;

	stu_lua_accept_pt              accept;
	stu_lua_reject_pt              reject;
} stu_application_t;

stu_int_t stu_lua_init();
void stu_lua_close();

#endif /* STU_LUA_H_ */
