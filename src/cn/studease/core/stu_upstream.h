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

#define STU_UPSTREAM_MAXIMUM        32

#define STU_UPSTREAM_SERVER_NORMAL  0x00
#define STU_UPSTREAM_SERVER_BACKUP  0x01
#define STU_UPSTREAM_SERVER_TIMEOUT 0x02
#define STU_UPSTREAM_SERVER_DOWN    0x04

#define STU_UPSTREAM_PEER_IDLE      0x00
#define STU_UPSTREAM_PEER_BUSY      0x10
#define STU_UPSTREAM_PEER_CONNECTED 0x11
#define STU_UPSTREAM_PEER_LOADING   0x12
#define STU_UPSTREAM_PEER_LOADED    0x14
#define STU_UPSTREAM_PEER_COMPLETED 0x18

typedef void (*stu_upstream_handler_pt)(stu_event_t *c);

typedef struct {
	stu_str_t   name;
	stu_addr_t  addr;
	in_port_t   port;

	stu_uint_t  weight;
	stu_uint_t  max_fails;
	time_t      fail_timeout;

	uint8_t     state;
} stu_upstream_server_t;

typedef struct {
	stu_connection_t *connection;

	stu_uint_t        tries;
	stu_msec_t        start_time;

	uint8_t           state;
} stu_peer_connection_t;

struct stu_upstream_s {
	stu_upstream_handler_pt  read_event_handler;
	stu_upstream_handler_pt  write_event_handler;

	stu_upstream_server_t   *server;
	stu_peer_connection_t    peer;

	void                  *(*create_request)(stu_connection_t *c);
	stu_int_t              (*reinit_request)(stu_connection_t *c);
	stu_int_t              (*process_response)(stu_connection_t *c);
	stu_int_t              (*analyze_response)(stu_connection_t *c);
	void                   (*finalize_handler)(stu_connection_t *c, ...);
};

stu_int_t stu_upstream_create(stu_connection_t *c, u_char *name, size_t len);
stu_int_t stu_upstream_init(stu_connection_t *c);

stu_http_request_t *stu_upstream_create_http_request(stu_connection_t *c);
stu_int_t stu_upstream_reinit_http_request(stu_connection_t *c);

void stu_upstream_cleanup(stu_connection_t *c);

#endif /* STU_UPSTREAM_H_ */
