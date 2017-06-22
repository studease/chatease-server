/*
 * stu_upstream_ident.c
 *
 *  Created on: 2017-3-16
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

extern stu_cycle_t *stu_cycle;
stu_int_t           stu_preview_auto_id = 1001;

stu_str_t  STU_HTTP_UPSTREAM_IDENT = stu_string("ident");
stu_str_t  STU_HTTP_UPSTREAM_IDENT_PARAM_TOKEN = stu_string("token");

stu_str_t  STU_HTTP_UPSTREAM_IDENT_REQUEST = stu_string(

		"GET /websocket/data/userinfo.json?channel=%s&token=%s HTTP/1.1" CRLF
		"Host: localhost" CRLF
		"User-Agent: " __NAME "/" __VERSION CRLF
		"Accept: application/json" CRLF
		"Accept-Charset: utf-8"
		"Accept-Language: zh-CN,zh;q=0.8,en-US;q=0.5,en;q=0.3" CRLF
		"Connection: keep-alive" CRLF CRLF
		/*
		"POST /live/method=httpChatRoom HTTP/1.1" CRLF
		"Host: www.qcus.cn" CRLF
		"User-Agent: " __NAME "/" __VERSION CRLF
		"Accept: text/html" CRLF
		"Accept-Language: zh-CN,zh;q=0.8" CRLF
		"Content-Type: application/json" CRLF
		"Content-Length: %ld" CRLF
		"Connection: keep-alive" CRLF CRLF
		"{\"channel\":\"%s\",\"token\":\"%s\"}"
		*/
	);

stu_str_t  STU_HTTP_UPSTREAM_IDENT_RESPONSE = stu_string(
		"{\"raw\":\"ident\",\"user\":{\"id\":\"%ld\",\"name\":\"%s\",\"role\":%d},\"channel\":{\"id\":\"%s\",\"state\":%d,\"total\":%lu}}"
	);


stu_int_t
stu_http_upstream_ident_generate_request(stu_connection_t *c) {
	stu_http_request_t *r, *pr;
	stu_upstream_t     *u;
	stu_connection_t   *pc;
	stu_str_t           arg;
	u_char             *temp, *data, channel[STU_CHANNEL_ID_MAX_LEN], token[STU_HTTP_UPSTREAM_IDENT_TOKEN_MAX_LEN];

	r = (stu_http_request_t *) c->data;
	u = c->upstream;
	pc = u->peer.connection;
	pr = (stu_http_request_t *) pc->data;

	stu_strncpy(channel, r->target.data, r->target.len);

	stu_str_null(&arg);
	if (stu_http_arg(r, STU_HTTP_UPSTREAM_IDENT_PARAM_TOKEN.data, STU_HTTP_UPSTREAM_IDENT_PARAM_TOKEN.len, &arg) != STU_OK) {
		stu_log_debug(4, "Param \"%s\" not found while sending upstream %s: fd=%d.", STU_HTTP_UPSTREAM_IDENT_PARAM_TOKEN.data, u->server->name.data, pc->fd);
	}
	*token = '\0';
	stu_strncpy(token, arg.data, arg.len);

	temp = pr->request_body.start;
	if (temp == NULL) {
		temp = stu_slab_calloc(stu_cycle->slab_pool, STU_HTTP_REQUEST_DEFAULT_SIZE);
		if (temp == NULL) {
			stu_log_error(0, "Failed to slab_calloc() request buffer: fd=%d.", c->fd);
			return STU_ERROR;
		}
	}

	data = stu_sprintf(temp, (const char *) STU_HTTP_UPSTREAM_IDENT_REQUEST.data, channel, token);
	//data = stu_sprintf(temp, (const char *) STU_HTTP_UPSTREAM_IDENT_REQUEST.data, 25 + stu_strlen(channel_id) + stu_strlen(tokenstr), channel, tokenstr);

	pr->request_body.start = temp;
	pr->request_body.last = data;
	pr->request_body.end = temp + STU_HTTP_REQUEST_DEFAULT_SIZE;

	return STU_OK;
}

stu_int_t
stu_http_upstream_ident_analyze_response(stu_connection_t *c) {
	stu_http_request_t *r, *pr;
	stu_upstream_t     *u;
	stu_connection_t   *pc;
	stu_table_elt_t    *protocol;
	stu_int_t           m, n, size, extened;
	stu_str_t          *cid, *uid, *uname, channel;
	u_char             *temp, *data, opcode;
	stu_channel_t      *ch;
	stu_json_t         *idt, *sta, *idchannel, *idcid, *idcstate, *iduser, *iduid, *iduname, *idurole;
	stu_json_t         *res, *raw, *rschannel, *rsctotal, *rsuser;

	r = (stu_http_request_t *) c->data;
	u = c->upstream;
	pc = u->peer.connection;
	pr = (stu_http_request_t *) pc->data;
	protocol = r->headers_out.sec_websocket_protocol;

	if (pr->headers_out.status != STU_HTTP_OK) {
		stu_log_error(0, "Failed to load ident data: status=%ld.", pr->headers_out.status);
		return STU_ERROR;
	}

	// parse JSON string
	stu_utf8_decode(&pc->buffer.last, pr->headers_out.content_length_n);
	idt = stu_json_parse(pc->buffer.last, pr->headers_out.content_length_n);
	if (idt == NULL) {
		stu_log_error(0, "Failed to parse ident response.");
		return STU_ERROR;
	}

	sta = stu_json_get_object_item_by(idt, &STU_PROTOCOL_STATUS);
	if (sta == NULL || sta->type != STU_JSON_TYPE_BOOLEAN) {
		stu_log_error(0, "Failed to analyze ident response: item status not found, fd=%d.", c->fd);
		goto failed;
	}

	if (sta->value == FALSE) {
		stu_log_error(0, "Access denied while joining channel, fd=%d.", c->fd);
		goto failed;
	}

	idchannel = stu_json_get_object_item_by(idt, &STU_PROTOCOL_CHANNEL);
	iduser = stu_json_get_object_item_by(idt, &STU_PROTOCOL_USER);
	if (idchannel == NULL || idchannel->type != STU_JSON_TYPE_OBJECT
			|| iduser == NULL || iduser->type != STU_JSON_TYPE_OBJECT) {
		stu_log_error(0, "Failed to analyze ident response: necessary item[s] not found.");
		goto failed;
	}

	idcid = stu_json_get_object_item_by(idchannel, &STU_PROTOCOL_ID);
	idcstate = stu_json_get_object_item_by(idchannel, &STU_PROTOCOL_STATE);

	iduid = stu_json_get_object_item_by(iduser, &STU_PROTOCOL_ID);
	iduname = stu_json_get_object_item_by(iduser, &STU_PROTOCOL_NAME);
	idurole = stu_json_get_object_item_by(iduser, &STU_PROTOCOL_ROLE);

	if (idcid == NULL || idcid->type != STU_JSON_TYPE_STRING
			|| idcstate == NULL || idcstate->type != STU_JSON_TYPE_NUMBER
			|| iduid == NULL || iduid->type != STU_JSON_TYPE_STRING
			|| iduname == NULL || iduname->type != STU_JSON_TYPE_STRING
			|| idurole == NULL || idurole->type != STU_JSON_TYPE_NUMBER) {
		stu_log_error(0, "Failed to analyze ident response: necessary item[s] not found.");
		goto failed;
	}

	// get channel ID
	channel.data = stu_base_pcalloc(c->pool, r->target.len + 1);
	if (channel.data == NULL) {
		stu_log_error(0, "Failed to pcalloc memory for channel id, fd=%d.", c->fd);
		goto failed;
	}
	stu_strncpy(channel.data, r->target.data, r->target.len);
	channel.len = r->target.len;

	cid = (stu_str_t *) idcid->value;
	if (stu_strncmp(cid->data, channel.data, channel.len) != 0) {
		stu_log_error(0, "Failed to analyze ident response: channel (%s != %s) not match.", cid->data, channel.data);
		goto failed;
	}

	// reset user info
	uid = (stu_str_t *) iduid->value;

	c->user.id.data = stu_base_pcalloc(c->pool, uid->len + 1);
	if (c->user.id.data == NULL) {
		stu_log_error(0, "Failed to pcalloc memory for user id, fd=%d.", c->fd);
		goto failed;
	}

	c->user.id.len = uid->len;
	stu_strncpy(c->user.id.data, uid->data, uid->len);

	uname = (stu_str_t *) iduname->value;
	c->user.name.data = stu_base_pcalloc(c->pool, uname->len + 1);
	if (c->user.name.data == NULL) {
		stu_log_error(0, "Failed to pcalloc memory for user name, fd=%d.", c->fd);
		goto failed;
	}
	stu_strncpy(c->user.name.data, uname->data, uname->len);
	c->user.name.len = uname->len;

	m = *(stu_double_t *) idurole->value;
	stu_user_set_role(&c->user, m & 0xFF);

	// insert user into channel
	if (stu_channel_insert(cid, c) == STU_ERROR) {
		stu_log_error(0, "Failed to insert connection: fd=%d.", c->fd);
		goto failed;
	}

	ch = c->user.channel;

	// create ident response
	res = stu_json_create_object(NULL);
	raw = stu_json_create_string(&STU_PROTOCOL_RAW, STU_PROTOCOL_RAWS_IDENT.data, STU_PROTOCOL_RAWS_IDENT.len);
	rschannel = stu_json_duplicate(idchannel, TRUE);
	rsuser = stu_json_duplicate(iduser, TRUE);

	rsctotal = stu_json_create_number(&STU_PROTOCOL_TOTAL, (stu_double_t) ch->userlist.length);

	stu_json_add_item_to_object(rschannel, rsctotal);

	stu_json_add_item_to_object(res, raw);
	stu_json_add_item_to_object(res, rschannel);
	stu_json_add_item_to_object(res, rsuser);

	temp = stu_slab_calloc(stu_cycle->slab_pool, STU_HTTP_REQUEST_DEFAULT_SIZE);
	if (temp == NULL) {
		stu_log_error(0, "Failed to slab_calloc() response buffer: fd=%d.", c->fd);
		stu_json_delete(res);
		goto failed;
	}
	data = stu_json_stringify(res, (u_char *) temp + 10);

	stu_json_delete(idt);
	stu_json_delete(res);

	// finalize request
	if (protocol && stu_strncmp("binary", protocol->value.data, protocol->value.len) == 0) {
		opcode = STU_WEBSOCKET_OPCODE_BINARY;
	} else {
		opcode = STU_WEBSOCKET_OPCODE_TEXT;
	}

	u->finalize_handler_pt(c, STU_HTTP_SWITCHING_PROTOCOLS);

	size = data - temp - 10;
	data = stu_websocket_encode_frame(opcode, temp, size, &extened);

	n = send(c->fd, data, size + 2 + extened, 0);
	if (n == -1) {
		stu_log_debug(4, "Failed to send \"ident\" frame: fd=%d.", c->fd);
		stu_slab_free(stu_cycle->slab_pool, temp);
		return STU_ERROR;
	}

	stu_log_debug(4, "sent: fd=%d, bytes=%d.", c->fd, n);
	stu_slab_free(stu_cycle->slab_pool, temp);

	return STU_OK;

failed:

	stu_json_delete(idt);

	return STU_ERROR;
}
