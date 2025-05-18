#include "d3d11renderer.h"
0
#include <Windows.h>

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
    native_weaver = new SR::PredictingDX11Weaver(sr_context, d3d111_device.Get(), d3d11_device_context, static_cast<unsigned int>(system_resolution.x), static_cast<unsigned int>(system_resolution.y), window.GetWindowHandle());
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

    d3d111_device->GetImmediateContext(&d3d11_device_context);

    HRESULT res = 0;
    for (uint32_t i = 0; i < back_buffer_count; i++) {
        // Create present command allocator and command list resources
        res = d3d12_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators[i]));
        if (FAILED(res)) {
            LOG(ERROR) << "D3D12 Error, failed to create command allocator";
            ThrowIfFailed(res);
            return false;
        }

        // TODO use initial pipeline state here later. First check if it works without.
        res = d3d12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators[i].Get(), compositor.GetDefaultPipelineState().Get(), IID_PPV_ARGS(&command_lists[i]));
        if (FAILED(res)) {
            LOG(ERROR) << "D3D12 Error, Failed creating DX12 renderer command list";
            ThrowIfFailed(res);
            return false;
        }

        std::wstring name = std::format(L"DX12Renderer Command List {}", i);
        command_lists[i]->SetName(name.c_str());
        command_lists[i]->Close();
    }
}

XrResult D3D11Renderer::RenderFrameWeaving(const XrFrameEndInfo* frameEndInfo, uint32_t window_swapchain_index, const float clear_color[4], uint64_t new_fence_value)
{
    return XrResult();
}

XrResult D3D11Renderer::RenderFrameSideBySide(const XrFrameEndInfo* frameEndInfo, uint32_t window_swapchain_index, const float clear_color[4], uint64_t new_fence_value)
{
    return XrResult();
}

XrResult D3D11Renderer::CreateD3D11Renderer(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding, D3D11Renderer* renderer) {
    const XrGraphicsBindingD3D11KHR* d3d11_bindings = static_cast<const XrGraphicsBindingD3D11KHR*> (graphics_binding);

    { // Check validity of the device
        const ID3D12Object* obj = dynamic_cast<ID3D12Object*> (d3d11_bindings->device);
        if (!obj) {
            return XR_ERROR_GRAPHICS_DEVICE_INVALID;
        }
    }

    renderer = new D3D11Renderer(instance, systemId, d3d11_bindings);

    return XR_SUCCESS;
}

D3D11Renderer::D3D11Renderer(GB_Instance* instance, XrSystemId systemId, const XrGraphicsBindingD3D11KHR* graphics_binding) {
    xr_system = systemId;
    d3d111_device = graphics_binding->device;
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
    return d3d111_device;
}
