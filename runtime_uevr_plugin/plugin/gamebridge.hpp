#pragma once
#include "uevr/Plugin.hpp"

#include <wrl/client.h>
#include <runtime_comm.hpp>

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

int32_t num_back_buffers = 3;

class RuntimeUevrPlugin : public uevr::Plugin {
	std::vector<ComPtr<ID3D12CommandAllocator>> command_allocators;
	std::vector<ComPtr<ID3D12GraphicsCommandList>> command_lists;
	std::vector<ComPtr<ID3D12Fence>> fences;
	std::vector<ComPtr<ID3D12Resource>> back_buffers;
	std::vector<HANDLE> fence_events = {nullptr};
	std::vector<uint64_t> fence_values = {0};

	ComPtr<ID3D12DescriptorHeap> rtv_heap;
	uint32_t frame_index = 0;

	bool initialized = false;
	bool needs_reacquire = false;

	bool get_gb_functions();
	bool init_rendering_pipeline();
	bool acquire_back_buffers();

	void on_dllmain() override;
	void on_initialize() override;
	void on_present() override;
	void on_device_reset() override;
	bool on_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

	void on_pre_slate_draw_window(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) override;
	void on_post_slate_draw_window(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) override;
	void on_pre_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle, int view_index, float world_to_meters, UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double) override;
	void on_post_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle, int view_index, float world_to_meters, UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double) override;
};

#define DllExport extern "C" __declspec(dllexport)
DllExport CommBackBufferDescription xrgbGetBackbufferDescription();
