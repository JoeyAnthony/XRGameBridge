/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "d3d12Renderer.h"

#include "instance.h"
#include "settings.h"
#include "swapchain.h"
#include "types.h"

#include <runtime_comm.hpp>

XrResult D3D12Renderer::CreateIntermediateTexture(const D3D12ProxySwapchain* back_buffer_swapchain) {
    // Initialize plugin communication
    auto res = pcomm::InitializeCommInterface();
    switch (res) {
        case CommResult::RUNTIME_NOT_FOUND:
            spdlog::error("Couldn't find XRGB Runtime. Falling back to regular texture settings.");
            break;
        case CommResult::FUNCTION_NOT_FOUND:
            spdlog::error("Couldn't find plugin functions. Falling back to regular texture settings.");
            break;
        case CommResult::SUCCESS:
            spdlog::info("Plugin for external window rendering found.");
            break;
    }

    try {
        // Create intermediate resources for weaving render target
        XrSwapchainCreateInfo intermediate_info;
        intermediate_info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        intermediate_info.width = back_buffer_swapchain->GetWidth();
        intermediate_info.height = back_buffer_swapchain->GetHeight();
        intermediate_info.format = back_buffer_swapchain->GetFormat();
        intermediate_info.arraySize = 1;
        intermediate_info.faceCount = 1;
        intermediate_info.mipCount = 1;
        intermediate_info.sampleCount = 1;
        intermediate_info.createFlags = 0;
        intermediate_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;

        intermediate_resource = std::unique_ptr<D3D12ProxySwapchain>(D3D12ProxySwapchain::Create(&intermediate_info, this, "Intermediate resource", 1));

        XrSwapchainCreateInfo weaved_info;
        weaved_info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        weaved_info.width = back_buffer_swapchain->GetWidth();
        weaved_info.height = back_buffer_swapchain->GetHeight();
        weaved_info.format = back_buffer_swapchain->GetFormat();
        weaved_info.arraySize = 1;
        weaved_info.faceCount = 1;
        weaved_info.mipCount = 1;
        weaved_info.sampleCount = 1;
        weaved_info.createFlags = 0;
        weaved_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;

        auto desc = pcomm::xrgbGetBackbufferDescription();
        if (desc.width != 0 || desc.height != 0) {
            weaved_info.width = desc.width;
            weaved_info.height = desc.height;
            weaved_info.format = desc.format;

            use_debug_window = false;
        }
        else {
            use_debug_window = true;
        }

        int64_t rtv_format = -1;
        if (weaved_info.format != back_buffer_swapchain->GetFormat() && IsSrgbFormat(back_buffer_swapchain->GetFormat())) {
            rtv_format = back_buffer_swapchain->GetFormat();
        }
        weaved_resource = std::unique_ptr<D3D12ProxySwapchain>(D3D12ProxySwapchain::Create(&weaved_info, this, "Weaved resource", 1, rtv_format));

        return XR_SUCCESS;
    }
    catch (XrException &e) {
        return e.GetResult();
    }
    catch (std::exception &e) {
        spdlog::error("RUNTIME FAILURE func: {} ln: {} err: {}", __func__, __LINE__, e.what());
        return XR_ERROR_RUNTIME_FAILURE;
    }
}

XrResult D3D12Renderer::CreateWeaver(const D3D12ProxySwapchain* back_buffer_swapchain, const std::shared_ptr<SRSystem> &gb_system) {
    spdlog::info("Creating DX12 weaver");

    HWND used_window;
    if (use_debug_window) {
        used_window = window.GetWindowHandle();
    }
    else {
        used_window = static_cast<HWND>(pcomm::xrgbGetBackbufferDescription().windowHandle);
    }

    auto sr_context = gb_system->GetSrContext();
    SR::CreateDX12Weaver(sr_context, d3d12_device.Get(), used_window, &d3d12weaver);
    d3d12weaver->setInputViewTexture(intermediate_resource->GetBuffers()[0].Get(), back_buffer_swapchain->GetWidth(), back_buffer_swapchain->GetHeight(), static_cast<DXGI_FORMAT>(back_buffer_swapchain->GetFormat()));

    if (use_debug_window) {
        current_weaver_output_format = back_buffer_swapchain->GetFormat();
        d3d12weaver->setOutputFormat(static_cast<DXGI_FORMAT>(current_weaver_output_format));
        d3d12weaver->setShaderSRGBConversion(false, false);
    }
    else {
        current_weaver_output_format = weaved_resource->GetFormat();
        d3d12weaver->setOutputFormat(static_cast<DXGI_FORMAT>(current_weaver_output_format));

        // Set in-shader sRGB conversion if necessary. Each flag simply mirrors whether that specific
        // resource's own format is SRGB-tagged - the weaver's SRV/RTV bindings match the real format
        // of whatever they're reading/writing, so this should never disagree with what hardware is
        // already doing at that boundary (see the compose-shader conversion discussion: correction
        // only belongs where the format tag and the data don't already agree).

        const bool input_conversion = IsSrgbFormat(back_buffer_swapchain->GetFormat());
        const bool output_conversion = IsSrgbFormat(current_weaver_output_format);
        d3d12weaver->setShaderSRGBConversion(input_conversion, output_conversion);
    }

    sr_context->initialize();
    return XR_SUCCESS;
}

XrResult D3D12Renderer::CreateSystemWindow(const std::shared_ptr<SRSystem> &gb_system) {
    // Create debug window
    window.CreateApplicationWindow(static_cast<HMODULE>(g_runtime_settings->GethInstance()), gb_system, gb_system->PhysicalResolutionWidth(), gb_system->PhysicalResolutionHeight(), true, true);
    // Debugging with non full screen mode
    // gb_session.display.CreateApplicationWindow(XRGameBridge::g_runtime_settings.hInst, 2560, 1440, true, false, true);

    return XR_SUCCESS;
}

XrResult D3D12Renderer::CreateWindowSwapchain(const D3D12ProxySwapchain* back_buffer_swapchain, const std::shared_ptr<SRSystem> &gb_system) {
    // Create swapchain info for the window swapchain
    window_swapchain.Initialize(this);

    XrSwapchainCreateInfo window_swapchain_info;
    window_swapchain_info.width = gb_system->RecommendedWidth();
    window_swapchain_info.height = gb_system->RecommendedHeight();
    window_swapchain_info.format = back_buffer_swapchain->GetFormat();
    window_swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

    // Create swapchain for debug window
    window_swapchain.CreateSwapChain(&window_swapchain_info, window.GetWindowHandle());

    return XR_SUCCESS;
}

bool D3D12Renderer::CreateCommandLists() {
    spdlog::info("Creating command lists");
    command_allocators.resize(standard_swapchain_buffer_count);
    command_lists.resize(standard_swapchain_buffer_count);

    HRESULT res = 0;
    for (uint32_t i = 0; i < standard_swapchain_buffer_count; i++) {
        // Create present command allocator and command list resources
        res = d3d12_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators[i]));
        if (FAILED(res)) {
            spdlog::error("D3D12 Error, failed to create command allocator");
            ThrowIfFailed(res);
            return false;
        }

        // TODO use initial pipeline state here later. First check if it works without.
        res = d3d12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators[i].Get(), compositor.GetDefaultPipelineState().Get(), IID_PPV_ARGS(&command_lists[i]));
        if (FAILED(res)) {
            spdlog::error("D3D12 Error, Failed creating DX12 renderer command list");
            ThrowIfFailed(res);
            return false;
        }

        std::wstring name = std::format(L"DX12Renderer Command List {}", i);
        command_lists[i]->SetName(name.c_str());
        command_lists[i]->Close();
    }
    return true;
}

bool D3D12Renderer::CreateFenceObjects() {
    spdlog::info("Creating fences");
    frame_fence_values.resize(standard_swapchain_buffer_count, 0);

    // Create fence
    HRESULT res = d3d12_device->CreateFence(fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(res)) {
        throw XrException(XR_ERROR_RUNTIME_FAILURE, "Failed to create fence object");
    }
    // Create an event handle to use for frame synchronization.
    fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fence_event == nullptr) {
        HRESULT_FROM_WIN32(GetLastError());
        return false;
    }
    return true;
}

bool D3D12Renderer::DestroyFences() {
    spdlog::info("Destroying fences");
    const uint64_t last_fence_value = fence_value;
    const uint64_t lastCompletedFence = fence->GetCompletedValue();

    // Signal and increment the fence value.
    if (FAILED(d3d12_command_queue->Signal(fence.Get(), fence_value))) {
        spdlog::error("Failed signaling fence on destroy");
        return false;
    }
    fence_value++;

    // Wait until the previous frame is finished.
    if (lastCompletedFence < last_fence_value) {
        if (FAILED(fence->SetEventOnCompletion(last_fence_value, fence_event))) {
            spdlog::error("Failed setting completion event on destroy");
            return false;
        }
        WaitForSingleObject(fence_event, INFINITE);
    }

    ResetCommandLists();

    CloseHandle(fence_event);

    return true;
}

XrResult D3D12Renderer::RenderFrame(const XrFrameEndInfo* frameEndInfo) {
    // Update the frame in flight.
    frame_in_flight = (frame_in_flight + 1) % standard_swapchain_buffer_count;

    // If the next frame in flight is still rendering wait until it is ready.
    if (fence->GetCompletedValue() < frame_fence_values[frame_in_flight]) {
        // Trigger an event when the fence value is updated.
        ThrowIfFailed(fence->SetEventOnCompletion(frame_fence_values[frame_in_flight], fence_event));
        // Wait for the event to trigger.
        WaitForSingleObjectEx(fence_event, INFINITE, FALSE);
    }

    auto &cmd_list = GetCommandList(frame_in_flight);
    auto &cmd_allocator = GetCommandAllocator(frame_in_flight);

    // Prepare command list
    cmd_allocator->Reset();
    cmd_list->Reset(cmd_allocator.Get(), compositor.GetDefaultPipelineState().Get());

    // Initialize to size of the debug window since we may not be able to get the size of the game window.
    int32_t window_width = weaved_resource->GetWidth(), window_height = weaved_resource->GetHeight();
    // For the weaved resource we want to prefer the size of the game window. If we can't get that size we use the debug window instead.
    auto desc = pcomm::xrgbGetBackbufferDescription();
    if (desc.width != 0 || desc.height != 0) {
        window_width = desc.width;
        window_height = desc.height;
    }

    if (use_debug_window) {
        // Get size of the debug window
        window_width = window_swapchain.GetWidth();
        window_height = window_swapchain.GetHeight();
        window.ConsumePendingResize(window_width, window_height);
    }


    // Resize weaved resource on change
    if (!weaved_resource->Resize(window_width, window_height)) {
        spdlog::error("D3D12 Error, Failed to resize weaved resource to window size");
        cmd_list->Close();
        return XR_ERROR_RUNTIME_FAILURE;
    }

    // Compose and weave or compose and blit to resources
    if (should_weave) {
        RenderFrameWeaving(frameEndInfo, cmd_list.Get(), Renderer::clear_color, fence_value);
    }
    else {
        RenderFrameSideBySide(frameEndInfo, cmd_list.Get(), Renderer::clear_color, fence_value);
    }

    if (use_debug_window) {
        // Resize swapchain on change
        if (!window_swapchain.Resize(window_width, window_height)) {
            spdlog::error("D3D12 Error, Failed to resize window swapchain to window size");
            cmd_list->Close();
            return XR_SUCCESS;
        }

        RenderToDebugWindow(cmd_list.Get());
    }

    // Todo: maybe use split barriers at the end here instead of regular ones. Then also initialize the resources in the correct state.

    // Close command list
    cmd_list->Close();
    ExecuteCommandList(cmd_list.Get());

    // Set frame fence value to new fence value.
    frame_fence_values[frame_in_flight] = fence_value;
    // Update the fence value when the GPU is done with execution.
    ThrowIfFailed(d3d12_command_queue->Signal(fence.Get(), fence_value));

    // Increase fence value so WaitForSwapchainImage will wait on it and the signal will be set for it.
    fence_value++;

    // Present to window
    window_swapchain.PresentFrame();

    return XR_SUCCESS;
}

void D3D12Renderer::EnableSrWindow(bool enable) {
}

void D3D12Renderer::EnableWeaving(bool enable) {
    should_weave = enable;
}

void D3D12Renderer::Update() {
    window.UpdateWindow();
}

XrResult D3D12Renderer::RenderFrameWeaving(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, const float clear_color[4], uint64_t new_fence_value) {
    // Set intermediate resource as render target
    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor_handle_to_compose = CD3DX12_CPU_DESCRIPTOR_HANDLE(intermediate_resource->GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), 0, intermediate_resource->GetRtvDescriptorSize());

    // Compose
    cmd_list->OMSetRenderTargets(1, &descriptor_handle_to_compose, true, nullptr);
    cmd_list->ClearRenderTargetView(descriptor_handle_to_compose, clear_color, 0, nullptr);
    cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Compose and draw to the intermediate resource
    compositor.ComposeImage(frameEndInfo, cmd_list, intermediate_resource->GetWidth(), intermediate_resource->GetHeight(), new_fence_value);

    // Transition intermediate resource to unordered access for the weaver
    TransitionImage(cmd_list, intermediate_resource->GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // The weaved_resource resource is already a render target and doesn't need to be anything else

    // Set weaved_resource swapchain as render target
    CD3DX12_CPU_DESCRIPTOR_HANDLE weaved_resource_rtv_handle(weaved_resource->GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), 0, weaved_resource->GetRtvDescriptorSize());
    cmd_list->OMSetRenderTargets(1, &weaved_resource_rtv_handle, true, nullptr);
    cmd_list->ClearRenderTargetView(weaved_resource_rtv_handle, clear_color, 0, nullptr);

    // Set viewport for weaving to window swapchain

    glm::ivec2 native_resolution = {weaved_resource->GetWidth(), weaved_resource->GetHeight()};
    D3D12_VIEWPORT view_port {0, 0, static_cast<float>(native_resolution.x), static_cast<float>(native_resolution.y), 0.0f, 1.0f};
    D3D12_RECT scissor_rect {0, 0, static_cast<long>(native_resolution.x), static_cast<long>(native_resolution.y)};
    cmd_list->RSSetViewports(1, &view_port);
    cmd_list->RSSetScissorRects(1, &scissor_rect);

    d3d12weaver->setCommandList(cmd_list);
    d3d12weaver->setViewport(view_port);
    d3d12weaver->setScissorRect(scissor_rect);
    d3d12weaver->weave();

    // Transition to render target
    TransitionImage(cmd_list, intermediate_resource->GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);

    return XR_SUCCESS;
}

XrResult D3D12Renderer::RenderFrameSideBySide(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, const float clear_color[4], uint64_t new_fence_value) {
    // Set intermediate resource as render target
    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor_handle_to_compose = CD3DX12_CPU_DESCRIPTOR_HANDLE(intermediate_resource->GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), 0, intermediate_resource->GetRtvDescriptorSize());

    // Compose
    cmd_list->OMSetRenderTargets(1, &descriptor_handle_to_compose, true, nullptr);
    cmd_list->ClearRenderTargetView(descriptor_handle_to_compose, clear_color, 0, nullptr);
    cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Compose and draw to the intermediate resource
    compositor.ComposeImage(frameEndInfo, cmd_list, intermediate_resource->GetWidth(), intermediate_resource->GetHeight(), new_fence_value);

    // Transition intermediate resource to unordered access for the weaver
    TransitionImage(cmd_list, intermediate_resource->GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // The weaved_resource resource is already a render target and doesn't need to be anything else

    // Set weaved_resource swapchain as render target
    CD3DX12_CPU_DESCRIPTOR_HANDLE weaved_resource_rtv_handle(weaved_resource->GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), 0, weaved_resource->GetRtvDescriptorSize());
    cmd_list->OMSetRenderTargets(1, &weaved_resource_rtv_handle, true, nullptr);
    cmd_list->ClearRenderTargetView(weaved_resource_rtv_handle, clear_color, 0, nullptr);

    compositor.BlitToBoundTarget(cmd_list, intermediate_resource->GetSrvHeap().Get(), weaved_resource->GetWidth(), weaved_resource->GetHeight());

    // Transition to render target
    TransitionImage(cmd_list, intermediate_resource->GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

    return XR_SUCCESS;
}

void D3D12Renderer::RenderToDebugWindow(ID3D12GraphicsCommandList* cmd_list) {
    // int32_t window_swapchain_index = window_swapchain.AcquireNextImage();

    //// Transition
    // auto resource = window_swapchain.GetImages()[window_swapchain_index].Get();
    // std::array<CD3DX12_RESOURCE_BARRIER, 2> barriers;
    // barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(intermediate_resource->GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    // barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    // cmd_list->ResourceBarrier(2, barriers.data());

    //// Set weaved_resource swapchain as render target
    // CD3DX12_CPU_DESCRIPTOR_HANDLE window_swapchain_resource_rtv_handle(window_swapchain.GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), window_swapchain_index, window_swapchain.GetRtvDescriptorSize());
    // cmd_list->OMSetRenderTargets(1, &window_swapchain_resource_rtv_handle, true, nullptr);
    // cmd_list->ClearRenderTargetView(window_swapchain_resource_rtv_handle, clear_color, 0, nullptr);

    //// Set viewport for weaving to window swapchain
    // glm::ivec2 native_resolution = {window_swapchain.GetWidth(), window_swapchain.GetHeight()};
    // D3D12_VIEWPORT view_port {0, 0, static_cast<float>(native_resolution.x), static_cast<float>(native_resolution.y), 0.0f, 1.0f};
    // D3D12_RECT scissor_rect {0, 0, static_cast<long>(native_resolution.x), static_cast<long>(native_resolution.y)};
    // cmd_list->RSSetViewports(1, &view_port);
    // cmd_list->RSSetScissorRects(1, &scissor_rect);

    // d3d12weaver->setCommandList(cmd_list);
    // d3d12weaver->setViewport(view_port);
    // d3d12weaver->setScissorRect(scissor_rect);
    // d3d12weaver->weave();

    //// Transtision back
    // barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(intermediate_resource->GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
    // barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    // cmd_list->ResourceBarrier(2, barriers.data());

    int32_t window_swapchain_index = window_swapchain.AcquireNextImage();

    // Transition
    auto resource = window_swapchain.GetImages()[window_swapchain_index].Get();
    std::array<CD3DX12_RESOURCE_BARRIER, 2> barriers;
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(weaved_resource->GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
    cmd_list->ResourceBarrier(2, barriers.data());

    // Do copy logic
    cmd_list->CopyResource(resource, weaved_resource->GetBuffers()[0].Get());

    // Transtision back
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(weaved_resource->GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    cmd_list->ResourceBarrier(2, barriers.data());
}

void D3D12Renderer::ExecuteCommandList(ID3D12GraphicsCommandList* cmd_list) {
    ID3D12CommandList* lists[] {cmd_list};
    d3d12_command_queue->ExecuteCommandLists(1, lists);
}

void D3D12Renderer::TransitionImage(ID3D12GraphicsCommandList* cmd_list, ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after) {
    if (state_before == state_after) {
        return;
    }

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, state_before, state_after);
    cmd_list->ResourceBarrier(1, &barrier);
}

GraphicsBackend D3D12Renderer::GetGraphicsBackend() {
    return GraphicsBackend::D3D12;
}

Compositor* const D3D12Renderer::GetCompositor() {
    return &compositor;
}

bool D3D12Renderer::IsSrgbFormat(int64_t format) {
    const auto dxgi_format = static_cast<DXGI_FORMAT>(format);
    return dxgi_format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || dxgi_format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
}

XrResult D3D12Renderer::WaitFenceSwapchain(uint32_t value, XrDuration timeout) {
    // If the next frame in flight is still rendering wait until it is ready.
    uint64_t completed_value = fence->GetCompletedValue();
    if (completed_value < value) {
        ThrowIfFailed(fence->SetEventOnCompletion(value, fence_event));
        HRESULT res = WaitForSingleObjectEx(fence_event, ch::duration_cast<ch::milliseconds>(ch::nanoseconds(timeout)).count(), FALSE);
        if (res == WAIT_TIMEOUT) {
            return XR_TIMEOUT_EXPIRED;
        }
    }
    return XR_SUCCESS;
}

void D3D12Renderer::WaitForGpu() {
    spdlog::info("Waiting for GPU to finish frame");
    // Schedule a Signal command in the queue.
    fence_value++;
    ThrowIfFailed(d3d12_command_queue->Signal(fence.Get(), fence_value));

    // Wait until the fence has been processed.
    ThrowIfFailed(fence->SetEventOnCompletion(fence_value, fence_event));
    WaitForSingleObjectEx(fence_event, INFINITE, FALSE);
}

void D3D12Renderer::ResetCommandLists() {
    spdlog::info("Resetting all command lists");
    // Reset command lists
    WaitForGpu();

    for (uint32_t i = 0; i < command_lists.size(); i++) {
        // Right now initializing with pipeline state opaque
        command_lists[i]->Reset(command_allocators[i].Get(), compositor.GetDefaultPipelineState().Get());
        command_lists[i]->Close();
        command_allocators[i]->Reset();
    }
}

uint32_t D3D12Renderer::GetFrameFenceValue(uint32_t frameNumber) {
    return frame_fence_values[frameNumber];
}

ComPtr<ID3D12Device> &D3D12Renderer::GetDevice() {
    return d3d12_device;
}

ComPtr<ID3D12CommandQueue> &D3D12Renderer::GetCommandQueue() {
    return d3d12_command_queue;
}

ComPtr<ID3D12GraphicsCommandList> &D3D12Renderer::GetCommandList(uint32_t index) {
    return command_lists[index];
}

ComPtr<ID3D12CommandAllocator> &D3D12Renderer::GetCommandAllocator(uint32_t index) {
    return command_allocators[index];
}

uint64_t D3D12Renderer::GetWeavedBufferHandle() {
    if (weaved_resource != nullptr) {
        return reinterpret_cast<uint64_t>(weaved_resource.get()->GetBuffers()[0].Get());
    }
    return 0;
}

void D3D12Renderer::SetupInfoQueue() {
#ifdef ENABLE_D3D12_DEBUG_LAYERS
    if (d3d12_device->QueryInterface(IID_PPV_ARGS(&info_queue)) == S_OK) {
        info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        ComPtr<ID3D12InfoQueue1> info_queue1;
        if (SUCCEEDED(info_queue->QueryInterface(IID_PPV_ARGS(&info_queue1)))) {
            info_queue1->RegisterMessageCallback(D3D12MessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, this, &m_infoqueue_callback_cookie);
            spdlog::info("D3D12 Info Queue message callback registered");
        }
    }
#endif
}

void D3D12Renderer::DestroyInfoQueue() {
    // Unregister callback first because the D3D12 debug layer may still fire messages during device teardown,
    // and our callback holds a raw pointer to D3D12Renderer (this) which would be dangling if we reset the
    // device before unregistering.
#ifdef ENABLE_D3D12_DEBUG_LAYERS
    if (m_infoqueue_callback_cookie != 0) {
        ComPtr<ID3D12InfoQueue1> info_queue1;
        if (SUCCEEDED(d3d12_device->QueryInterface(IID_PPV_ARGS(&info_queue1)))) {
            info_queue1->UnregisterMessageCallback(m_infoqueue_callback_cookie);
            spdlog::info("D3D12 Info Queue message callback unregistered");
        }
    }
#endif
}

void D3D12Renderer::D3D12MessageCallback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR description, void* pContext) {
#ifdef ENABLE_D3D12_DEBUG_LAYERS
    D3D12Renderer* renderer = static_cast<D3D12Renderer*>(pContext);
    (void)renderer;

    auto level = spdlog::level::info;
    switch (severity) {
        case D3D12_MESSAGE_SEVERITY_CORRUPTION:
            level = spdlog::level::critical;
            break;
        case D3D12_MESSAGE_SEVERITY_ERROR:
            level = spdlog::level::err;
            break;
        case D3D12_MESSAGE_SEVERITY_WARNING:
            level = spdlog::level::warn;
            break;
        case D3D12_MESSAGE_SEVERITY_INFO:
            level = spdlog::level::info;
            break;
        case D3D12_MESSAGE_SEVERITY_MESSAGE:
            level = spdlog::level::info;
            break;
    }

    std::string message = description;

    if (!message.empty()) {
        spdlog::log(level, "[Info Queue] [ID {}] {}", static_cast<int32_t>(id), message);
    }
    else {
        spdlog::log(level, "[Info Queue] [ID {}] <no description>", static_cast<int32_t>(id));
    }
#endif
}

void D3D12Renderer::InitializePipeline(XrSwapchain swapchain) {
    spdlog::info("Initializing rendering pipeline for DX12");
    auto proxy_swapchain = reinterpret_cast<D3D12ProxySwapchain*>(g_proxy_swapchains[swapchain]);
    std::shared_ptr<SRSystem> gb_system = std::dynamic_pointer_cast<SRSystem>(g_systems[xr_system]);

    CreateIntermediateTexture(proxy_swapchain); // Should be created first

    if (use_debug_window) {
        CreateSystemWindow(gb_system);
        CreateWindowSwapchain(proxy_swapchain, gb_system); // Needs a window
    }

    CreateWeaver(proxy_swapchain, gb_system); // Needs SR context and weaved resource format

    if (compositor.Initialize(this, intermediate_resource->GetFormat()) == false) {
        throw XrException(XR_ERROR_RUNTIME_FAILURE, "Failed to create compositor");
    }
}

D3D12Renderer* D3D12Renderer::Create(XrSystemId systemId, const void* graphics_binding) {
    const XrGraphicsBindingD3D12KHR* d3d12_bindings = static_cast<const XrGraphicsBindingD3D12KHR*>(graphics_binding);

    { // Check validity of the device
        ComPtr<ID3D12Object> obj;
        if (!d3d12_bindings->device || FAILED(d3d12_bindings->device->QueryInterface(IID_PPV_ARGS(&obj)))) {
            throw XrException(XR_ERROR_GRAPHICS_DEVICE_INVALID, "Failed to create D3D12Renderer, graphics binding may be invalid");
        }
    }

    return new D3D12Renderer(systemId, d3d12_bindings);
}

D3D12Renderer::D3D12Renderer(XrSystemId systemId, const XrGraphicsBindingD3D12KHR* graphics_binding) {
    xr_system = systemId;
    d3d12_device = graphics_binding->device;
    d3d12_command_queue = graphics_binding->queue;
    SetupInfoQueue();

    CreateFenceObjects();
    CreateCommandLists();
}

D3D12Renderer::~D3D12Renderer() {
    DestroyInfoQueue();
    d3d12weaver->destroy();
    d3d12weaver = nullptr;
    // Destroy window
    window.DestroyApplicationWindow();
}
