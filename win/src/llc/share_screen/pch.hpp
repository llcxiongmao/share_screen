#pragma once
#include <vector>
#include <queue>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <cassert>

extern "C" {
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <timeapi.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>

#include <uv.h>
}

#include <libavutil/hwcontext_d3d11va.h>

#include <d3d11.h>
#include <wrl.h>
#include <d3dcompiler.h>