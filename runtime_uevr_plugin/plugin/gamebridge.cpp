#include "gamebridge.hpp"

#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <openxr/openxr.h>
#include <array>
#include <string>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

bool RuntimeUevrPlugin::get_gb_functions() {
	auto res = rcomm::InitializeCommInterface();
	switch (res) {
	case CommResult::RUNTIME_NOT_FOUND:
		uevr::API::get()->log_error("[XRGB UEVR plugin] Couldn't find XRGB Runtime.");
		return false;
	case CommResult::FUNCTION_NOT_FOUND:
		uevr::API::get()->log_error("[XRGB UEVR plugin] Couldn't find XRGB functions.");
		return false;
	case CommResult::SUCCESS:
		return true;
	}
}

bool RuntimeUevrPlugin::init_rendering_pipeline() {
	const auto uevr_renderer_data = uevr::API::get()->param()->renderer;
	auto device = static_cast<ID3D12Device*>(uevr_renderer_data->device);
	auto swapchain = static_cast<IDXGISwapChain3*>(uevr_renderer_data->swapchain);

	DXGI_SWAP_CHAIN_DESC1 sw_descriptions;
	swapchain->GetDesc1(&sw_descriptions);
	num_back_buffers = sw_descriptions.BufferCount;

	command_allocators.resize(num_back_buffers);
	command_lists.resize(num_back_buffers);
	fences.resize(num_back_buffers);
	back_buffers.resize(num_back_buffers);
	fence_events.resize(num_back_buffers);
	fence_values.resize(num_back_buffers);

	if (!get_gb_functions()) {
		uevr::API::get()->log_error("[XRGB UEVR plugin] Failed to load XRGB functions from loaded XR Runtime.");
		return false;
	}

	for (int32_t i = 0; i < command_allocators.size(); i++) {
		if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators[i])))) {
			uevr::API::get()->log_error("[XRGB UEVR plugin] Failed to create command allocator.");
			return false;
		}

		std::wstring cmd_allocator_name = std::format(L"[XRGB UEVR plugin] command allocator {}", i);
		command_allocators[i]->SetName(cmd_allocator_name.c_str());

		if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators[i].Get(), nullptr, IID_PPV_ARGS(&command_lists[i])))) {
			uevr::API::get()->log_error("[[XRGB UEVR plugin] Failed to create command list.");
			return false;
		}

		std::wstring cmd_alist_name = std::format(L"[XRGB UEVR plugin] command list {}", i);
		command_lists[i]->SetName(cmd_alist_name.c_str());

		if (FAILED(command_lists[i]->Close())) {
			uevr::API::get()->log_error("[XRGB UEVR plugin] Failed to close command list.");
			return false;
		}

		if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fences[i])))) {
			uevr::API::get()->log_error("[XRGB UEVR plugin] Failed to create fence.");
			return false;
		}

		fences[i]->SetName(L"[XRGB UEVR plugin] fence");
		fence_events[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC desc{};

		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NumDescriptors = num_back_buffers;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 1;
		if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtv_heap)))) {
			return false;
		}
	}

	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtv_heap->GetCPUDescriptorHandleForHeapStart());
		uint32_t rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// Create a RTV for each frame.
		for (int32_t i = 0; i < num_back_buffers; i++) {
			if (FAILED(swapchain->GetBuffer(i, IID_PPV_ARGS(&back_buffers[i])))) {
				uevr::API::get()->log_error("[XRGB UEVR plugin] Failed to create rtv");
				return false;
			}
			device->CreateRenderTargetView(back_buffers[i].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, rtv_descriptor_size);
		}
	}
	return true;
}

void RuntimeUevrPlugin::on_dllmain() {
}

ComPtr<ID3D12CommandAllocator> cmd_allocator;

void RuntimeUevrPlugin::on_initialize() {
	uevr::API::get()->log_error("[[XRGB UEVR plugin] Loading XR Game Bridge UEVR Plugin.");

	if (!init_rendering_pipeline()) {
		uevr::API::get()->log_error("[[XRGB UEVR plugin] Failed to initialize.");
		return;
	}

	initialized = true;
}
void RuntimeUevrPlugin::on_present() {
	if (!initialized) {
		return;
	}

	const auto uevr_renderer_data = uevr::API::get()->param()->renderer;
	const auto device = (ID3D12Device*)uevr_renderer_data->device;
	const auto swapchain = (IDXGISwapChain3*)uevr_renderer_data->swapchain;
	const auto command_queue = (ID3D12CommandQueue*)uevr_renderer_data->command_queue;

	frame_index = (frame_index + 1) % num_back_buffers;

	const auto& fence = fences[frame_index];
	const auto& cmd_list = command_lists[frame_index];
	const auto& cmd_allocator = command_allocators[frame_index];
	const auto& fence_value = fence_values[frame_index];
	const auto fence_event = fence_events[frame_index];

	if (fence->GetCompletedValue() < fence_values[frame_index]) {
		WaitForSingleObject(fence_event, 2000);
		ResetEvent(fence_event);
	}

	cmd_allocator->Reset();
	cmd_list->Reset(cmd_allocator.Get(), nullptr);

	auto swapchain_index = swapchain->GetCurrentBackBufferIndex();

	// Get weaved and swapchain resources
	const auto xrSession = reinterpret_cast<XrSession>(uevr::API::get()->param()->openxr->get_xr_session());
	uint64_t resource_handle;
	if (rcomm::xrgbGetReleasedBufferHandle(xrSession, &resource_handle) != XR_SUCCESS) {
		uevr::API::get()->log_error("[[XRGB UEVR plugin] Failed to get XRGB buffer handle from runtime.");
		return;
	}
	ID3D12Resource* swapchain_resource = back_buffers[swapchain_index].Get();
	ID3D12Resource* weaved_resource = reinterpret_cast<ID3D12Resource*>(resource_handle);

	// Transition
	std::array<CD3DX12_RESOURCE_BARRIER, 2> barriers;
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(weaved_resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(swapchain_resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
	cmd_list->ResourceBarrier(2, barriers.data());

	// Copy
	cmd_list->CopyResource(swapchain_resource, weaved_resource);

	// Transtision back
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(weaved_resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(swapchain_resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	cmd_list->ResourceBarrier(2, barriers.data());

	if (FAILED(cmd_list->Close())) {
		uevr::API::get()->log_error("[[XRGB UEVR plugin] Failed to close command list.");
		return;
	}

	ID3D12CommandList* lists[]{cmd_list.Get()};
	command_queue->ExecuteCommandLists(1, lists);

	fence_values[frame_index]++;
	command_queue->Signal(fence.Get(), fence_values[frame_index]);
	fence->SetEventOnCompletion(fence_values[frame_index], fence_event);
};
void RuntimeUevrPlugin::on_device_reset() {};
bool RuntimeUevrPlugin::on_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	return true;
}

void RuntimeUevrPlugin::on_pre_slate_draw_window(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) {};
void RuntimeUevrPlugin::on_post_slate_draw_window(UEVR_FSlateRHIRendererHandle renderer, UEVR_FViewportInfoHandle viewport_info) {};
void RuntimeUevrPlugin::on_pre_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle, int view_index, float world_to_meters, UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double) {};
void RuntimeUevrPlugin::on_post_calculate_stereo_view_offset(UEVR_StereoRenderingDeviceHandle, int view_index, float world_to_meters, UEVR_Vector3f* position, UEVR_Rotatorf* rotation, bool is_double) {};

std::unique_ptr<RuntimeUevrPlugin> g_plugin{new RuntimeUevrPlugin()};

CommBackBufferDescription xrgbGetBackbufferDescription() {
	const auto uevr_renderer_data = uevr::API::get()->param()->renderer;
	auto swapchain = static_cast<IDXGISwapChain3*>(uevr_renderer_data->swapchain);
    HWND window;
	DXGI_SWAP_CHAIN_DESC1 desc;
	swapchain->GetDesc1(&desc);
    swapchain->GetHwnd(&window);
	return CommBackBufferDescription{
		.width = desc.Width,
		.height = desc.Height,
		.format = desc.Format,
        .windowHandle = window
	};
}
