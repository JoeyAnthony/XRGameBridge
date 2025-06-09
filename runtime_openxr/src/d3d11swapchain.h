#pragma once
#include "swapchain.h"
#include "xrrendering.h"
#include "openxr_includes.h"

class D3D11Renderer;

class D3D11ProxySwapchain: public ProxySwapchain {
    D3D11Renderer* d3d11_renderer;

    std::wstring proxy_name;
    uint32_t resolution_x = 0;
    uint32_t resolution_y = 0;
    bool is_depth_resource = false;

    std::vector<ComPtr<ID3D11Texture2D>> back_buffers;
    std::vector<ComPtr<ID3D11RenderTargetView>> render_target_views;
    std::vector<ComPtr<ID3D11ShaderResourceView>> shader_resource_views;
    std::vector<ComPtr<ID3D11DepthStencilView>> depth_stencil_views;

    uint32_t current_frame_index = 0;
    uint32_t awaited_frame_index = 0;
    uint32_t released_frame_index = 0;
    std::vector<ImageState> current_image_state;

    explicit D3D11ProxySwapchain(XrSwapchain handle, D3D11Renderer* renderer);

public:
    static D3D11ProxySwapchain* Create(const XrSwapchainCreateInfo* createInfo, D3D11Renderer* renderer);

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

void GetResourceStateFlags(XrSwapchainUsageFlags usage_flags, D3D11_USAGE& usage, uint32_t& bind_flags);
