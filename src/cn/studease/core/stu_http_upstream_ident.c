/*
 * stu_upstream_ident.c
 *
 *  Created on: 2017-3-16
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_int_t  stu_preview_auto_id = 1001;

stu_str_t  STU_HTTP_UPSTREAM_IDENT_PARAM_CHANNEL = stu_string("channel");
stu_str_t  STU_HTTP_UPSTREAM_IDENT_PARAM_TOKEN = stu_string("token");

stu_str_t  STU_HTTP_UPSTREAM_IDENT_RESPONSE = stu_string(
		"{\"raw\":\"ident\",\"user\":{\"id\":\"%ld\",\"name\":\"%s\",\"role\":%d},\"channel\":{\"id\":\"%s\",\"state\":%d,\"total\":%lu}}"
	);


stu_int_t
stu_http_upstream_ident_generate_request(stu_connection_t *c) {
	stu_http_request_t *r, *pr;
	stu_upstream_t     *u;
	stu_connection_t   *pc;
	stu_str_t           arg;
	u_char             *p;
	stu_json_t         *res, *rschannel, *rstoken;

	r = (stu_http_request_t *) c->data;
	u = c->upstream;
	pc = u->peer.connection;
	pr = (stu_http_request_t *) pc->data;

	stu_str_null(&arg);
	if (stu_http_arg(r, STU_HTTP_UPSTREAM_IDENT_PARAM_TOKEN.data, STU_HTTP_UPSTREAM_IDENT_PARAM_TOKEN.len, &arg) != STU_OK) {
		stu_log_debug(4, "Param \"%s\" not found while sending upstream %s: c->fd=%d, u->fd=%d.",
				STU_HTTP_UPSTREAM_IDENT_PARAM_TOKEN.data, u->server->name.data, c->fd, pc->fd);
	}

	if (pr->request_body.start == NULL) {
		pr->request_body.start = stu_base_palloc(pc->pool, STU_HTTP_REQUEST_DEFAULT_SIZE);
		if (pr->request_body.start == NULL) {
			stu_log_error(0, "Failed to palloc() request body: fd=%d.", c->fd);
			return STU_ERROR;
		}

		pr->request_body.end = pr->request_body.start + STU_HTTP_REQUEST_DEFAULT_SIZE;
	}

	switch (u->server->method) {
	case STU_HTTP_GET:
		p = stu_sprintf(pr->request_body.start, "?%s=", STU_HTTP_UPSTREAM_IDENT_PARAM_CHANNEL.data);
		p = stu_strncpy(p, r->target.data, r->target.len);
		p = stu_sprintf(p, "&%s=", STU_HTTP_UPSTREAM_IDENT_PARAM_TOKEN.data);
		p = stu_strncpy(p, arg.data, arg.len);
		break;
	case STU_HTTP_POST:
		res = stu_json_create_object(NULL);
		rschannel = stu_json_create_string(&STU_HTTP_UPSTREAM_IDENT_PARAM_CHANNEL, r->target.data, r->target.len);
		rstoken = stu_json_create_string(&STU_HTTP_UPSTREAM_IDENT_PARAM_TOKEN, arg.data, arg.len);
		stu_json_add_item_to_object(res, rschannel);
		stu_json_add_item_to_object(res, rstoken);

		p = stu_json_stringify(res, pr->request_body.start);
		*p = '\0';

		stu_json_delete(res);
		break;
	default:
		stu_log_error(0, "Http method unknown while generating upstream request: fd=%d, method=%hd.", c->fd, u->server->method);
		return STU_ERROR;
	}

	pr->request_body.last = p;

	stu_http_upstream_generate_request(c);

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
	u_char             *data, temp[STU_HTTP_REQUEST_DEFAULT_SIZE], opcode;
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

	data = stu_json_stringify(res, (u_char *) temp + 10);
	*data = '\0';

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
		return STU_ERROR;
	}

	stu_log_debug(4, "sent: fd=%d, bytes=%d.", c->fd, n);

	return STU_OK;

failed:

	stu_json_delete(idt);

	return STU_ERROR;
}
