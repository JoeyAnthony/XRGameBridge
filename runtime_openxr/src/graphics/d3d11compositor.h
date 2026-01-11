/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

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
    ComPtr<ID3D11RasterizerState> rasterizer_state;

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

    //void InitShaders(const ComPtr<ID3D12Device>& device);
    void ComposeImage(const XrFrameEndInfo* frameEndInfo, ID3D11DeviceContext* context, uint32_t system_width, uint32_t system_height);
    void ComposeProjectionLayer(ID3D11DeviceContext* context, uint32_t system_width, uint32_t system_height, const XrCompositionLayerProjection* layer);
    void ComposeQuadLayer(ID3D11DeviceContext* context, uint32_t system_width, uint32_t system_height, const XrCompositionLayerQuad* layer);
};
