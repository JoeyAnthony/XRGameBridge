/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

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

    SR::PredictingDX11Weaver* native_weaver = nullptr;
    D3D11Compositor* compositor = nullptr;
    D3D11WindowSwapchain* window_swapchain = nullptr;
    D3D11ProxySwapchain* intermediate_resource = nullptr;
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
    static D3D11Renderer* Create(XrSystemId systemId, const void* graphics_binding);

    D3D11Renderer() = delete;
    explicit D3D11Renderer(XrSystemId systemId, const XrGraphicsBindingD3D11KHR* graphics_binding);
    ~D3D11Renderer() override;

    // Inherited via Renderer
    void InitializePipeline(GB_Instance* instance) override;
    XrResult RenderFrame(const XrFrameEndInfo* frameEndInfo) override;
    void EnableSrWindow(bool enable) override;
    void EnableWeaving(bool enable) override;
    void Update() override;
    GraphicsBackend GetGraphicsBackend() override;
    Compositor* const GetCompositor() override;

    ComPtr<ID3D11Device> GetDevice();
    ComPtr<ID3D11DeviceContext>& GetDeviceContext();
};
