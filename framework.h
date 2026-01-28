#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <wrl.h>
#include <winnt.h>
using Microsoft::WRL::ComPtr;
#include <winbase.h>
#include <unknwn.h>
#include <fileapi.h>
#include <profileapi.h>

#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#include <vector>
#include <format>
#include <memory>
#include <array>
#include <thread>
#include <atomic>
#include <mutex>
#include <shared_mutex>

#include "openvr_driver.h"

#define VR_DRIVER_LOG_FORMAT(...) {\
std::string _message = std::format(__VA_ARGS__); \
vr::VRDriverLog()->Log(_message.c_str()); \
}