#pragma once

typedef struct server_client_t {
	lws* socket;
	struct server_client_t* next;
} client_t;

client_t* client_insert(client_t** head, lws* socket);
void client_remove(client_t** head, lws* socket);
#define client_foreach(HEAD, CLIENT) for (client_t* CLIENT = HEAD; CLIENT; CLIENT = CLIENT->next)

typedef struct server_t {
	lws_context* context;
	size_t buffer_capacity;
	size_t buffer_size;
	unsigned char* buffer_with_padding;
	unsigned char* buffer;
	void* user;

	int port;
	server_client_t* clients;

	void (*on_connect)(server_t* server, lws* wsi);
	void (*on_message)(server_t* server, lws* wsi, void* in, size_t len);
	void (*on_close)(server_t* server, lws* wsi);
	int (*on_http_req)(server_t* server, lws* wsi, char* request);
} server_t;

server_t* server_create(int port, size_t buffer_size);
void server_destroy(server_t* self);
wchar_t* server_get_host_address();
wchar_t* server_get_client_address(lws* wsi);
void server_update(server_t* self);
void server_send(server_t* self, lws* socket, void* data, size_t size);
void server_broadcast(server_t* self, void* data, size_t size);
bool server_has_clients(server_t* self);
