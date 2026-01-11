/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once
#include "openxr_includes.h"
#include "xrrendering.h"

class D3D12Renderer;

class D3D12Compositor : public Compositor {
    ComPtr<ID3D12RootSignature> root_signature;

    ComPtr<ID3D12PipelineState> pipeline_state_opaque;
    ComPtr<ID3D12PipelineState> pipeline_state_blend;

    ComPtr<ID3D12DescriptorHeap> sampler_heap;

    // DirectX 12
    ComPtr<ID3D12Device> d3d12_device;
    ComPtr<ID3D12CommandQueue> command_queue;

public:
    bool Initialize(D3D12Renderer* renderer);

    bool CreatePipelineStateObject(ComPtr<ID3D12Device>& device, ComPtr<ID3D12RootSignature>& root, D3D12_BLEND_DESC blend_state, ComPtr<ID3D12PipelineState>& pipeline_state);

    //void InitShaders(const ComPtr<ID3D12Device>& device);
    void ComposeImage(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, uint64_t new_fence_value);
    void ComposeProjectionLayer(ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, const XrCompositionLayerProjection* layer, uint64_t new_fence_value);
    void ComposeQuadLayer(ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, const XrCompositionLayerQuad* layer, uint64_t new_fence_value);
    //void SignalSwapchainsForFrame(const XrFrameEndInfo* frameEndInfo);

    ComPtr<ID3D12PipelineState>& GetDefaultPipelineState();
};
