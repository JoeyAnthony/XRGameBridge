#pragma once

#include <unordered_map>
#include <array>

#include "D3D12Renderer.h"
#include "openxr_includes.h"

XrResult xrEnumerateSwapchainFormats(XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats);
XrResult xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain);
XrResult xrDestroySwapchain(XrSwapchain swapchain);
XrResult xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images);
XrResult xrEnumerateBoundSourcesForAction(XrSession session, const XrBoundSourcesForActionEnumerateInfo* enumerateInfo, uint32_t sourceCapacityInput, uint32_t* sourceCountOutput, XrPath* sources);
XrResult xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index);
XrResult xrWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo);
XrResult xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo);

namespace XRGameBridge {
    class D3D12Renderer;
    // Forward declaration for GB_ProxySwapchain friend
    class GB_DX12Compositor;

    enum ImageState {
        IMAGE_STATE_WAITING,
        IMAGE_STATE_RELEASED,

        IMAGE_STATE_ACQUIRED,
        IMAGE_STATE_RENDER_TARGET,
        IMAGE_STATE_WEAVING,
        IMAGE_STATE_DONE_WEAVING
    };

    // Back buffer count
    //TODO make this const inside the class and mutable through the constructor
    constexpr unsigned short g_back_buffer_count = 2;

    class GB_ProxySwapchain {
        virtual bool CreateResources(const XrSwapchainCreateInfo* createInfo, std::wstring resource_name = L"") = 0;
        virtual void DestroyResources() = 0;

        // Returns the oldest image index
        virtual XrResult AcquireNextImage(uint32_t& index) = 0;

        // Waits for an image that has been weaved
        virtual XrResult WaitForImage(const XrDuration& timeout) = 0;

        // Make the image available for weaving
        virtual XrResult ReleaseImage() = 0;

        virtual uint32_t GetWidth() = 0;
        virtual uint32_t GetHeight() = 0;
        virtual uint32_t GetBufferCount() = 0;

        virtual void SetReleasedImageFenceValue(uint32_t frameNum, uint64_t fenceValue) = 0;

        virtual Renderer* GetRenderer() = 0;
    };

    // TODO Use resources instead of creating multiple swap chains? Is that better?
    // UEVR creates a lot of swap chains so let's just use images....
    class GB_D3D12ProxySwapchain: public GB_ProxySwapchain {
        friend GB_DX12Compositor;
        XrSwapchain handle;
        D3D12Renderer* d3d12_renderer;

        std::wstring proxy_name;
        bool is_depth_resource = false;

        std::array<ComPtr<ID3D12Resource>, g_back_buffer_count> back_buffers;
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
        std::array<ImageState, g_back_buffer_count> current_image_state;
        uint64_t previous_fence_value = 0;

        // Fence values per image to check for
        std::array<uint32_t, g_back_buffer_count> back_buffer_fence_values;

        static constexpr float clear_color[4] = { 0.5f, 0.0f, 0.5f, 1.0f };

        ComPtr<ID3D12DescriptorHeap>& GetRtvHeap();
        ComPtr<ID3D12DescriptorHeap>& GetSrvHeap();
        uint32_t GetRtvDescriptorSize();

    public:
        GB_D3D12ProxySwapchain() = default;
        GB_D3D12ProxySwapchain(XrSwapchain handle, D3D12Renderer* renderer);

        // Todo Not sure how to get the initial resource usage if there are multiple specified, for example D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE and D3D12_RESOURCE_STATE_UNORDERED_ACCESS. Can't set them both initially so there exist the initial_usage parameter for now
        bool CreateResources(uint32_t width, uint32_t height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES states, std::wstring resource_name = L"");
        std::array<ComPtr<ID3D12Resource>, g_back_buffer_count> GetBuffers();

        // Interface functions
        bool CreateResources(const XrSwapchainCreateInfo* createInfo, std::wstring resource_name = L"") override;
        void DestroyResources() override;

        // Returns the oldest image index
        XrResult AcquireNextImage(uint32_t& index) override;

        // Waits for an image that has been weaved
        XrResult WaitForImage(const XrDuration& timeout) override;

        // Make the image available for weaving
        XrResult ReleaseImage() override;

        uint32_t GetWidth() override;
        uint32_t GetHeight() override;
        uint32_t GetBufferCount() override;

        void SetReleasedImageFenceValue(uint32_t back_buffer_frame_num, uint64_t fence_value) override;

        Renderer* GetRenderer() override;
    };

    // TODO swapchain is only necessary if we render to the XR Game Bridge window, otherwise we render to the back buffer of UEVR window
    // TODO Remark, this swapchain does not have synchronization objects, this is because we already wait for fences on proxy swapchains, which implicitly waits for this swapchains resources.
    class GB_GraphicsDevice {
        ComPtr<IDXGISwapChain3> swap_chain;
        ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
        ComPtr<ID3D12DescriptorHeap> m_srvHeap;
        std::array<ComPtr<ID3D12Resource>, g_back_buffer_count> back_buffers;

        D3D12_RESOURCE_STATES resource_usage = D3D12_RESOURCE_STATE_COMMON;
        uint32_t rtv_descriptor_size = 0;
        uint32_t frame_index = 0;

    public:
        static void CreateDXGIFactory(IDXGIFactory4** factory);
        static void GetGraphicsAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter);

        // Creates device
        bool CreateSwapChain(const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12CommandQueue>& queue, const XrSwapchainCreateInfo* createInfo, HWND hwnd);

        std::array<ComPtr<ID3D12Resource>, g_back_buffer_count> GetImages();
        ComPtr<ID3D12DescriptorHeap>& GetRtvHeap();
        ComPtr<ID3D12DescriptorHeap>& GetSrvHeap();
        uint32_t GetRtvDescriptorSize();

        uint32_t AcquireNextImage();
        void PresentFrame();
    };

    void GetResourceStateFlags(XrSwapchainUsageFlags usage_flags, D3D12_RESOURCE_FLAGS& flags, D3D12_RESOURCE_STATES& states);

    inline std::unordered_map<XrSwapchain, GB_D3D12ProxySwapchain> g_proxy_swapchains;
}
