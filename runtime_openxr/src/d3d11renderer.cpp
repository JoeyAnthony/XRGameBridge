/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "d3d11renderer.h"

#include <Windows.h>

#include "d3d11compositor.h"
#include "openxr_includes.h"
#include "d3d11swapchain.h"
#include "system.h"
#include "window.h"
#include "settings.h"
#include "instance.h"

XrResult D3D11Renderer::CreateIntermediateTexture(GB_System& gb_system) {
    // Create intermediate resources for weaving render target
    // TODO Remove session parameter

    // Handle 0 is not being used by xrCreateSwapchain
    auto system_resolution = GetSystemResolution(gb_system);
    XrSwapchainCreateInfo info;
    info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    info.width = system_resolution.x;
    info.height = system_resolution.y;
    info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    info.arraySize = 1;
    info.faceCount = 1;
    info.mipCount = 1;
    info.sampleCount = 1;
    info.createFlags = 0;
    info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;
    
    try {
        intermediate_resource = D3D11ProxySwapchain::Create(&info, this, "Intermediate resource");
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

XrResult D3D11Renderer::CreateWeaver(GB_Instance* instance, GB_System& gb_system) {

    SR::SRContext* sr_context = instance->GetPlatformManager()->GetContext();
    auto system_resolution = GetSystemResolution(gb_system);
    native_weaver = new SR::PredictingDX11Weaver(*sr_context, d3d11_device.Get(), d3d11_device_context.Get(), system_resolution.x, system_resolution.y, window.GetWindowHandle());
    sr_context->initialize();
    native_weaver->setInputFrameBuffer(intermediate_resource->GetShaderResourceViews()[0].Get());

    return XR_SUCCESS;
}

XrResult D3D11Renderer::CreateSystemWindow(GB_System& gb_system) {
    if (window.TryGetExternalDisplay() != nullptr) {
       spdlog::info("Got window");
    }

    // Create debug window
    auto system_resolution = GetSystemResolution(gb_system);
    window.CreateApplicationWindow(g_runtime_settings.hInst, gb_system, system_resolution.x, system_resolution.y, true, true);
    // Debugging with non full screen mode
    //window.CreateApplicationWindow(g_runtime_settings.hInst, gb_system, 2560, 1440, true, false, true);

    window.UpdateWindow();

    return XR_SUCCESS;
}

XrResult D3D11Renderer::CreateWindowSwapchain(GB_System& gb_system) {
    // Create swapchain info for the window swapchain
    auto system_resolution = GetSystemResolution(gb_system);
    XrSwapchainCreateInfo create_info;
    create_info.width = system_resolution.x;
    create_info.height = system_resolution.y;
    create_info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    create_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

    // Create swapchain for debug window
    try {
        window_swapchain = new D3D11WindowSwapchain(this, &create_info, back_buffer_count, window.GetWindowHandle());
    }
    catch (std::exception& e) {
        LOG_RUNTIME_ERROR
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return XR_SUCCESS;
}

bool D3D11Renderer::CreateCommandLists() {
    d3d11_device->CreateDeferredContext(0, d3d11_device_context.GetAddressOf());
    d3d11_device->GetImmediateContext(d3d11_immediate_context.GetAddressOf());
    return true;
}

XrResult D3D11Renderer::CreateCompositor() {
    compositor = new D3D11Compositor();
    compositor->Initialize(this);
    return XR_SUCCESS;
}

XrResult D3D11Renderer::RenderFrameWeaving(const XrFrameEndInfo* frameEndInfo, uint32_t window_swapchain_index, const float clear_color[4]) {
    // TODO pass from caller
    auto native_resolution = GetSystemResolution(g_systems[xr_system]);

    // Set intermediate resource as render target
    ComPtr<ID3D11RenderTargetView> intermediate_rtv = intermediate_resource->GetRenderTargetViews()[0];
    d3d11_device_context->OMSetRenderTargets(1, intermediate_rtv.GetAddressOf(), nullptr);
    d3d11_device_context->ClearRenderTargetView(intermediate_rtv.Get(), clear_color);

    // Compose and draw to the intermediate resource
    compositor->ComposeImage(frameEndInfo, d3d11_device_context.Get(), native_resolution.x, native_resolution.y);


    //// Transition intermediate resource to unordered access for the weaver
    //TransitionImage(cmd_list, intermediate_resource.GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    //// Transition window swapchain to render target
    //TransitionImage(cmd_list, window_swapchain.GetImages()[window_swapchain_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);


    // Set window swapchain as render target
    ComPtr<ID3D11RenderTargetView> window_back_buffer = window_swapchain->GetRenderTargetViews()[window_swapchain_index];
    d3d11_device_context->OMSetRenderTargets(1, window_back_buffer.GetAddressOf(), nullptr);
    d3d11_device_context->ClearRenderTargetView(window_back_buffer.Get(), clear_color);


    // Set viewport for weaving to window swapchain
    D3D11_VIEWPORT view_port{ 0, 0, static_cast<float>(native_resolution.x) , static_cast<float>(native_resolution.y), 0.0f, 1.0f };
    D3D11_RECT scissor_rect{ 0, 0, static_cast<long>(native_resolution.x) , static_cast<long>(native_resolution.y) };
    d3d11_device_context->RSSetViewports(1, &view_port);
    d3d11_device_context->RSSetScissorRects(1, &scissor_rect);


    // Do weaving
    native_weaver->weave(native_resolution.x, native_resolution.y);

    // Transition to render target
    //TransitionImage(d3d11_device_context, intermediate_resource.GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);

    return XR_SUCCESS;
}

XrResult D3D11Renderer::RenderFrameSideBySide(const XrFrameEndInfo* frameEndInfo, uint32_t window_swapchain_index, const float clear_color[4]) {
    auto native_resolution = GetSystemResolution(g_systems[xr_system]);
    // Transition to render target
    //TransitionImage(cmd_list, window_swapchain.GetImages()[window_swapchain_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // Set window swapchain as render target
    //CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor_handle_to_compose = CD3DX12_CPU_DESCRIPTOR_HANDLE(window_swapchain.GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), window_swapchain_index, window_swapchain.GetRtvDescriptorSize());

    // Compose
    auto window_back_buffer = window_swapchain->GetRenderTargetViews()[window_swapchain_index];
    d3d11_device_context->OMSetRenderTargets(1, window_back_buffer.GetAddressOf(), nullptr);
    d3d11_device_context->ClearRenderTargetView(window_back_buffer.Get(), clear_color);

    // Compose and draw to the intermediate resource
    compositor->ComposeImage(frameEndInfo, d3d11_device_context.Get(), native_resolution.x, native_resolution.y);

    return XR_SUCCESS;
}

D3D11Renderer* D3D11Renderer::Create(XrSystemId systemId, const void* graphics_binding) {
    const XrGraphicsBindingD3D11KHR* d3d11_bindings = static_cast<const XrGraphicsBindingD3D11KHR*> (graphics_binding);

    { // Check validity of the device
        const IUnknown* obj = dynamic_cast<IUnknown*> (d3d11_bindings->device);
        if (!obj) {
            throw XrException(XR_ERROR_GRAPHICS_DEVICE_INVALID, "Failed to create D3D11Renderer");
        }
    }

    return new D3D11Renderer(systemId, d3d11_bindings);
}

D3D11Renderer::D3D11Renderer(XrSystemId systemId, const XrGraphicsBindingD3D11KHR* graphics_binding) {
    xr_system = systemId;
    d3d11_device = graphics_binding->device;
}

D3D11Renderer::~D3D11Renderer() {
    delete native_weaver;
    delete compositor;
    delete window_swapchain;
    window.DestroyApplicationWindow();
    delete intermediate_resource;
}

void D3D11Renderer::InitializePipeline(GB_Instance* instance) {
    auto& system = g_systems[xr_system];

    CreateCompositor();
    CreateCommandLists();
    CreateIntermediateTexture(system);

    CreateSystemWindow(system);
    CreateWeaver(instance, system);
    CreateWindowSwapchain(system);
}

XrResult D3D11Renderer::RenderFrame(const XrFrameEndInfo* frameEndInfo) {

    window.UpdateWindow();

    // Update the frame in flight.
    frame_in_flight = frame_in_flight++ % back_buffer_count;

    int32_t window_swapchain_index = window_swapchain->GetCurrentImageIndex();

    // Render weaving
    if (should_weave) {
        RenderFrameWeaving(frameEndInfo, window_swapchain_index, Renderer::clear_color);
    }
    else {
        RenderFrameSideBySide(frameEndInfo, window_swapchain_index, Renderer::clear_color);
    }

    // Transition swapchain to present
    //TransitionImage(context.Get(), window_swapchain.GetImages()[window_swapchain_index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    // Todo: maybe use split barriers at the end here instead of regular ones. Then also initialize the resources in the correct state.

    // Close command list
    ID3D11CommandList* cmd_list;
    d3d11_device_context->FinishCommandList(false, &cmd_list);
    d3d11_immediate_context->ExecuteCommandList(cmd_list, true);
    // The created command list needs to be released when done
    cmd_list->Release();

    // Present to window
    window_swapchain->PresentFrame();

    return XR_SUCCESS;
}

void D3D11Renderer::EnableSrWindow(bool enable) {
}

void D3D11Renderer::EnableWeaving(bool enable) {
}

void D3D11Renderer::Update() {
}

GraphicsBackend D3D11Renderer::GetGraphicsBackend() {
    return GraphicsBackend::D3D11;
}

ComPtr<ID3D11Device> D3D11Renderer::GetDevice() {
    return d3d11_device;
}

ComPtr<ID3D11DeviceContext>& D3D11Renderer::GetDeviceContext() {
    return d3d11_device_context;
}

Compositor* const D3D11Renderer::GetCompositor() {
    return nullptr;
}
