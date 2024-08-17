#pragma once
#include "openxr_includes.h"


namespace XRGameBridge {
    class GB_Session;

    class GB_Compositor {
        ComPtr<ID3D12RootSignature> root_signature;

        ComPtr<ID3D12PipelineState> pipeline_state_opaque;
        ComPtr<ID3D12PipelineState> pipeline_state_blend;

        ComPtr<ID3D12DescriptorHeap> sampler_heap;

        // TODO map holding an array of descriptors for each swapchain handle

        ComPtr<ID3D12Device> d3d12_device;
        ComPtr<ID3D12CommandQueue> command_queue;

        std::vector<ComPtr<ID3D12CommandAllocator>> command_allocators;
        std::vector<ComPtr<ID3D12GraphicsCommandList>> command_lists;

    public:
        bool Initialize(const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12CommandQueue>& queue, uint32_t back_buffer_count);

        bool CreatePipelineStateObject(ComPtr<ID3D12Device>& device, ComPtr<ID3D12RootSignature>& root, D3D12_BLEND_DESC blend_state, ComPtr<ID3D12PipelineState>& pipeline_state);

        //void InitShaders(const ComPtr<ID3D12Device>& device);
        void ComposeImage(GB_Session& session, const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height);
        //void ComposeProjectionLayer(ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, XrCompositionLayerProjection& layer);
        void ComposeQuadLayer(ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, const XrCompositionLayerQuad* layer);
        void ExecuteCommandList(ID3D12GraphicsCommandList* cmd_list);
        void SignalSwapchainsForFrame(const XrFrameEndInfo* frameEndInfo);

        void TransitionImage(ID3D12GraphicsCommandList* cmd_list, ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);

        void AddResource();
        void RemoveResource();
        void AddSwapchainResources();
        void RemoveSwapchainResources();

        ComPtr<ID3D12GraphicsCommandList>& GetCommandList(uint32_t index);
        ComPtr<ID3D12CommandAllocator>& GetCommandAllocator(uint32_t index);
        ComPtr<ID3D12PipelineState>& GetPipelineState();
    };
}
