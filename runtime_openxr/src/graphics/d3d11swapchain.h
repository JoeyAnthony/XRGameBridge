/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once
#include "xrrendering.h"
#include "openxr_includes.h"

class D3D11Renderer;

class D3D11ProxySwapchain: public ProxySwapchain {
    D3D11Renderer* d3d11_renderer;

    std::string proxy_name;
    uint32_t resolution_x = 0;
    uint32_t resolution_y = 0;
    bool is_depth_resource = false;

    std::vector<ComPtr<ID3D11Texture2D>> back_buffers;
    std::vector<ComPtr<ID3D11RenderTargetView>> render_target_views;
    std::vector<ComPtr<ID3D11ShaderResourceView>> shader_resource_views;
    std::vector<ComPtr<ID3D11DepthStencilView>> depth_stencil_views;

    uint32_t back_buffer_count = 0;
    uint32_t current_frame_index = 0;
    uint32_t awaited_frame_index = 0;
    uint32_t released_frame_index = 0;
    std::vector<ImageState> current_image_state;

    explicit D3D11ProxySwapchain(XrSwapchain handle, D3D11Renderer* renderer);

public:
    static D3D11ProxySwapchain* Create(const XrSwapchainCreateInfo* createInfo, D3D11Renderer* renderer, std::string resource_name = "");

    D3D11ProxySwapchain() = delete;

    // Resource initializer
    bool CreateResources(const XrSwapchainCreateInfo* createInfo, uint32_t num_resources, std::string resource_name);

    void DestroyResources() override;
    XrResult AcquireNextImage(uint32_t& index) override;
    XrResult WaitForImage(const XrDuration& timeout) override;
    XrResult ReleaseImage() override;
    uint32_t GetWidth() override;
    uint32_t GetHeight() override;
    uint64_t GetBufferCount() override;
    Renderer* GetRenderer() override;

    std::vector<ComPtr<ID3D11Texture2D>> GetBuffers();
    std::vector<ComPtr<ID3D11ShaderResourceView>> GetShaderResourceViews();
    std::vector<ComPtr<ID3D11RenderTargetView>> GetRenderTargetViews();
    bool IsDepthResource();

    uint32_t GetAwaitedImageIndex();
};

class D3D11WindowSwapchain {
public:
    //static void CreateDXGIFactory(IDXGIFactory4** factory);
    //static void GetGraphicsAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter);

    D3D11Renderer* d3d11_renderer;
    ComPtr<IDXGISwapChain3> swap_chain;
    uint32_t width, height;
    uint32_t back_buffer_count;
    std::vector<ComPtr<ID3D11RenderTargetView>> render_target_views;

    D3D11WindowSwapchain() = delete;
    D3D11WindowSwapchain(D3D11Renderer* renderer, const XrSwapchainCreateInfo* createInfo, uint32_t back_buffer_count, HWND hwnd);
public:

    uint32_t GetCurrentImageIndex();
    void PresentFrame();
    uint32_t GetWidth();
    uint32_t GetHeight();
    uint32_t GetBufferCount();
    Renderer* GetRenderer();

    std::vector<ComPtr<ID3D11Texture2D>> GetBuffers();
    std::vector<ComPtr<ID3D11ShaderResourceView>> GetShaderResourceViews();
    std::vector<ComPtr<ID3D11RenderTargetView>> GetRenderTargetViews();
};

void ApplyBindFlag(D3D11_BIND_FLAG flag, uint32_t& bind_flags);
void GetResourceStateFlags(XrSwapchainUsageFlags usage_flags, D3D11_USAGE& usage, uint32_t& bind_flags);
DXGI_FORMAT ResolveTextureFormatForUsage(DXGI_FORMAT application_format, XrSwapchainUsageFlags usage_flags);
