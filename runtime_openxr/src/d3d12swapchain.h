#pragma once
#include <array>

#include "openxr_includes.h"
#include "xrrendering.h"

class D3D12Renderer;

//TODO make this const inside the class and mutable through the constructor
class D3D12ProxySwapchain : public ProxySwapchain {
    XrSwapchain xr_handle;
    D3D12Renderer* d3d12_renderer;

    std::string proxy_name;
    bool is_depth_resource = false;

    std::array<ComPtr<ID3D12Resource>, back_buffer_count> back_buffers;
    ComPtr<ID3D12DescriptorHeap> rtv_heap;
    ComPtr<ID3D12DescriptorHeap> srv_heap;

    uint32_t rtv_descriptor_size = 0;
    uint32_t cbc_srv_uav_descriptor_size = 0;
    uint32_t resolution_x = 0;
    uint32_t resolution_y = 0;

    D3D12_RESOURCE_STATES resource_usage = D3D12_RESOURCE_STATE_COMMON;
    uint32_t current_frame_index = 0;
    uint32_t awaited_frame_index = 0;
    uint32_t released_frame_index = 0;
    std::array<ImageState, back_buffer_count> current_image_state;
    uint64_t previous_fence_value = 0;

    // Fence values per image to check for
    std::array<uint32_t, back_buffer_count> back_buffer_fence_values;

    D3D12ProxySwapchain() = delete;
    D3D12ProxySwapchain(XrSwapchain handle, D3D12Renderer* renderer);

public:
    ~D3D12ProxySwapchain() override;

    static D3D12ProxySwapchain* Create(const XrSwapchainCreateInfo* createInfo, D3D12Renderer* renderer, std::string resource_name = "");

    // Overriden initializer
    bool CreateResources(const XrSwapchainCreateInfo* createInfo, std::string resource_name = "") override;
    // Resource initializer
    bool CreateResources(uint32_t width, uint32_t height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES states, std::string resource_name = "");

    std::array<ComPtr<ID3D12Resource>, back_buffer_count> GetBuffers();

    // Interface functions
    void DestroyResources() override;
    // Returns the oldest image index
    XrResult AcquireNextImage(uint32_t& index) override;
    // Waits for an image that has been weaved
    XrResult WaitForImage(const XrDuration& timeout) override;
    // Make the image available for weaving
    XrResult ReleaseImage() override;
    uint32_t GetWidth() override;
    uint32_t GetHeight() override;
    size_t GetBufferCount() override;
    void SetReleasedImageFenceValue(uint32_t back_buffer_frame_num, uint64_t fence_value);

    ComPtr<ID3D12DescriptorHeap>& GetRtvHeap();
    ComPtr<ID3D12DescriptorHeap>& GetSrvHeap();
    [[nodiscard]] uint32_t GetRtvDescriptorSize();
    [[nodiscard]] uint32_t GetCbcSrvUavDescriptorSize();
    [[nodiscard]] uint32_t GetAwaitedImageIndex();

    Renderer* GetRenderer() override;
};

class D3D12WindowSwapchain {
public:
    D3D12Renderer* d3d12_renderer;
    ComPtr<IDXGISwapChain3> swap_chain;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    std::array<ComPtr<ID3D12Resource>, back_buffer_count> back_buffers;

    D3D12_RESOURCE_STATES resource_usage = D3D12_RESOURCE_STATE_COMMON;
    uint32_t rtv_descriptor_size = 0;

public:
    void Initialize(D3D12Renderer* renderer);

    // Creates device
    bool CreateSwapChain(const XrSwapchainCreateInfo* createInfo, HWND hwnd);

    std::array<ComPtr<ID3D12Resource>, back_buffer_count> GetImages();
    ComPtr<ID3D12DescriptorHeap>& GetRtvHeap();
    ComPtr<ID3D12DescriptorHeap>& GetSrvHeap();
    uint32_t GetRtvDescriptorSize();
    uint32_t GetCbcSrvUavDescriptorSize();
    uint32_t AcquireNextImage();
    void PresentFrame();

    D3D12WindowSwapchain();
};

void GetResourceStateFlags(XrSwapchainUsageFlags usage_flags, D3D12_RESOURCE_FLAGS& flags, D3D12_RESOURCE_STATES& states);
