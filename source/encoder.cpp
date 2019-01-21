#include "encoder.hpp"
#include "utilities.hpp"
#include "timer.hpp"

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")

encoder_t* encoder_create(int in_width, int in_height, int out_width, int out_height, int bitrate, int fps, int thread_count) {
	encoder_t* self = new encoder_t;
	self->fps = fps;
	self->in_width = in_width;
	self->in_height = in_height;
	self->out_width = out_width;
	self->out_height = out_height;

	av_log_set_level(AV_LOG_ERROR);

	avcodec_register_all();
	self->codec = avcodec_find_encoder(AV_CODEC_ID_MPEG1VIDEO);

	self->context = avcodec_alloc_context3(self->codec);
	self->context->dct_algo = FF_DCT_FASTINT;
	self->context->bit_rate = bitrate;
	self->context->width = out_width;
	self->context->height = out_height;
	self->context->time_base.num = 1;
	self->context->time_base.den = fps;
	self->context->gop_size = 250;
	self->context->max_b_frames = 0;
	self->context->pix_fmt = AV_PIX_FMT_YUV420P;
	self->context->thread_count = thread_count;

	avcodec_open2(self->context, self->codec, nullptr);

	self->frame = av_frame_alloc();
	self->frame->format = AV_PIX_FMT_YUV420P;
	self->frame->width = out_width;
	self->frame->height = out_height;
	self->frame->pts = 0;

	const int frame_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, out_width, out_height, 1);
	self->frame_buffer = new uint8_t[frame_size];

	av_image_fill_arrays(
		self->frame->data,
		self->frame->linesize,
		(uint8_t*)self->frame_buffer,
		AV_PIX_FMT_YUV420P,
		out_width, out_height, 1);

	self->sws = sws_getContext(
		in_width, in_height, AV_PIX_FMT_BGRA,
		out_width, out_height, AV_PIX_FMT_YUV420P,
		SWS_FAST_BILINEAR,
		nullptr, nullptr, nullptr);

	return self;
}

void encoder_destroy(encoder_t* self) {
	if (self == nullptr) {
		return;
	}

	sws_freeContext(self->sws);
	avcodec_close(self->context);
	av_free(self->context);
	av_free(self->frame);
	delete[] self->frame_buffer;
	delete self;
}

bool encoder_encode(encoder_t* self, void* rgb_pixels, void* out_data, size_t* out_size) {
	size_t available_size = *out_size;
	*out_size = 0;

	static int64_t start = av_gettime();
	int64_t now = av_gettime();
	int64_t elapsed = now - start;

	int64_t pts = av_rescale_q(elapsed, {1, 1'000'000}, self->context->time_base);
	static int64_t last_pts = 0;
	if (pts <= last_pts) {
		return false;
	} else {
		self->frame->pts = pts * pts;
		last_pts = pts;
	}

	uint8_t* in_data[1] = {(uint8_t*)rgb_pixels};
	int in_linesize[1] = {self->in_width * 4};
	sws_scale(
		self->sws,
		in_data,
		in_linesize,
		0,
		self->in_height,
		self->frame->data,
		self->frame->linesize);

	AVPacket packet;
	av_init_packet(&packet);

	int success = avcodec_send_frame(self->context, self->frame);

	while (success == 0) {
		success = avcodec_receive_packet(self->context, &packet);
		if (success >= 0) {
			if (*out_size + packet.size > available_size) {
				wprintf_s(L"Frame too large for buffer (current: %zu, needed: %d)\n", available_size, packet.size);
			}

			memcpy((uint8_t*)out_data + *out_size, packet.data, packet.size);
			*out_size += packet.size;
		}

		av_packet_unref(&packet);
	}

	return true;
}
