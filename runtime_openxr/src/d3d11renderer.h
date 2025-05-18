#pragma once
#include "xrrendering.h"

class GB_System;

class D3D11Renderer : public Renderer {
    uint64_t xr_system;
    ComPtr<ID3D11Device> d3d111_device;

    ID3D11DeviceContext* d3d11_device_context;
    D3D11WindowSwapchain* window_swapchain;
    D3D11ProxySwapchain* intermediate_resource;
    // Windowing
    GameBridgeWindow window;

    // Initialization
    XrResult CreateIntermediateTexture(GB_System& gb_system);
    XrResult CreateWeaver(GB_Instance* instance, GB_System& gb_system);
    XrResult CreateSystemWindow(GB_System& gb_system);
    XrResult CreateWindowSwapchain(GB_System& gb_system);
    bool CreateCommandLists();

    // Pipeline functions
    XrResult RenderFrameWeaving(const XrFrameEndInfo* frameEndInfo, uint32_t window_swapchain_index, const float clear_color[4], uint64_t new_fence_value);
    XrResult RenderFrameSideBySide(const XrFrameEndInfo* frameEndInfo, uint32_t window_swapchain_index, const float clear_color[4], uint64_t new_fence_value);

public:
    static XrResult CreateD3D11Renderer(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding, D3D11Renderer* renderer);

    D3D11Renderer() = delete;
    explicit D3D11Renderer(GB_Instance* instance, XrSystemId systemId, const XrGraphicsBindingD3D11KHR* graphics_binding);

    XrResult Initialize(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding) override;

    XrResult RenderFrame(const XrFrameEndInfo* frameEndInfo) override;
    void EnableSrWindow(bool enable) override;
    void EnableWeaving(bool enable) override;
    void Update() override;
    GraphicsBackend GetGraphicsBackend() override;
    Compositor* const GetCompositor() override;

    ComPtr<ID3D11Device> GetDevice();
    ComPtr<ID3D12CommandQueue>& GetCommandQueue();
    ComPtr<ID3D11DeviceContext>& GetCommandList(uint32_t index);
};
