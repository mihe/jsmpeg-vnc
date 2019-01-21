#pragma once

typedef struct {
	AVCodec* codec = nullptr;
	AVCodecContext* context = nullptr;
	AVFrame* frame = nullptr;
	void* frame_buffer = nullptr;

	int fps = 0;
	int in_width = 0;
	int in_height = 0;
	int out_width = 0;
	int out_height = 0;

	SwsContext* sws = nullptr;
} encoder_t;

encoder_t* encoder_create(int in_width, int in_height, int out_width, int out_height, int bitrate, int fps, int thread_count);
void encoder_destroy(encoder_t* self);
bool encoder_encode(encoder_t* self, void* rgb_pixels, void* out_data, size_t* out_size);
