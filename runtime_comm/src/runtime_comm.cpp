#include "runtime_comm.hpp"

#include <Windows.h>
#include <array>
#include <mutex>

std::mutex rcomm_mutex;
std::mutex pcomm_mutex;

#define xrgbGetReleasedBufferHandle_FUNCTION_NAME "xrgbGetReleasedBufferHandle"
typedef XrResult(XRAPI_PTR* PFN_xrgbGetReleasedBufferHandle)(XrSession session, uint64_t* resourceHandle);
PFN_xrgbGetReleasedBufferHandle xrgbGetReleasedBufferHandle_func = nullptr;

namespace rcomm {
CommResult InitializeCommInterface() {
	std::lock_guard<std::mutex> guard(rcomm_mutex);
	std::array modules = {"RuntimeOpenXR.dll", "runtimeopenxr.dll", "RuntimeOpenXRd.dll", "runtimeopenxrd.dll"};
	HMODULE xrgb_module = NULL;
	for (auto& module : modules) {
		xrgb_module = GetModuleHandle(module);
		if (xrgb_module != NULL) {
			break;
		}
	}
	if (xrgb_module == NULL) {
		return CommResult::RUNTIME_NOT_FOUND;
	}

	auto func = GetProcAddress(xrgb_module, xrgbGetReleasedBufferHandle_FUNCTION_NAME);
	if (func == nullptr) {
		return CommResult::FUNCTION_NOT_FOUND;
	}

	xrgbGetReleasedBufferHandle_func = reinterpret_cast<PFN_xrgbGetReleasedBufferHandle>(func);

	return CommResult::SUCCESS;
}

void DeinitializeCommInterface() {
	std::lock_guard<std::mutex> guard(rcomm_mutex);
	xrgbGetReleasedBufferHandle_func = nullptr;
}

XrResult xrgbGetReleasedBufferHandle(XrSession session, uint64_t* resourceHandle) {
	std::lock_guard<std::mutex> guard(rcomm_mutex);
	if (xrgbGetReleasedBufferHandle_func == nullptr) {
		return XR_ERROR_RUNTIME_FAILURE;
	}
	return xrgbGetReleasedBufferHandle_func(session, resourceHandle);
}
} // namespace rcomm

#define xrgbGetBackbufferDescription_FUNCTION_NAME "xrgbGetBackbufferDescription"
typedef CommBackBufferDescription(XRAPI_PTR* PFN_xrgbGetBackbufferDescription)();
PFN_xrgbGetBackbufferDescription xrgbGetBackbufferDescription_func = nullptr;

namespace pcomm {
CommResult InitializeCommInterface() {
	std::lock_guard<std::mutex> guard(pcomm_mutex);
	std::array modules = {"RuntimeUEVRPlugin.dll", "runtimeuevrplugin.dll", "RuntimeUEVRPlugind.dll", "runtimeuevrplugind.dll"};
	HMODULE plugin_module = NULL;
	for (auto& module : modules) {
		plugin_module = GetModuleHandle(module);
		if (plugin_module != NULL) {
			break;
		}
	}
	if (plugin_module == NULL) {
		return CommResult::RUNTIME_NOT_FOUND;
	}

	auto func = GetProcAddress(plugin_module, xrgbGetBackbufferDescription_FUNCTION_NAME);
	if (func == nullptr) {
		return CommResult::FUNCTION_NOT_FOUND;
	}

	xrgbGetBackbufferDescription_func = reinterpret_cast<PFN_xrgbGetBackbufferDescription>(func);

	return CommResult::SUCCESS;
}

void DeinitializeCommInterface() {
	std::lock_guard<std::mutex> guard(pcomm_mutex);
	xrgbGetBackbufferDescription_func = nullptr;
}

CommBackBufferDescription xrgbGetBackbufferDescription() {
	std::lock_guard<std::mutex> guard(pcomm_mutex);
	if (xrgbGetBackbufferDescription_func == nullptr) {
		return {};
	}
	return xrgbGetBackbufferDescription_func();
}

} // namespace pcomm
