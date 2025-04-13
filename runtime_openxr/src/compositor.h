#pragma once
#include "openxr_includes.h"

namespace XRGameBridge {
    class GB_Session;

    class GB_Compositor {
    public:
        virtual ~GB_Compositor() = default;

        /*
        * Check if a specific fence value for a frame has been reached, and wait for it when that's not the case.
        */
        //virtual void Destroy() = 0;
    };

    class GB_D3D12Compositor : public GB_Compositor{
        ComPtr<ID3D12RootSignature> root_signature;

        ComPtr<ID3D12PipelineState> pipeline_state_opaque;
        ComPtr<ID3D12PipelineState> pipeline_state_blend;

        ComPtr<ID3D12DescriptorHeap> sampler_heap;

        // DirectX 12
        ComPtr<ID3D12Device> d3d12_device;
        ComPtr<ID3D12CommandQueue> command_queue;

    public:
        bool Initialize(const XrGraphicsBindingD3D12KHR* d3d12, uint32_t back_buffer_count);

        bool CreatePipelineStateObject(ComPtr<ID3D12Device>& device, ComPtr<ID3D12RootSignature>& root, D3D12_BLEND_DESC blend_state, ComPtr<ID3D12PipelineState>& pipeline_state);

        //void InitShaders(const ComPtr<ID3D12Device>& device);
        void ComposeImage(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, uint64_t new_fence_value);
        void ComposeProjectionLayer(ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, const XrCompositionLayerProjection* layer, uint64_t new_fence_value);
        void ComposeQuadLayer(ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, const XrCompositionLayerQuad* layer, uint64_t new_fence_value);
        //void SignalSwapchainsForFrame(const XrFrameEndInfo* frameEndInfo);

        ComPtr<ID3D12PipelineState>& GetDefaultPipelineState();

        GB_D3D12Compositor();
    };
}
