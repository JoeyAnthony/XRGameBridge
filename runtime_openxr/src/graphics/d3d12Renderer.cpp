/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "d3d12Renderer.h"

#include "instance.h"
#include "settings.h"
#include "types.h"

XrResult D3D12Renderer::CreateIntermediateTexture(const std::shared_ptr<SRSystem>& gb_system) {
    // Create intermediate resources for weaving render target
    XrSwapchainCreateInfo info;
    info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    info.width = gb_system->RecommendedWidth();
    info.height = gb_system->RecommendedHeight();
    info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    info.arraySize = 1;
    info.faceCount = 1;
    info.mipCount = 1;
    info.sampleCount = 1;
    info.createFlags = 0;
    info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;

    try {
        intermediate_resource = D3D12ProxySwapchain::Create(&info, this, "Intermediate resource");
        return XR_SUCCESS;
    }
    catch (XrException& e) {
        return e.GetResult();
    }
    catch (std::exception& e) {
        LOG_RUNTIME_ERROR
        return XR_ERROR_RUNTIME_FAILURE;
    }
}

XrResult D3D12Renderer::CreateWeaver(const std::shared_ptr<SRSystem>& gb_system) {
    auto sr_context = gb_system->GetSrContext();
    d3d12weaver = new SR::PredictingDX12Weaver(*sr_context, d3d12_device.Get(), command_allocators[0].Get(), d3d12_command_queue.Get(), intermediate_resource->GetBuffers()[0].Get(), window_swapchain.GetImages()[0].Get(), window.GetWindowHandle());
    sr_context->initialize();
    return XR_SUCCESS;
}

XrResult D3D12Renderer::CreateSystemWindow(const std::shared_ptr<SRSystem>& gb_system) {
    if (window.TryGetExternalDisplay() != nullptr) {
        spdlog::info("Got window");
    }

    // Create debug window
    window.CreateApplicationWindow(static_cast<HMODULE>(g_runtime_settings->GethInstance()), gb_system, gb_system->PhysicalResolutionWidth(), gb_system->PhysicalResolutionHeight(), true, true);
    // Debugging with non full screen mode
    //gb_session.display.CreateApplicationWindow(XRGameBridge::g_runtime_settings.hInst, 2560, 1440, true, false, true);

    return XR_SUCCESS;
}

XrResult D3D12Renderer::CreateWindowSwapchain(const std::shared_ptr<SRSystem>& gb_system) {
    // Create swapchain info for the window swapchain
    window_swapchain.Initialize(this);

    XrSwapchainCreateInfo window_swapchain_info;
    window_swapchain_info.width = gb_system->RecommendedWidth();
    window_swapchain_info.height = gb_system->RecommendedHeight();
    window_swapchain_info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    window_swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

    // Create swapchain for debug window
    window_swapchain.CreateSwapChain(&window_swapchain_info, window.GetWindowHandle());

    return XR_SUCCESS;
}

bool D3D12Renderer::CreateCommandLists() {
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
    frame_fence_values.resize(standard_swapchain_buffer_count, 0);

    // Create fence
    HRESULT res = d3d12_device->CreateFence(fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if(FAILED(res)) {
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

    // Increase fence value so WaitForSwapchainImage will wait on it and the signal will be set for it.
    fence_value++;

    int32_t window_swapchain_index = window_swapchain.AcquireNextImage();
    auto& cmd_list = GetCommandList(frame_in_flight);
    auto& cmd_allocator = GetCommandAllocator(frame_in_flight);

    // Prepare command list
    cmd_allocator->Reset();
    cmd_list->Reset(cmd_allocator.Get(), compositor.GetDefaultPipelineState().Get());

    // Render weaving
    if (should_weave) {
        RenderFrameWeaving(frameEndInfo, cmd_list.Get(), window_swapchain, window_swapchain_index, Renderer::clear_color, fence_value);
    }
    else {
        RenderFrameSideBySide(frameEndInfo, cmd_list.Get(), window_swapchain, window_swapchain_index, Renderer::clear_color, fence_value);
    }

    // Transition swapchain to present
    TransitionImage(cmd_list.Get(), window_swapchain.GetImages()[window_swapchain_index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    // Todo: maybe use split barriers at the end here instead of regular ones. Then also initialize the resources in the correct state.

    // Close command list
    cmd_list->Close();
    ExecuteCommandList(cmd_list.Get());

    // Set frame fence value to new fence value.
    frame_fence_values[frame_in_flight] = fence_value;
    // Update the fence value when the GPU is done with execution.
    ThrowIfFailed(d3d12_command_queue->Signal(fence.Get(), fence_value));

    // Present to window
    window_swapchain.PresentFrame();

    return XR_SUCCESS;
}

void D3D12Renderer::EnableSrWindow(bool enable)
{
}

void D3D12Renderer::EnableWeaving(bool enable)
{
}

void D3D12Renderer::Update()
{
    window.UpdateWindow();
}

XrResult D3D12Renderer::RenderFrameWeaving(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, D3D12WindowSwapchain& window_swapchain, uint32_t window_swapchain_index, const float clear_color[4], uint64_t new_fence_value) {

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

    // Transition window swapchain to render target
    TransitionImage(cmd_list, window_swapchain.GetImages()[window_swapchain_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);


    // Set window swapchain as render target
    CD3DX12_CPU_DESCRIPTOR_HANDLE back_buffer_rtv_handle(window_swapchain.GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), window_swapchain_index, window_swapchain.GetRtvDescriptorSize());
    cmd_list->OMSetRenderTargets(1, &back_buffer_rtv_handle, true, nullptr);
    cmd_list->ClearRenderTargetView(back_buffer_rtv_handle, clear_color, 0, nullptr);


    // Set viewport for weaving to window swapchain
    auto gb_system = g_systems[xr_system];
    glm::ivec2 native_resolution = { gb_system->RecommendedWidth(), gb_system->RecommendedHeight()};
    D3D12_VIEWPORT view_port{ 0, 0, static_cast<float>(native_resolution.x) , static_cast<float>(native_resolution.y), 0.0f, 1.0f };
    D3D12_RECT scissor_rect{ 0, 0, static_cast<long>(native_resolution.x) , static_cast<long>(native_resolution.y) };
    cmd_list->RSSetViewports(1, &view_port);
    cmd_list->RSSetScissorRects(1, &scissor_rect);

    // Do weaving
    d3d12weaver->weave(cmd_list, native_resolution.x, native_resolution.y, 0, 0);

    // Transition to render target
    TransitionImage(cmd_list, intermediate_resource->GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);

    return XR_SUCCESS;
}

XrResult D3D12Renderer::RenderFrameSideBySide(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, D3D12WindowSwapchain& window_swapchain, uint32_t window_swapchain_index, const float clear_color[4], uint64_t new_fence_value) {

    // Transition to render target
    TransitionImage(cmd_list, window_swapchain.GetImages()[window_swapchain_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // Set window swapchain as render target
    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor_handle_to_compose = CD3DX12_CPU_DESCRIPTOR_HANDLE(window_swapchain.GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), window_swapchain_index, window_swapchain.GetRtvDescriptorSize());


    // Compose
    cmd_list->OMSetRenderTargets(1, &descriptor_handle_to_compose, true, nullptr);
    cmd_list->ClearRenderTargetView(descriptor_handle_to_compose, clear_color, 0, nullptr);
    cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Compose and draw to the intermediate resource
    compositor.ComposeImage(frameEndInfo, cmd_list, intermediate_resource->GetWidth(), intermediate_resource->GetHeight(), new_fence_value);

    return XR_SUCCESS;
}

void D3D12Renderer::ExecuteCommandList(ID3D12GraphicsCommandList* cmd_list) {
    ID3D12CommandList* lists[]{ cmd_list };
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
    // Schedule a Signal command in the queue.
    fence_value++;
    ThrowIfFailed(d3d12_command_queue->Signal(fence.Get(), fence_value));

    // Wait until the fence has been processed.
    ThrowIfFailed(fence->SetEventOnCompletion(fence_value, fence_event));
    WaitForSingleObjectEx(fence_event, INFINITE, FALSE);
}

void D3D12Renderer::ResetCommandLists() {
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

ComPtr<ID3D12Device>& D3D12Renderer::GetDevice() {
    return d3d12_device;
}

ComPtr<ID3D12CommandQueue>& D3D12Renderer::GetCommandQueue() {
    return d3d12_command_queue;
}

ComPtr<ID3D12GraphicsCommandList>& D3D12Renderer::GetCommandList(uint32_t index) {
    return command_lists[index];
}

ComPtr<ID3D12CommandAllocator>& D3D12Renderer::GetCommandAllocator(uint32_t index) {
    return command_allocators[index];
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

void D3D12Renderer::DestroyInfoQueue()
{
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

void D3D12Renderer::InitializePipeline(GB_Instance* instance) {
    CreateFenceObjects();
    CreateCommandLists();

    if (compositor.Initialize(this) == false) {
        throw XrException(XR_ERROR_RUNTIME_FAILURE, "Failed to create compositor");
    }

    std::shared_ptr<SRSystem> gb_system = std::dynamic_pointer_cast<SRSystem>(g_systems[xr_system]);
    CreateIntermediateTexture(gb_system);
    CreateSystemWindow(gb_system);
    CreateWindowSwapchain(gb_system); // Needs a window
    CreateWeaver(gb_system); // Needs command allocators created in CreateCommandLists
}

D3D12Renderer* D3D12Renderer::Create(XrSystemId systemId, const void* graphics_binding)
{
    const XrGraphicsBindingD3D12KHR* d3d12_bindings = static_cast<const XrGraphicsBindingD3D12KHR*> (graphics_binding);

    { // Check validity of the device
        const ID3D12Object* obj = dynamic_cast<ID3D12Object*> (d3d12_bindings->device);
        if (!obj) {
            throw XrException(XR_ERROR_GRAPHICS_DEVICE_INVALID, "Failed to create D3D12Renderer, graphics bining may be invalid");
        }
    }

    return new D3D12Renderer(systemId, d3d12_bindings);
}

D3D12Renderer::D3D12Renderer(XrSystemId systemId, const XrGraphicsBindingD3D12KHR* graphics_binding) {
    xr_system = systemId;
    d3d12_device = graphics_binding->device;
    d3d12_command_queue = graphics_binding->queue;
    SetupInfoQueue();
}

D3D12Renderer::~D3D12Renderer() {
    DestroyInfoQueue();

    delete intermediate_resource;
    delete d3d12weaver;
    // Destroy window
    window.DestroyApplicationWindow();
    d3d12_command_queue.Reset();
    d3d12_device.Reset();
}
