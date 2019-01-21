#pragma once

typedef struct {
	int x;
	int y;
	int width;
	int height;
	int size;
	int pitch;
	int type;
	void* pixels;
} grabber_pointer_t;

typedef struct {
	struct ID3D11Device* device;
	struct ID3D11DeviceContext* context;
	struct IDXGIOutputDuplication* dupl;
	struct ID3D11Texture2D* staging;

	int width;
	int height;

	void* pixels_back;
	void* pixels_front;

	grabber_pointer_t pointer;
} grabber_t;

grabber_t* grabber_create(int adapter_index);
void grabber_destroy(grabber_t* self);
bool grabber_grab(grabber_t* self, int timeout_ms);
void* grabber_swap(grabber_t* self);
