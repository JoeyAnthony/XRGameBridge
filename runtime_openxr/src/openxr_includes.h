#pragma once

// OpenXR Windows and DirectX

//#ifndef WIN32_LEAN_AND_MEAN
//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
//#endif

#include <windows.h>
#include <directx/d3d12.h>
#include <dxgi1_6.h>
#include <directx/d3dx12.h>

// DX11
#include <d3d11.h>
#include <D3Dcommon.h>
#pragma comment( lib, "dxguid.lib")

// COM
#include <wrl/client.h>
#include <winerror.h>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib, "dxgi.lib")

template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

inline void ThrowIfFailed(HRESULT hr) {
#ifdef  _DEBUG
    if (FAILED(hr)) {
        // Set a breakpoint on this line to catch DirectX API errors
        throw std::exception();
    }
#else

#endif

}

// OpenXR headers
#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>
#include <openxr/openxr_platform.h>

// Extra
#include "easylogging++.h"
#include <stdexcept>

#ifdef WIN32
    // Declare functions here so system.h doesn't have to be included in openxr_includes.h
    XrResult xrConvertWin32PerformanceCounterToTimeKHR(XrInstance instance, const LARGE_INTEGER* performanceCounter, XrTime* time);
    XrResult xrConvertTimeToWin32PerformanceCounterKHR(XrInstance instance, XrTime time, LARGE_INTEGER* performanceCounter);
#endif

class XrException : public std::runtime_error {
    const XrResult xr_result;
public:
    XrException(std::string message, XrResult result) : xr_result(result), std::runtime_error(message) {
        LOG(ERROR) << "XrException was thrown: " << message << "\n";
    }

    XrResult GetResult() {
        return xr_result;
    }
};