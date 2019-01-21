#include "app.hpp"
#include "timer.hpp"
#include "utilities.hpp"
#include "config.hpp"

typedef enum : unsigned int {
	jsmpeg_frame_type_video = 0xFA010000,
	jsmpeg_frame_type_audio = 0xFB010000
} jsmpeg_frame_type_t;

#pragma warning(push)
#pragma warning(disable : 4200) // nonstandard extension
typedef struct {
	jsmpeg_frame_type_t type;
	int size;
	char data[];
} jsmpeg_frame_t;
#pragma warning(pop)

typedef struct {
	unsigned char magic[4];
	unsigned short width;
	unsigned short height;
} jsmpeg_header_t;

int swap_int32(int in) {
	return ((in >> 24) & 0xff) | ((in << 8) & 0xff0000) | ((in >> 8) & 0xff00) | ((in << 24) & 0xff000000);
}

int swap_int16(int in) {
	return ((in >> 8) & 0xff) | ((in << 8) & 0xff00);
}

void on_connect(server_t* server, lws* socket) {
	app_on_connect((app_t*)server->user, socket);
}

int on_http_req(server_t* server, lws* socket, char* request) {
	return app_on_http_req((app_t*)server->user, socket, request);
}

void on_close(server_t* server, lws* socket) {
	app_on_close((app_t*)server->user, socket);
}

app_t* app_create(int port, int bitrate, int fps, int thread_count, int width, int height, int adapter) {
	app_t* self = new app_t;
	self->mouse_speed = CFG_MOUSE_SPEED;
	self->grabber = grabber_create(adapter);

	if (!width) {
		width = self->grabber->width;
	}

	if (!height) {
		height = self->grabber->height;
	}

	if (!bitrate) {
		bitrate = width * 1500;
	}

	self->encoder = encoder_create(
		self->grabber->width,
		self->grabber->height,
		width,
		height,
		bitrate,
		fps,
		thread_count);

	self->server = server_create(port, CFG_FRAME_BUFFER_SIZE);
	if (!self->server) {
		wprintf_s(L"Error: could not create Server; try using another port\n");
		return nullptr;
	}

	self->server->on_connect = on_connect;
	self->server->on_http_req = on_http_req;
	self->server->on_message = nullptr;
	self->server->on_close = on_close;
	self->server->user = self; // Set the app as user data, so we can access it in callbacks

	return self;
}

void app_destroy(app_t* self) {
	if (self == nullptr) {
		return;
	}

	encoder_destroy(self->encoder);
	grabber_destroy(self->grabber);
	server_destroy(self->server);
	delete self;
}

int app_on_http_req(app_t* /*self*/, lws* socket, char* request) {
	// printf_s("\nhttp request: %s\n", request);

	if (strcmp(request, "/") == 0) {
		lws_serve_http_file(socket, "client/index.html", "text/html; charset=utf-8", nullptr, 0);
		return true;
	}

	return false;
}

void app_on_connect(app_t* self, lws* socket) {
	wprintf_s(L"\nClient connected: %s\n", server_get_client_address(socket));
	PlaySoundW(L"connected.wav", NULL, SND_FILENAME);

	jsmpeg_header_t header = {
		{'j', 's', 'm', 'p'},
		(unsigned short)swap_int16(self->encoder->out_width),
		(unsigned short)swap_int16(self->encoder->out_height)};

	server_send(self->server, socket, &header, sizeof(header));
}

void app_on_close(app_t* /*self*/, lws* socket) {
	wprintf_s(L"\nClient disconnected: %s\n", server_get_client_address(socket));
	PlaySoundW(L"disconnected.wav", NULL, SND_FILENAME);
}

void app_run(app_t* self) {
	jsmpeg_frame_t* frame = (jsmpeg_frame_t*)new uint8_t[CFG_FRAME_BUFFER_SIZE];
	frame->type = jsmpeg_frame_type_video;
	frame->size = 0;

	static const double target_time = 1000.0 / self->encoder->fps;

	timer_t* frame_timer = timer_create();
	timer_t* print_timer = timer_create();

	auto event_encoded = CreateEvent(nullptr, false, true, nullptr);
	auto event_grabbed = CreateEvent(nullptr, false, false, nullptr);

	void* pixels = nullptr;

	std::thread([&] {
		while (true) {
			while (!server_has_clients(self->server)) {
				Sleep(100);
			}

			while (!grabber_grab(self->grabber, -1)) {
				continue;
			}

			WaitForSingleObject(event_encoded, INFINITE);
			pixels = grabber_swap(self->grabber);
			SetEvent(event_grabbed);
		}
	}).detach();

	while (true) {
		double grab_time = 0;
		double encode_time = 0;
		double serve_time = 0;
		size_t encoded_size = 0;

		if (server_has_clients(self->server)) {
			timer_measure(grab_time) {
				WaitForSingleObject(event_grabbed, INFINITE);
			}

			timer_measure(encode_time) {
				encoded_size = CFG_FRAME_BUFFER_SIZE - sizeof(jsmpeg_frame_t);
				encoder_encode(self->encoder, pixels, frame->data, &encoded_size);
			}

			SetEvent(event_encoded);
		} else {
			Sleep(100);
		}

		timer_measure(serve_time) {
			if (encoded_size > 0) {
				frame->size = swap_int32((int)(sizeof(jsmpeg_frame_t) + encoded_size));
				server_broadcast(self->server, frame, sizeof(jsmpeg_frame_t) + encoded_size);
			}

			server_update(self->server);
		}

		double delta_time = timer_elapsed(frame_timer);
		while (target_time - delta_time > 2.0) {
			Sleep(1);
			delta_time = timer_elapsed(frame_timer);
		}

		if (server_has_clients(self->server) && timer_elapsed(print_timer) > 100.0) {
			int fps = (int)(1.0 / (delta_time / 1000.0));
			wprintf_s(L"FPS: %3d (grabbing:%6.2fms, encoding:%6.2fms, serving:%6.2fms)\r", fps, grab_time, encode_time, serve_time);
			timer_reset(print_timer);
		}

		timer_reset(frame_timer);
	}

	timer_destroy(print_timer);
	timer_destroy(frame_timer);
	delete[] frame;
}
