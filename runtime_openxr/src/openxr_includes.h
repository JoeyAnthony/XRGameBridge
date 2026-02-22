/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

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

// OpenXR headers
#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>
#include <openxr/openxr_platform.h>

// Debug
#include "debug.h"

#ifdef WIN32
    // Declare functions here so system.h doesn't have to be included in openxr_includes.h
    XrResult xrConvertWin32PerformanceCounterToTimeKHR(XrInstance instance, const LARGE_INTEGER* performanceCounter, XrTime* time);
    XrResult xrConvertTimeToWin32PerformanceCounterKHR(XrInstance instance, XrTime time, LARGE_INTEGER* performanceCounter);
#endif
