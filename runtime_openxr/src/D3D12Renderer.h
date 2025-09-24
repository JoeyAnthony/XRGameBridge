#pragma once

#include "types.h"
#include "compositor.h"
#include "swapchain.h"
#include "window.h"
#include "D3D12Renderer.h"
#include "openxr_includes.h"
#include "weaver_directx_12.h"
#include "xrrendering.h"

class GB_Instance;

class D3D12Renderer : public Renderer {
    const uint8_t back_buffer_count = 2;
    XrSystemId xr_system;
    bool should_weave = true;

    // Graphics
    D3D12Compositor compositor;
    DirectX12Weaver* d3d12weaver;
    D3D12ProxySwapchain* intermediate_resource;
    // Windowing
    GameBridgeWindow window;
    D3D12WindowSwapchain window_swapchain;

    // D3D12
    ComPtr<ID3D12Device> d3d12_device;
    ComPtr<ID3D12CommandQueue> d3d12_command_queue;
    // TODO map holding an array of descriptors for each swapchain handle?
    std::vector<ComPtr<ID3D12CommandAllocator>> command_allocators;
    std::vector<ComPtr<ID3D12GraphicsCommandList>> command_lists;
    // Fence data
    HANDLE fence_event = nullptr;
    ComPtr<ID3D12Fence> fence;
    uint64_t fence_value = 0;
    std::vector<uint64_t> frame_fence_values;
    uint8_t frame_in_flight = 0;

    // Initialization
    XrResult CreateIntermediateTexture(GB_System& gb_system);
    XrResult CreateWeaver(GB_Instance* instance);
    XrResult CreateSystemWindow(GB_System& gb_system);
    XrResult CreateWindowSwapchain(GB_System& gb_system);
    bool CreateCommandLists();
    bool CreateFenceObjects();
    bool DestroyFences();

    // Pipeline functions
    XrResult RenderFrameWeaving(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, D3D12WindowSwapchain& window_swapchain, uint32_t window_swapchain_index, const float clear_color[4], uint64_t new_fence_value);
    XrResult RenderFrameSideBySide(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, D3D12WindowSwapchain& window_swapchain, uint32_t window_swapchain_index, const float clear_color[4], uint64_t new_fence_value);

    void ExecuteCommandList(ID3D12GraphicsCommandList* cmd_list);
    void TransitionImage(ID3D12GraphicsCommandList* cmd_list, ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);

public:
    D3D12Renderer();
    ~D3D12Renderer();

    XrResult Initialize(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding);

    XrResult RenderFrame(const XrFrameEndInfo* frameEndInfo) override;
    void EnableSrWindow(bool enable) override;
    void EnableWeaving(bool enable = true) override;
    void Update() override;
    GraphicsBackend GetGraphicsBackend() override;
    Compositor* const GetCompositor() override;

    /*
    * Check if a specific fence value for a frame has been reached, and wait for it when that's not the case.
    */
    XrResult WaitFenceSwapchain(uint32_t value, XrDuration timeout);
    void WaitForGpu();
    void ResetCommandLists();
    uint32_t GetFrameFenceValue(uint32_t frameNumber);

    ComPtr<ID3D12Device>& GetDevice();
    ComPtr<ID3D12CommandQueue>& GetCommandQueue();
    ComPtr<ID3D12GraphicsCommandList>& GetCommandList(uint32_t index);
    ComPtr<ID3D12CommandAllocator>& GetCommandAllocator(uint32_t index);
    void InitializePipeline(GB_Instance* instance) override;
};
