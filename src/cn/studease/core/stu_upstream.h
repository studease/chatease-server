/*
 * stu_upstream.h
 *
 *  Created on: 2017-3-14
 *      Author: Tony Lau
 */

#ifndef STU_UPSTREAM_H_
#define STU_UPSTREAM_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_UPSTREAM_MAXIMUM         32
#define STU_UPSTREAM_DEFAULT_TIMEOUT 3

#define STU_UPSTREAM_SERVER_NORMAL   0x00
#define STU_UPSTREAM_SERVER_BACKUP   0x01
#define STU_UPSTREAM_SERVER_TIMEOUT  0x02
#define STU_UPSTREAM_SERVER_DOWN     0x04

#define STU_UPSTREAM_PEER_IDLE       0x00
#define STU_UPSTREAM_PEER_CONNECTED  0x01
#define STU_UPSTREAM_PEER_LOADING    0x02
#define STU_UPSTREAM_PEER_LOADED     0x04
#define STU_UPSTREAM_PEER_COMPLETED  0x08

typedef void (*stu_upstream_handler_pt)(stu_event_t *c);

typedef struct stu_upstream_server_s stu_upstream_server_t;

struct stu_upstream_server_s {
	stu_str_t                name;

	stu_str_t                protocol;
	stu_short_t              method;
	stu_addr_t               addr;
	in_port_t                port;
	stu_str_t                target;

	stu_uint_t               weight;
	stu_uint_t               max_fails;
	time_t                   timeout;

	stu_uint_t               fails;
	uint8_t                  state;

	stu_upstream_server_t   *next;
};

typedef struct {
	stu_connection_t        *connection;
	uint8_t                  state;
} stu_peer_connection_t;

struct stu_upstream_s {
	stu_upstream_handler_pt  read_event_handler;
	stu_upstream_handler_pt  write_event_handler;

	stu_upstream_server_t   *server;
	stu_peer_connection_t    peer;

	void                  *(*create_request_pt)(stu_connection_t *c);
	stu_int_t              (*reinit_request_pt)(stu_connection_t *c);
	stu_int_t              (*generate_request_pt)(stu_connection_t *c);
	stu_int_t              (*process_response_pt)(stu_connection_t *c);
	stu_int_t              (*analyze_response_pt)(stu_connection_t *c);
	void                   (*finalize_handler_pt)(stu_connection_t *c, stu_int_t rc);
	void                   (*cleanup_pt)(stu_connection_t *c);
};

stu_int_t  stu_upstream_create(stu_connection_t *c, u_char *name, size_t len);
stu_int_t  stu_upstream_init(stu_connection_t *c);
void       stu_upstream_cleanup(stu_connection_t *c);

#endif /* STU_UPSTREAM_H_ */
