#pragma once
#include "xrrendering.h"
#include "window.h"

class GB_System;
class D3D11Compositor;
class D3D11ProxySwapchain;
class D3D11WindowSwapchain;

class D3D11Renderer : public Renderer {
    uint64_t xr_system;
    ComPtr<ID3D11Device> d3d11_device;
    ComPtr<ID3D11DeviceContext> d3d11_device_context;
    ComPtr<ID3D11DeviceContext> d3d11_immediate_context;
    bool should_weave = true;

    D3D11Compositor* compositor;
    D3D11WindowSwapchain* window_swapchain;
    D3D11ProxySwapchain* intermediate_resource;
    // Windowing
    GameBridgeWindow window;

    uint8_t frame_in_flight = 0;

    // Initialization
    XrResult CreateIntermediateTexture(GB_System& gb_system);
    XrResult CreateWeaver(GB_Instance* instance, GB_System& gb_system);
    XrResult CreateSystemWindow(GB_System& gb_system);
    XrResult CreateWindowSwapchain(GB_System& gb_system);
    bool CreateCommandLists();
    XrResult CreateCompositor();

    // Pipeline functions
    XrResult RenderFrameWeaving(const XrFrameEndInfo* frameEndInfo, uint32_t window_swapchain_index, const float clear_color[4]);
    XrResult RenderFrameSideBySide(const XrFrameEndInfo* frameEndInfo, uint32_t window_swapchain_index, const float clear_color[4]);

public:
    static D3D11Renderer* Create(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding);

    D3D11Renderer() = delete;
    explicit D3D11Renderer(GB_Instance* instance, XrSystemId systemId, const XrGraphicsBindingD3D11KHR* graphics_binding);

    XrResult RenderFrame(const XrFrameEndInfo* frameEndInfo) override;
    void EnableSrWindow(bool enable) override;
    void EnableWeaving(bool enable) override;
    void Update() override;
    GraphicsBackend GetGraphicsBackend() override;

    ComPtr<ID3D11Device> GetDevice();
    ComPtr<ID3D12CommandQueue>& GetCommandQueue();
    ComPtr<ID3D11DeviceContext>& GetDeviceContext();

    // Inherited via Renderer
    Compositor* const GetCompositor() override;
};
