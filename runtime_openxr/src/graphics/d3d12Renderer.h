/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once

#include "d3d12compositor.h"
#include "d3d12swapchain.h"
#include "openxr_includes.h"
#include "types.h"
#include "window.h"
#include "xrrendering.h"

#include <sr/weaver/dx12weaver.h>

class GB_Instance;
class D3D12ProxySwapchain;

class D3D12Renderer: public Renderer {
    XrSystemId xr_system;
    bool should_weave = true;
    bool weave_to_debug_window = true;

    // Graphics
    D3D12Compositor compositor;
    SR::IDX12Weaver1* d3d12weaver = nullptr;
    std::unique_ptr<D3D12ProxySwapchain> intermediate_resource = nullptr;
    std::unique_ptr<D3D12ProxySwapchain> weaved_resource = nullptr;
    // Windowing
    GameBridgeWindow window;
    D3D12WindowSwapchain window_swapchain;

    // D3D12
    ComPtr<ID3D12Device> d3d12_device;
    ComPtr<ID3D12CommandQueue> d3d12_command_queue;
    ComPtr<ID3D12InfoQueue> info_queue;
    DWORD m_infoqueue_callback_cookie = 0;
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
    XrResult CreateIntermediateTexture(const D3D12ProxySwapchain* back_buffer_swapchain);
    XrResult CreateWeaver(const D3D12ProxySwapchain* back_buffer_swapchain, const std::shared_ptr<SRSystem> &gb_system);
    XrResult CreateSystemWindow(const std::shared_ptr<SRSystem> &gb_system);
    XrResult CreateWindowSwapchain(const D3D12ProxySwapchain* back_buffer_swapchain, const std::shared_ptr<SRSystem> &gb_system);
    bool CreateCommandLists();
    bool CreateFenceObjects();
    bool DestroyFences();

    // Pipeline functions
    XrResult RenderFrameWeaving(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, const float clear_color[4], uint64_t new_fence_value, int32_t width, int32_t height);
    XrResult RenderFrameSideBySide(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, const float clear_color[4], uint64_t new_fence_value, int32_t width, int32_t height);

    void ExecuteCommandList(ID3D12GraphicsCommandList* cmd_list);
    void TransitionImage(ID3D12GraphicsCommandList* cmd_list, ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);
    void SetupInfoQueue();
    void DestroyInfoQueue();

    static void D3D12MessageCallback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR description, void* pContext);

public:
    static D3D12Renderer* Create(XrSystemId systemId, const void* graphics_binding);

    D3D12Renderer() = delete;
    explicit D3D12Renderer(XrSystemId systemId, const XrGraphicsBindingD3D12KHR* graphics_binding);
    ~D3D12Renderer() override;

    // Inherited via Renderer
    void InitializePipeline(XrSwapchain swapchain) override;
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

    ComPtr<ID3D12Device> &GetDevice();
    ComPtr<ID3D12CommandQueue> &GetCommandQueue();
    ComPtr<ID3D12GraphicsCommandList> &GetCommandList(uint32_t index);
    ComPtr<ID3D12CommandAllocator> &GetCommandAllocator(uint32_t index);

    // Inherited via Renderer
    uint64_t GetWeavedBufferHandle() override;
};
