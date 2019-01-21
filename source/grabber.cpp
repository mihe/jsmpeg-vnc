#include "grabber.hpp"
#include "utilities.hpp"

#pragma comment(lib, "d3d11.lib")

#define DPL_ASSERT_HR(...)
#define DPL_ASSERT(...)

template<typename T>
HRESULT grabber_query_iface(IUnknown* unk, T** iface) {
	return unk->QueryInterface(__uuidof(T), (void**)iface);
}

void grabber_init_d3d(grabber_t* self) {
	HRESULT hr = S_OK;

	UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if _DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // _DEBUG

	static constexpr D3D_FEATURE_LEVEL feature_levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0};

	for (D3D_FEATURE_LEVEL desired_level : feature_levels) {
		D3D_FEATURE_LEVEL feature_level;
		hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			NULL,
			flags,
			&desired_level,
			1,
			D3D11_SDK_VERSION,
			&self->device,
			&feature_level,
			&self->context);

		if (hr == DXGI_ERROR_UNSUPPORTED) {
			continue;
		} else {
			break;
		}
	}

	DPL_ASSERT_HR(hr, "Failed to initialize D3D");

	CD3D11_TEXTURE2D_DESC staging_desc(
		DXGI_FORMAT_B8G8R8A8_UNORM,
		(UINT)self->width,
		(UINT)self->height,
		1,
		1,
		0,
		D3D11_USAGE_STAGING,
		D3D11_CPU_ACCESS_READ);

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = (UINT)self->width;
	desc.Height = (UINT)self->height;
	desc.ArraySize = 1;
	desc.MipLevels = 1;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	hr = self->device->CreateTexture2D(&staging_desc, nullptr, &self->staging);
	DPL_ASSERT_HR(hr, "Failed to create input texture");
}

void grabber_init_dupl(grabber_t* self, int adapter_index) {
	HRESULT hr = S_OK;

	IDXGIDevice* dxgi_device = nullptr;
	hr = grabber_query_iface(self->device, &dxgi_device);
	DPL_ASSERT_HR(hr, "Failed to query for DXGI Device");

	IDXGIAdapter* dxgi_adapter = nullptr;
	hr = dxgi_device->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgi_adapter);
	DPL_ASSERT_HR(hr, "Failed to get parent DXGI Adapter");

	IDXGIOutput* dxgi_output = nullptr;
	hr = dxgi_adapter->EnumOutputs(adapter_index, &dxgi_output);
	DPL_ASSERT_HR(hr, "Failed to get DXGI output");

	IDXGIOutput1* dxgi_output1 = nullptr;
	hr = grabber_query_iface(dxgi_output, &dxgi_output1);
	DPL_ASSERT_HR(hr, "Failed to query for DXGI Output1");

	hr = dxgi_output1->DuplicateOutput(self->device, &self->dupl);
	if (FAILED(hr)) {
		if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
			DPL_ASSERT(false, "There is already the maximum number of "
							  "applications using the Desktop Duplication API "
							  "running, please close one of those applications "
							  "and then try again.");
		}

		DPL_ASSERT(false, "Failed to duplicate output");
	}
}

void grabber_get_pointer(grabber_t* self, DXGI_OUTDUPL_FRAME_INFO* frame_info) {
	if (frame_info->LastMouseUpdateTime.QuadPart == 0) {
		return;
	}

	self->pointer.x = frame_info->PointerPosition.Position.x;
	self->pointer.y = frame_info->PointerPosition.Position.y;

	if (!frame_info->PointerPosition.Visible || frame_info->PointerShapeBufferSize == 0) {
		return;
	}

	if (frame_info->PointerShapeBufferSize > (UINT)self->pointer.size) {
		if (self->pointer.pixels) {
			delete[] self->pointer.pixels;
			memzero(&self->pointer);
		}

		self->pointer.size = frame_info->PointerShapeBufferSize;
		self->pointer.pixels = new BYTE[self->pointer.size];
	}

	if (!self->pointer.pixels) {
		return;
	}

	memzero(self->pointer.pixels, self->pointer.size);

	DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info = {};
	UINT size_required = 0;
	HRESULT hr = self->dupl->GetFramePointerShape(
		frame_info->PointerShapeBufferSize,
		self->pointer.pixels,
		&size_required,
		&shape_info);

	if (FAILED(hr)) {
		if (hr != DXGI_ERROR_ACCESS_LOST) {
			DPL_ASSERT_HR(hr, L"Failed to get frame pointer shape in DUPLICATIONMANAGER");
		}

		delete[] self->pointer.pixels;
		memzero(&self->pointer);
	}

	self->pointer.width = shape_info.Width;
	self->pointer.height = shape_info.Height;
	self->pointer.pitch = shape_info.Pitch;
	self->pointer.type = shape_info.Type;
}

ID3D11Texture2D* grabber_get_texture(grabber_t* self, UINT timeout_ms) {
	IDXGIResource* desktop_resources = nullptr;
	DXGI_OUTDUPL_FRAME_INFO frame_info = {};
	HRESULT hr = self->dupl->AcquireNextFrame(timeout_ms, &frame_info, &desktop_resources);
	if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
		return nullptr;
	}

	DPL_ASSERT_HR(hr, "Failed to acquire next frame");

	grabber_get_pointer(self, &frame_info);

	ID3D11Texture2D* desktop_texture = nullptr;
	hr = grabber_query_iface(desktop_resources, &desktop_texture);
	DPL_ASSERT_HR(hr, "Failed to query for D3D texture");

	return desktop_texture;
}

grabber_t* grabber_create(int adapter_index) {
	grabber_t* self = new grabber_t;
	memzero(self);

	RECT rect = {};
	GetClientRect(GetDesktopWindow(), &rect);

	self->width = rect.right - rect.left;
	self->height = rect.bottom - rect.top;

	self->pixels_back = new uint8_t[self->width * self->height * 4];
	self->pixels_front = new uint8_t[self->width * self->height * 4];

	grabber_init_d3d(self);
	grabber_init_dupl(self, adapter_index);

	return self;
}

void grabber_destroy(grabber_t* self) {
	if (self == nullptr) {
		return;
	}

	delete[] self->pixels_front;
	delete[] self->pixels_back;
	self->dupl->Release();
	self->staging->Release();
	self->context->Release();
	self->device->Release();
	delete self;
}

void grabber_argb_unshift(uint32_t rgba, uint8_t& a, uint8_t& r, uint8_t& g, uint8_t& b) {
	a = (rgba >> 24) & 0xFF;
	r = (rgba >> 16) & 0xFF;
	g = (rgba >> 8) & 0xFF;
	b = (rgba >> 0) & 0xFF;
}

uint32_t grabber_argb_shift(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
	return (a << 24) | (r << 16) | (g << 8) | b;
}

uint32_t grabber_alpha_blend(uint32_t background, uint32_t foreground) {
	uint8_t b_a, b_r, b_g, b_b;
	grabber_argb_unshift(background, b_a, b_r, b_g, b_b);
	b_a = 255;

	uint8_t f_a, f_r, f_g, f_b;
	grabber_argb_unshift(foreground, f_a, f_r, f_g, f_b);

	uint8_t out_a = f_a + (b_a * (255 - f_a) / 255);
	uint8_t out_r = uint8_t((f_r * f_a / 255) + (b_r * b_a * (255 - f_a) / (255 * 255)));
	uint8_t out_g = uint8_t((f_g * f_a / 255) + (b_g * b_a * (255 - f_a) / (255 * 255)));
	uint8_t out_b = uint8_t((f_b * f_a / 255) + (b_b * b_a * (255 - f_a) / (255 * 255)));

	return grabber_argb_shift(out_a, out_r, out_g, out_b);
}

bool grabber_is_inside(grabber_t* self, size_t x, int y) {
	return x >= 0 && x < self->width && y >= 0 && y < self->height;
}

void grabber_copy_pointer_color(grabber_t* self, bool masked) {
	uint8_t* dst = (uint8_t*)self->pixels_back;
	uint8_t* ptr = (uint8_t*)self->pointer.pixels;

	const size_t dst_pitch = self->width * 4;

	const int ptr_x = std::max(0, std::min(self->width, self->pointer.x));
	const int ptr_y = std::max(0, std::min(self->height, self->pointer.y));

	for (int y = 0; y < self->pointer.height; ++y) {
		for (int x = 0; x < self->pointer.width; ++x) {
			const int dst_x = ptr_x + x;
			const int dst_y = ptr_y + y;

			if (!grabber_is_inside(self, dst_x, dst_y)) {
				continue;
			}

			const size_t dst_offset = (dst_y * dst_pitch) + (dst_x * 4);
			uint32_t& dst_px = *(uint32_t*)(dst + dst_offset);

			const size_t ptr_offset = (y * self->pointer.pitch) + (x * 4);
			const uint32_t ptr_px = *(uint32_t*)(ptr + ptr_offset);

			if (masked) {
				if (ptr_px & 0xFF000000) {
					dst_px = (dst_px ^ ptr_px) | 0xFF000000;
				} else {
					dst_px = ptr_px;
				}
			} else {
				dst_px = grabber_alpha_blend(dst_px, ptr_px);
			}
		}
	}
}

uint32_t grabber_monochrome_magic(grabber_t* self, int x, int y, uint32_t dst_px) {
	const uint8_t* ptr = (uint8_t*)self->pointer.pixels;

	const int height = self->pointer.height;
	const int pitch = self->pointer.pitch;

	const uint8_t mask = 0x80 >> (x % 8);
	const uint8_t and_mask = ptr[x / 8 + pitch * y] & mask;
	const uint8_t xor_mask = ptr[x / 8 + pitch * (y + height / 2)] & mask;

	const uint32_t and_mask_32 = and_mask ? 0xFFFFFFFF : 0xFF000000;
	const uint32_t xor_mask_32 = xor_mask ? 0x00FFFFFF : 0x00000000;

	return (dst_px & and_mask_32) ^ xor_mask_32;
}

void grabber_copy_pointer_monochrome(grabber_t* self) {
	uint8_t* dst = (uint8_t*)self->pixels_back;

	const size_t dst_pitch = self->width * 4;

	const int ptr_x = std::max(0, std::min(self->width, self->pointer.x));
	const int ptr_y = std::max(0, std::min(self->height, self->pointer.y));

	for (int y = 0; y < self->pointer.height / 2; ++y) {
		for (int x = 0; x < self->pointer.width; ++x) {
			const int dst_x = ptr_x + x;
			const int dst_y = ptr_y + y;

			if (!grabber_is_inside(self, dst_x, dst_y)) {
				continue;
			}

			const size_t dst_offset = (dst_y * dst_pitch) + (dst_x * 4);
			uint32_t& dst_px = *(uint32_t*)(dst + dst_offset);

			dst_px = grabber_monochrome_magic(self, x, y, dst_px);
		}
	}
}

bool grabber_grab(grabber_t* self, int timeout_ms) {
	HRESULT hr = S_OK;

	ID3D11Texture2D* desktop_texture = grabber_get_texture(self, (UINT)timeout_ms);
	if (!desktop_texture) {
		return false;
	}

	self->context->CopyResource(self->staging, desktop_texture);
	self->context->Flush();

	hr = self->dupl->ReleaseFrame();
	DPL_ASSERT_HR(hr, "Failed to release frame");

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	hr = self->context->Map(self->staging, 0, D3D11_MAP_READ, 0, &mapped);
	DPL_ASSERT_HR(hr, "Failed to map texture");

	const uint8_t* src = (uint8_t*)mapped.pData;
	uint8_t* dst = (uint8_t*)self->pixels_back;

	const size_t src_pitch = mapped.RowPitch;
	const size_t dst_pitch = self->width * 4;

	if (src_pitch == dst_pitch) {
		memcpy(dst, src, self->width * self->height * 4);
	} else {
		for (size_t line = 0; line < self->height; ++line) {
			const size_t src_offset = line * src_pitch;
			const size_t dst_offset = line * dst_pitch;
			memcpy(dst + dst_offset, src + src_offset, dst_pitch);
		}
	}

	self->context->Unmap(self->staging, 0);

	switch (self->pointer.type) {
	case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR: {
		grabber_copy_pointer_color(self, false);
	} break;
	case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR: {
		grabber_copy_pointer_color(self, true);
	} break;
	case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME: {
		grabber_copy_pointer_monochrome(self);
	} break;
	}

	return true;
}

void* grabber_swap(grabber_t* self) {
	std::swap(self->pixels_back, self->pixels_front);
	return self->pixels_front;
}
