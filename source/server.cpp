#include "server.hpp"
#include "utilities.hpp"
#include "config.hpp"
// #include "server_debug.hpp"

#pragma comment(lib, "websockets_static.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Ws2_32.lib")

client_t* client_insert(client_t** head, lws* socket) {
	client_t* client = new client_t;
	client->socket = socket;
	client->next = *head;
	*head = client;
	return client;
}

void client_remove(client_t** head, lws* socket) {
	for (client_t** current = head; *current; current = &(*current)->next) {
		if ((*current)->socket == socket) {
			client_t* next = (*current)->next;
			delete *current;
			*current = next;
			break;
		}
	}
}

static int callback_http(struct lws*, enum lws_callback_reasons, void*, void*, size_t);
static int callback_websockets(struct lws*, enum lws_callback_reasons, void*, void*, size_t);

static lws_protocols server_protocols[] = {
	{
		"http-only", // name
		callback_http, // callback
		0, // per_session_data_size
		0 // rx_buffer_size
	},
	{
		"vnc", // name
		callback_websockets, // callback
		sizeof(int), // per_session_data_size
		CFG_FRAME_BUFFER_SIZE // rx_buffer_size
	},
	{ NULL, NULL, 0, 0 } // terminator
};

server_t* server_create(int port, size_t buffer_size) {
	server_t* self = new server_t;
	memzero(self);

	self->buffer_capacity = buffer_size;
	size_t full_buffer_size = LWS_PRE + buffer_size;
	self->buffer_with_padding = new unsigned char[full_buffer_size];
	self->buffer = &self->buffer_with_padding[LWS_PRE];

	self->port = port;
	self->clients = nullptr;

	for (lws_protocols& server_protocol : server_protocols) {
		server_protocol.user = self;
	}

	lws_set_log_level(LLL_ERR | LLL_WARN, nullptr);

	struct lws_context_creation_info info = {};
	info.max_http_header_pool = 16;
	info.port = port;
	info.gid = -1;
	info.uid = -1;
	info.user = (void*)self;
	info.protocols = server_protocols;
	info.timeout_secs = 5;
	self->context = lws_create_context(&info);

	if (!self->context) {
		server_destroy(self);
		return nullptr;
	}

	return self;
}

void server_destroy(server_t* self) {
	if (self == nullptr) {
		return;
	}

	if (self->context) {
		lws_context_destroy(self->context);
	}

	delete[] self->buffer_with_padding;
	delete self;
}

wchar_t* server_get_host_address() {
	return L"localhost";
}

wchar_t* server_get_client_address(lws* wsi) {
	static wchar_t name_buffer_w[32];
	static wchar_t ip_buffer_w[32];

	char name_buffer[32];
	char ip_buffer[32];
	lws_get_peer_addresses(
		wsi, lws_get_socket_fd(wsi),
		name_buffer, sizeof(name_buffer),
		ip_buffer, sizeof(ip_buffer));

	string_to_wide(name_buffer, name_buffer_w);
	string_to_wide(ip_buffer, ip_buffer_w);

	return ip_buffer_w;
}

void server_update(server_t* self) {
	lws_service(self->context, 0);
}

void server_send(server_t* self, lws* socket, void* data, size_t size) {
	if (size > CFG_FRAME_BUFFER_SIZE) {
		wprintf_s(L"Cant send %d bytes; exceeds buffer size (%d bytes)\n", (int)size, (int)CFG_FRAME_BUFFER_SIZE);
		return;
	}

	memcpy(self->buffer, data, size);
	lws_write(socket, self->buffer, size, LWS_WRITE_BINARY);
}

void server_broadcast(server_t* self, void* data, size_t size) {
	if (size > CFG_FRAME_BUFFER_SIZE) {
		wprintf_s(L"Cant send %d bytes; exceeds buffer size (%d bytes)\n", (int)size, (int)CFG_FRAME_BUFFER_SIZE);
		return;
	}

	memcpy(self->buffer, data, size);
	self->buffer_size = size;

	client_foreach(self->clients, client) {
		lws_callback_on_writable(client->socket);
	}
}

bool server_has_clients(server_t* self) {
	return self->clients != nullptr;
}

static int callback_websockets(
	struct lws* socket,
	enum lws_callback_reasons reason,
	void* /*user*/,
	void* in,
	size_t len) {
	server_t* self = (server_t*)lws_context_user(lws_get_context(socket));

	// wprintf_s(L"\nwebsockets: %d\n", reason);

	switch (reason) {
	case LWS_CALLBACK_ESTABLISHED: {
		client_insert(&self->clients, socket);
		if (self->on_connect) {
			self->on_connect(self, socket);
		}
	} break;

	case LWS_CALLBACK_RECEIVE: {
		if (self->on_message) {
			self->on_message(self, socket, in, len);
		}
	} break;

	case LWS_CALLBACK_SERVER_WRITEABLE: {
		lws_write(socket, self->buffer, self->buffer_size, LWS_WRITE_BINARY);
	} break;

	case LWS_CALLBACK_CLOSED: {
		client_remove(&self->clients, socket);
		if (self->on_close) {
			self->on_close(self, socket);
		}
	} break;
	}

	return 0;
}

static int callback_http(
	struct lws* wsi,
	enum lws_callback_reasons reason,
	void* /*user*/,
	void* in,
	size_t /*len*/) {
	server_t* self = (server_t*)lws_context_user(lws_get_context(wsi));

	//if (reason != LWS_CALLBACK_GET_THREAD_ID) {
	//	wprintf_s(L"\nhttp: %s\n", http_reason_str[reason]);
	//}

	switch (reason) {
	case LWS_CALLBACK_HTTP: {
		if (self && self->on_http_req && self->on_http_req(self, wsi, (char*)in)) {
			return 0;
		}
		lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, nullptr);
		return 0;
	} break;
	}

	return 0;
}
