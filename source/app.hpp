#pragma once

#include "encoder.hpp"
#include "grabber.hpp"
#include "server.hpp"

typedef struct {
	encoder_t* encoder = nullptr;
	grabber_t* grabber = nullptr;
	server_t* server = nullptr;

	float mouse_speed = 0.f;
} app_t;

app_t* app_create(int port, int bitrate, int fps, int thread_count, int width, int height, int adapter);
void app_destroy(app_t* self);
void app_run(app_t* self);

int app_on_http_req(app_t* self, lws* socket, char* request);
void app_on_connect(app_t* self, lws* socket);
void app_on_close(app_t* self, lws* socket);
void app_on_message(app_t* self, lws* socket, void* data, size_t len);
