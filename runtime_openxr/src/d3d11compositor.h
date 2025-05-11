#pragma once
#include "xrrendering.h"

class D3D11Renderer;

class D3D11Compositor: public Compositor {
    // DirectX 11
    ComPtr<ID3D11Device> d3d11_device;
    ComPtr <ID3D11VertexShader> vertex_shader;
    ComPtr <ID3D11PixelShader> pixel_shader;
    ComPtr<ID3D11SamplerState> sampler_state;
    ComPtr<ID3D11Buffer> shader_constant_buffer;
    ComPtr<ID3D11BlendState> blend_state_opaque;
    ComPtr<ID3D11BlendState> blend_state_blend;

    struct LayeringConstants {
        uint32_t is_opaque;
        uint32_t multiply_alpha;
        float convert_to_linear;
        float uvmin_x;
        float uvmin_y;
        float uvmax_x;
        float uvmax_y;
        float pad;
    };

public:
    bool Initialize(D3D11Renderer* renderer);

    bool CreatePipelineStateObject(ComPtr<ID3D12Device>& device, ComPtr<ID3D12RootSignature>& root, D3D12_BLEND_DESC blend_state, ComPtr<ID3D12PipelineState>& pipeline_state);

    //void InitShaders(const ComPtr<ID3D12Device>& device);
    void ComposeImage(const XrFrameEndInfo* frameEndInfo, ID3D11DeviceContext* cmd_list, uint32_t system_width, uint32_t system_height, uint64_t new_fence_value);
    void ComposeProjectionLayer(ID3D11DeviceContext* cmd_list, uint32_t system_width, uint32_t system_height, const XrCompositionLayerProjection* layer, uint64_t new_fence_value);
    void ComposeQuadLayer(ID3D11DeviceContext* cmd_list, uint32_t system_width, uint32_t system_height, const XrCompositionLayerQuad* layer, uint64_t new_fence_value);

    ComPtr<ID3D12PipelineState>& GetDefaultPipelineState();
};
