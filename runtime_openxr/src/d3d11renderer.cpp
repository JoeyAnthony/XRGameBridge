#include "d3d11renderer.h"

#include <Windows.h>

#include "d3d11compositor.h"
#include "openxr_includes.h"
#include "d3d11swapchain.h"
#include "system.h"
#include "window.h"
#include "settings.h"
#include "instance.h"

SR::SRContext* sr_context;
SR::PredictingDX11Weaver* native_weaver;

XrResult D3D11Renderer::CreateIntermediateTexture(GB_System& gb_system) {
    // Create intermediate resources for weaving render target
    // TODO Remove session parameter

    // Handle 0 is not being used by xrCreateSwapchain
    auto system_resolution = GetSystemResolution(gb_system);
    XrSwapchainCreateInfo info;
    info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    info.width = system_resolution.x;
    info.height = system_resolution.y;
    info.arraySize = 1;
    info.faceCount = 1;
    info.mipCount = 1;
    info.sampleCount = 1;
    info.createFlags = 0;
    info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    
    XrResult result = D3D11ProxySwapchain::CreateD3D11ProxySwapchain(&info, this, intermediate_resource);
    if (result != XR_SUCCESS) {
        return result;
    }

    intermediate_resource->CreateResources(&info, L"Intermediate resource");

    return XR_SUCCESS;
}

XrResult D3D11Renderer::CreateWeaver(GB_Instance* instance, GB_System& gb_system) {

    SR::SRContext* sr_context = instance->GetPlatformManager()->GetContext();
    auto system_resolution = GetSystemResolution(gb_system);
    native_weaver = new SR::PredictingDX11Weaver(*sr_context, d3d11_device.Get(), d3d11_device_context, system_resolution.x, system_resolution.y, window.GetWindowHandle());
    sr_context->initialize();

    return XR_SUCCESS;
}

XrResult D3D11Renderer::CreateSystemWindow(GB_System& gb_system) {
    if (window.TryGetExternalDisplay() != nullptr) {
        LOG(INFO) << "Got window";
    }

    // Create debug window
    auto system_resolution = GetSystemResolution(gb_system);
    window.CreateApplicationWindow(g_runtime_settings.hInst, gb_system, system_resolution.x, system_resolution.y, true, true);
    // Debugging with non full screen mode
    //gb_session.display.CreateApplicationWindow(XRGameBridge::g_runtime_settings.hInst, 2560, 1440, true, false, true);

    return XR_SUCCESS;
}

XrResult D3D11Renderer::CreateWindowSwapchain(GB_System& gb_system) {
    // Create swapchain info for the window swapchain
    window_swapchain.Initialize(this);

    auto system_resolution = GetSystemResolution(gb_system);
    XrSwapchainCreateInfo window_swapchain_info;
    window_swapchain_info.width = system_resolution.x;
    window_swapchain_info.height = system_resolution.y;
    window_swapchain_info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    window_swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

    // Create swapchain for debug window
    window_swapchain.CreateSwapChain(&window_swapchain_info, window.GetWindowHandle());

    return XR_SUCCESS;
}

bool D3D11Renderer::CreateCommandLists() {
    d3d11_device->GetImmediateContext(&d3d11_device_context);
    return true;
}

XrResult D3D11Renderer::RenderFrameWeaving(const XrFrameEndInfo* frameEndInfo, uint32_t window_swapchain_index, const float clear_color[4]) {
    auto native_resolution = GetSystemResolution(g_systems[xr_system]);

    // Set intermediate resource as render target
    

    // Compose
    auto& intermediate_rtv = intermediate_resource->GetRenderTargetViews()[0];
    d3d11_device_context->OMSetRenderTargets(1, &intermediate_rtv, nullptr);
    d3d11_device_context->ClearRenderTargetView(intermediate_rtv.Get(), clear_color);
    d3d11_device_context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Compose and draw to the intermediate resource
    compositor->ComposeImage(frameEndInfo, d3d11_device_context, native_resolution.x, native_resolution.y);


    //// Transition intermediate resource to unordered access for the weaver
    //TransitionImage(cmd_list, intermediate_resource.GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    //// Transition window swapchain to render target
    //TransitionImage(cmd_list, window_swapchain.GetImages()[window_swapchain_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);


    // Set window swapchain as render target
    CD3DX12_CPU_DESCRIPTOR_HANDLE back_buffer_rtv_handle(window_swapchain.GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), window_swapchain_index, window_swapchain.GetRtvDescriptorSize());
    d3d11_device_context->OMSetRenderTargets(1, &back_buffer_rtv_handle, nullptr);
    d3d11_device_context->ClearRenderTargetView(back_buffer_rtv_handle, clear_color);


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

    // Transition to render target
    TransitionImage(cmd_list, window_swapchain.GetImages()[window_swapchain_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // Set window swapchain as render target
    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor_handle_to_compose = CD3DX12_CPU_DESCRIPTOR_HANDLE(window_swapchain.GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), window_swapchain_index, window_swapchain.GetRtvDescriptorSize());


    // Compose
    d3d11_device_context->OMSetRenderTargets(1, &descriptor_handle_to_compose, true, nullptr);
    d3d11_device_context->ClearRenderTargetView(descriptor_handle_to_compose, clear_color, 0, nullptr);
    d3d11_device_context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Compose and draw to the intermediate resource
    compositor->ComposeImage(frameEndInfo, d3d11_device_context, native_resolution.x, native_resolution.y);

    return XR_SUCCESS;
}

XrResult D3D11Renderer::CreateD3D11Renderer(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding, D3D11Renderer* renderer) {
    const XrGraphicsBindingD3D11KHR* d3d11_bindings = static_cast<const XrGraphicsBindingD3D11KHR*> (graphics_binding);

    { // Check validity of the device
        const ID3D11Object* obj = dynamic_cast<ID3D11Object*> (d3d11_bindings->device);
        if (!obj) {
            return XR_ERROR_GRAPHICS_DEVICE_INVALID;
        }
    }

    renderer = new D3D11Renderer(instance, systemId, d3d11_bindings);

    return XR_SUCCESS;
}

D3D11Renderer::D3D11Renderer(GB_Instance* instance, XrSystemId systemId, const XrGraphicsBindingD3D11KHR* graphics_binding) {
    xr_system = systemId;
    d3d11_device = graphics_binding->device;
}

XrResult D3D11Renderer::Initialize(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding) {
}

XrResult D3D11Renderer::RenderFrame(const XrFrameEndInfo* frameEndInfo) {
}

void D3D11Renderer::EnableSrWindow(bool enable) {
}

void D3D11Renderer::EnableWeaving(bool enable) {
}

void D3D11Renderer::Update() {
}

GraphicsBackend D3D11Renderer::GetGraphicsBackend() {
}

Compositor* const D3D11Renderer::GetCompositor() {
}

ComPtr<ID3D11Device> D3D11Renderer::GetDevice() {
    return d3d11_device;
}
