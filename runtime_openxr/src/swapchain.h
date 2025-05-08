#pragma once

#include <unordered_map>
#include <array>

#include "openxr_includes.h"
#include "xrrendering.h"

XrResult xrEnumerateSwapchainFormats(XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats);
XrResult xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain);
XrResult xrDestroySwapchain(XrSwapchain swapchain);
XrResult xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images);
XrResult xrEnumerateBoundSourcesForAction(XrSession session, const XrBoundSourcesForActionEnumerateInfo* enumerateInfo, uint32_t sourceCapacityInput, uint32_t* sourceCountOutput, XrPath* sources);
XrResult xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index);
XrResult xrWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo);
XrResult xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo);

class D3D12Renderer;

enum ImageState {
    IMAGE_STATE_WAITING,
    IMAGE_STATE_RELEASED,

    IMAGE_STATE_ACQUIRED,
    IMAGE_STATE_RENDER_TARGET,
    IMAGE_STATE_WEAVING,
    IMAGE_STATE_DONE_WEAVING
};

//TODO make this const inside the class and mutable through the constructor
class D3D12ProxySwapchain : public ProxySwapchain {
    XrSwapchain xr_handle;
    D3D12Renderer* d3d12_renderer;

    std::wstring proxy_name;
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

public:
    void Initialize(XrSwapchain handle, D3D12Renderer* renderer);

    // Overriden initializer
    bool CreateResources(const XrSwapchainCreateInfo* createInfo, std::wstring resource_name = L"") override;
    // Resource initializer
    bool CreateResources(uint32_t width, uint32_t height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES states, std::wstring resource_name = L"");

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
    [[nodiscard]] uint32_t GetRtvDescriptorSize() const;
    [[nodiscard]] uint32_t GetCbcSrvUavDescriptorSize() const;
    [[nodiscard]] uint32_t GetAwaitedImageIndex() const;

    Renderer* GetRenderer() override;

    D3D12ProxySwapchain();

    static constexpr float clear_color[4] = { 0.5f, 0.0f, 0.5f, 1.0f };
};

class GB_GraphicsDevice {
public:
    static void CreateDXGIFactory(IDXGIFactory4** factory);
    static void GetGraphicsAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter);
};

class GB_D3D12WindowSwapchain {
    D3D12Renderer* d3d12_renderer;

    ComPtr<IDXGISwapChain3> swap_chain;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    std::array<ComPtr<ID3D12Resource>, back_buffer_count> back_buffers;

    D3D12_RESOURCE_STATES resource_usage = D3D12_RESOURCE_STATE_COMMON;
    uint32_t rtv_descriptor_size = 0;
    uint32_t frame_index = 0;

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

    GB_D3D12WindowSwapchain();
};

void GetResourceStateFlags(XrSwapchainUsageFlags usage_flags, D3D12_RESOURCE_FLAGS& flags, D3D12_RESOURCE_STATES& states);

// Global of swapchains
inline std::unordered_map<XrSwapchain, ProxySwapchain*> g_proxy_swapchains;
