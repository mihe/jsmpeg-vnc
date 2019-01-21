#pragma once

#pragma warning(push, 0)

// C
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>

// C++
#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Windows
#include <Windows.h>
#include <playsoundapi.h>

// DirectX
#include <dxgi1_2.h>
#include <d3d11.h>

// libwebsockets
#include <libwebsockets.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
}

#pragma warning(pop)