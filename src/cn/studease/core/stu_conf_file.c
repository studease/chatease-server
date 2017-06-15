/*
 * stu_conf_file.c
 *
 *  Created on: 2017-6-7
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_str_t  STU_CONF_FILE_DEFAULT_PATH = stu_string("conf/chatd.conf");

static stu_str_t  STU_CONF_FILE_LOG = stu_string("log");
static stu_str_t  STU_CONF_FILE_PID = stu_string("pid");

static stu_str_t  STU_CONF_FILE_EDITION = stu_string("edition");
static stu_str_t  STU_CONF_FILE_MASTER_PROCESS = stu_string("master_process");
static stu_str_t  STU_CONF_FILE_WORKER_PROCESSES = stu_string("worker_processes");
static stu_str_t  STU_CONF_FILE_WORKER_THREADS = stu_string("worker_threads");

static stu_str_t  STU_CONF_FILE_SERVER = stu_string("server");
static stu_str_t  STU_CONF_FILE_SERVER_LISTEN = stu_string("listen");
static stu_str_t  STU_CONF_FILE_SERVER_HOSTNAME = stu_string("hostname");
static stu_str_t  STU_CONF_FILE_SERVER_PUSH_USERS = stu_string("push_users");
static stu_str_t  STU_CONF_FILE_SERVER_PUSH_USERS_INTERVAL = stu_string("push_users_interval");

static stu_str_t  STU_CONF_FILE_UPSTREAM = stu_string("upstream");
static stu_str_t  STU_CONF_FILE_UPSTREAM_URL = stu_string("url");
static stu_str_t  STU_CONF_FILE_UPSTREAM_ADDRESS = stu_string("address");
static stu_str_t  STU_CONF_FILE_UPSTREAM_PORT = stu_string("port");
static stu_str_t  STU_CONF_FILE_UPSTREAM_WEIGHT = stu_string("weight");
static stu_str_t  STU_CONF_FILE_UPSTREAM_MAX_FAILS = stu_string("max_fails");
static stu_str_t  STU_CONF_FILE_UPSTREAM_TIMEOUT = stu_string("timeout");


stu_int_t
stu_conf_file_parse(stu_config_t *cf, u_char *name, stu_pool_t *pool) {
	u_char                 temp[STU_CONF_FILE_MAX_SIZE];
	stu_file_t             file;
	stu_json_t            *conf, *item, *sub, *srv, *srv_property;
	stu_str_t             *v_string;
	stu_double_t          *v_double;
	stu_uint_t             hk;
	stu_list_t            *upstream;
	stu_upstream_server_t *server;

	file.fd = stu_file_open(name, STU_FILE_RDONLY, STU_FILE_CREATE_OR_OPEN, STU_FILE_DEFAULT_ACCESS);
	if (file.fd == STU_FILE_INVALID) {
		stu_log_error(stu_errno, "Failed to " stu_file_open_n " conf file \"%s\".", name);
		return STU_ERROR;
	}

	stu_memzero(temp, STU_CONF_FILE_MAX_SIZE);
	if (stu_file_read(&file, temp, STU_CONF_FILE_MAX_SIZE, 0) == STU_ERROR) {
		stu_log_error(stu_errno, "Failed to " stu_file_read_n " conf file \"%s\".", name);
		return STU_ERROR;
	}

	conf = stu_json_parse((u_char *) temp, file.offset);
	if (conf == NULL || conf->type != STU_JSON_TYPE_OBJECT) {
		stu_log_error(0, "Bad configure file format.");
		return STU_ERROR;
	}

	// log
	item = stu_json_get_object_item_by(conf, &STU_CONF_FILE_LOG);
	if (item && item->type == STU_JSON_TYPE_STRING) {
		// use default
	}

	// pid
	item = stu_json_get_object_item_by(conf, &STU_CONF_FILE_PID);
	if (item && item->type == STU_JSON_TYPE_STRING) {
		v_string = (stu_str_t *) item->value;
		cf->pid.name.data = stu_pcalloc(pool, v_string->len + 1);
		cf->pid.name.len = v_string->len;
		stu_strncpy(cf->pid.name.data, v_string->data, v_string->len);
	}

	// edition
	item = stu_json_get_object_item_by(conf, &STU_CONF_FILE_EDITION);
	if (item && item->type == STU_JSON_TYPE_STRING) {
		v_string = (stu_str_t *) item->value;
		if (stu_strncasecmp((u_char *) "preview", v_string->data, v_string->len) == 0) {
			cf->edition = PREVIEW;
		} else if (stu_strncasecmp((u_char *) "enterprise", v_string->data, v_string->len) == 0) {
			cf->edition = ENTERPRISE;
		} else {
			stu_log_error(0, "Unknown edition!");
			goto failed;
		}
	}

	// master_process
	item = stu_json_get_object_item_by(conf, &STU_CONF_FILE_MASTER_PROCESS);
	if (item && item->type == STU_JSON_TYPE_BOOLEAN) {
		cf->master_process = TRUE & item->value;
	}

	// worker_processes
	item = stu_json_get_object_item_by(conf, &STU_CONF_FILE_WORKER_PROCESSES);
	if (item && item->type == STU_JSON_TYPE_NUMBER) {
		v_double = (stu_double_t *) item->value;
		cf->worker_processes = *v_double;
	}

	// worker_threads
	item = stu_json_get_object_item_by(conf, &STU_CONF_FILE_WORKER_THREADS);
	if (item && item->type == STU_JSON_TYPE_NUMBER) {
		v_double = (stu_double_t *) item->value;
		cf->worker_threads = *v_double;
	}

	// server
	item = stu_json_get_object_item_by(conf, &STU_CONF_FILE_SERVER);
	if (item && item->type == STU_JSON_TYPE_OBJECT) {
		sub = stu_json_get_object_item_by(item, &STU_CONF_FILE_SERVER_LISTEN);
		if (sub) {
			v_double = (stu_double_t *) sub->value;
			cf->port = 0xFFFF & (stu_uint_t) *v_double;
		}

		sub = stu_json_get_object_item_by(item, &STU_CONF_FILE_SERVER_HOSTNAME);
		if (sub) {
			v_string = (stu_str_t *) sub->value;
			cf->hostname.data = stu_pcalloc(pool, v_string->len + 1);
			cf->hostname.len = v_string->len;
			stu_strncpy(cf->hostname.data, v_string->data, v_string->len);
		}

		sub = stu_json_get_object_item_by(item, &STU_CONF_FILE_SERVER_PUSH_USERS);
		if (sub) {
			cf->push_users = TRUE & sub->value;
		}

		sub = stu_json_get_object_item_by(item, &STU_CONF_FILE_SERVER_PUSH_USERS_INTERVAL);
		if (sub) {
			v_double = (stu_double_t *) sub->value;
			cf->push_users_interval = *v_double;
		}
	}

	// upstream
	item = stu_json_get_object_item_by(conf, &STU_CONF_FILE_UPSTREAM);
	if (item && item->type == STU_JSON_TYPE_OBJECT) {
		for (sub = (stu_json_t *) item->value; sub; sub = sub->next) {
			hk = stu_hash_key(sub->key.data, sub->key.len);
			upstream = stu_hash_find(&cf->upstreams, hk, sub->key.data, sub->key.len);
			if (upstream == NULL) {
				upstream = stu_pcalloc(pool, sizeof(stu_list_t));
				if (upstream == NULL) {
					stu_log_error(0, "Failed to pcalloc upstream list.");
					goto failed;
				}

				stu_list_init(upstream, pool, (stu_list_palloc_pt) stu_pcalloc, NULL);

				if (stu_hash_insert(&cf->upstreams, &sub->key, upstream, STU_HASH_LOWCASE|STU_HASH_REPLACE) == STU_ERROR) {
					stu_log_error(0, "Failed to insert upstream %s into hash.", sub->key.data);
					goto failed;
				}
			}

			for (srv = (stu_json_t *) sub->value; srv; srv = srv->next) {
				if (srv->type != STU_JSON_TYPE_OBJECT) {
					stu_log_error(0, "Bad upstream server format.");
					goto failed;
				}

				server = stu_pcalloc(pool, sizeof(stu_upstream_server_t));
				if (server == NULL) {
					stu_log_error(0, "Failed to pcalloc upstream server.");
					goto failed;
				}

				server->name.data = stu_pcalloc(pool, sub->key.len + 1);
				server->name.len = sub->key.len;
				stu_strncpy(server->name.data, sub->key.data, sub->key.len);

				srv_property = stu_json_get_object_item_by(srv, &STU_CONF_FILE_UPSTREAM_URL);
				if (srv_property && srv_property->type == STU_JSON_TYPE_STRING) {
					v_string = (stu_str_t *) srv_property->value;
					server->url.data = stu_pcalloc(pool, v_string->len + 1);
					server->url.len = v_string->len;
					stu_strncpy(server->url.data, v_string->data, v_string->len);
				}

				srv_property = stu_json_get_object_item_by(srv, &STU_CONF_FILE_UPSTREAM_ADDRESS);
				if (srv_property && srv_property->type == STU_JSON_TYPE_STRING) {
					v_string = (stu_str_t *) srv_property->value;
					server->addr.name.data = stu_pcalloc(pool, v_string->len + 1);
					server->addr.name.len = v_string->len;
					stu_strncpy(server->addr.name.data, v_string->data, v_string->len);
				}

				srv_property = stu_json_get_object_item_by(srv, &STU_CONF_FILE_UPSTREAM_PORT);
				if (srv_property && srv_property->type == STU_JSON_TYPE_NUMBER) {
					v_double = (stu_double_t *) srv_property->value;
					server->port = 0xFFFF & (stu_uint_t) *v_double;
				}

				srv_property = stu_json_get_object_item_by(srv, &STU_CONF_FILE_UPSTREAM_WEIGHT);
				if (srv_property && srv_property->type == STU_JSON_TYPE_NUMBER) {
					v_double = (stu_double_t *) srv_property->value;
					server->weight = *v_double;
				}

				srv_property = stu_json_get_object_item_by(srv, &STU_CONF_FILE_UPSTREAM_MAX_FAILS);
				if (srv_property && srv_property->type == STU_JSON_TYPE_NUMBER) {
					v_double = (stu_double_t *) srv_property->value;
					server->max_fails = *v_double;
				}

				srv_property = stu_json_get_object_item_by(srv, &STU_CONF_FILE_UPSTREAM_TIMEOUT);
				if (srv_property && srv_property->type == STU_JSON_TYPE_NUMBER) {
					v_double = (stu_double_t *) srv_property->value;
					server->timeout = *v_double;
				}

				server->addr.sockaddr.sin_family = AF_INET;
				server->addr.sockaddr.sin_addr.s_addr = inet_addr((const char *) server->addr.name.data);
				server->addr.sockaddr.sin_port = htons(server->port);
				bzero(&(server->addr.sockaddr.sin_zero), 8);
				server->addr.socklen = sizeof(struct sockaddr);

				stu_list_push(upstream, server, sizeof(stu_upstream_server_t));
			}
		}
	}

	stu_json_delete(conf);

	return STU_OK;

failed:

	stu_json_delete(conf);

	return STU_ERROR;
}
