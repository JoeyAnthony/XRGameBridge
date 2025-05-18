#pragma once
#include "swapchain.h"
#include "xrrendering.h"

class D3D11Renderer;

class D3D11ProxySwapchain: public ProxySwapchain {
    XrSwapchain xr_handle;
    D3D11Renderer* d3d11_renderer;

    std::wstring proxy_name;
    uint32_t resolution_x = 0;
    uint32_t resolution_y = 0;
    bool is_depth_resource = false;

    std::vector<ComPtr<ID3D11Texture2D>> back_buffers;
    std::vector<ComPtr<ID3D11RenderTargetView>> render_target_views;
    std::vector<ComPtr<ID3D11ShaderResourceView>> shader_resource_views;
    std::vector<ComPtr<ID3D11DepthStencilView>> depth_stencil_views;

    D3D12_RESOURCE_STATES resource_usage = D3D12_RESOURCE_STATE_COMMON;
    uint32_t current_frame_index = 0;
    uint32_t awaited_frame_index = 0;
    uint32_t released_frame_index = 0;
    std::vector<ImageState> current_image_state;

    explicit D3D11ProxySwapchain(XrSwapchain handle, D3D11Renderer* renderer);

public:
    static XrResult CreateD3D11ProxySwapchain(const XrSwapchainCreateInfo* createInfo, D3D11Renderer* renderer, const D3D11ProxySwapchain* proxy_swapchain);
    //static XrResult CreateD3D11ProxySwapchain(ProxySwapchain* proxy_swapchain);

    D3D11ProxySwapchain() = delete;

    bool CreateResources(const XrSwapchainCreateInfo* createInfo, std::wstring resource_name = L"") override;
    // Resource initializer
    bool CreateResources(const XrSwapchainCreateInfo* createInfo, D3D11_USAGE usage, uint32_t bind_flags, std::wstring resource_name);

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

    [[nodiscard]] uint32_t GetAwaitedImageIndex();
};

void GetResourceStateFlags(XrSwapchainUsageFlags usage_flags, D3D11_USAGE& usage, uint32_t& bind_flags);
