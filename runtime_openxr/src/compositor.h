#pragma once
#include "openxr_includes.h"

namespace XRGameBridge {
    class GB_GraphicsDevice;
    class GB_Session;

    class GB_Compositor {
    public:
        virtual XrResult RenderFrame(GB_Session& gb_session, const XrFrameEndInfo* frameEndInfo) = 0;
        virtual XrResult RenderFrameWeaving(GB_Session& gb_session, const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, GB_GraphicsDevice& window_swapchain, uint32_t window_swapchain_index, const float clear_color[4]) = 0;
        virtual XrResult RenderFrameSideBySide(GB_Session& gb_session, const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, GB_GraphicsDevice& window_swapchain, uint32_t window_swapchain_index, const float clear_color[4]) = 0;

        /*
        * Check if a specific fence value for a frame has been reached, and wait for it when that's not the case.
        */
        virtual XrResult WaitFenceSwapchain(uint32_t value, XrDuration timeout) = 0;
        virtual void WaitForGpu() = 0;
        virtual void ResetCommandLists() = 0;
        virtual uint32_t GetFrameFenceValue(uint32_t frameNumber) = 0;
    };

    class GB_DX12Compositor : public GB_Compositor{
        ComPtr<ID3D12RootSignature> root_signature;

        ComPtr<ID3D12PipelineState> pipeline_state_opaque;
        ComPtr<ID3D12PipelineState> pipeline_state_blend;

        ComPtr<ID3D12DescriptorHeap> sampler_heap;

        // TODO map holding an array of descriptors for each swapchain handle?

        std::vector<ComPtr<ID3D12CommandAllocator>> command_allocators;
        std::vector<ComPtr<ID3D12GraphicsCommandList>> command_lists;

        HANDLE fence_event = nullptr;
        ComPtr<ID3D12Fence> fence;
        uint64_t fence_value = 0;
        std::vector<uint64_t> frame_fence_values;
        uint8_t frame_in_flight = 0;
        uint8_t back_buffer_num = 0;

        // DirectX 12
        ComPtr<ID3D12Device> d3d12_device;
        ComPtr<ID3D12CommandQueue> command_queue;
        GB_ProxySwapchain intermediate_resource;
        // SR
        DirectX12Weaver* d3d12weaver;

    public:
        ~GB_DX12Compositor();

        bool Initialize(const XrGraphicsBindingD3D12KHR* d3d12, uint32_t back_buffer_count);
        void Deinitialize();
        bool CreateWeaver();

        bool CreatePipelineStateObject(ComPtr<ID3D12Device>& device, ComPtr<ID3D12RootSignature>& root, D3D12_BLEND_DESC blend_state, ComPtr<ID3D12PipelineState>& pipeline_state);

        //void InitShaders(const ComPtr<ID3D12Device>& device);
        void ComposeImage(GB_Session& session, const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height);
        void ComposeProjectionLayer(ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, const XrCompositionLayerProjection* layer);
        void ComposeQuadLayer(ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, const XrCompositionLayerQuad* layer);
        void ExecuteCommandList(ID3D12GraphicsCommandList* cmd_list);
        //void SignalSwapchainsForFrame(const XrFrameEndInfo* frameEndInfo);

        void TransitionImage(ID3D12GraphicsCommandList* cmd_list, ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);

        ComPtr<ID3D12GraphicsCommandList>& GetCommandList(uint32_t index);
        ComPtr<ID3D12CommandAllocator>& GetCommandAllocator(uint32_t index);
        ComPtr<ID3D12PipelineState>& GetPipelineState();

        // Interface functions
        XrResult RenderFrame(GB_Session& gb_session, const XrFrameEndInfo* frameEndInfo) override;
        XrResult RenderFrameWeaving(GB_Session& gb_session, const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, GB_GraphicsDevice& window_swapchain, uint32_t window_swapchain_index, const float clear_color[4]) override;
        XrResult RenderFrameSideBySide(GB_Session& gb_session, const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, GB_GraphicsDevice& window_swapchain, uint32_t window_swapchain_index, const float clear_color[4]) override;

        /*
        * Check if a specific fence value for a frame has been reached, and wait for it when that's not the case.
        */
        XrResult WaitFenceSwapchain(uint32_t value, XrDuration timeout) override;
        void WaitForGpu() override;
        void ResetCommandLists() override;
        uint32_t GetFrameFenceValue(uint32_t frameNumber) override;
    };
}
