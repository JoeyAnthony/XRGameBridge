/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

// Constant buffer
cbuffer cbShaderParams : register(b0)
{
    int is_opaque;
    int multiply_alpha;
    float convert_to_linear;
    float uvmin_x;
    float uvmin_y;
    float uvmax_x;
    float uvmax_y;
    float pad;
};

// Pixel shader input
struct PSInput {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

// Pixel shader
Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    float4 layer_color = g_texture.Sample(g_sampler, input.uv.xy);

    // Determine whether we need gamma correction
    float4 output_color = pow(abs(layer_color), 1.0f / (1.0f + (1.333f * convert_to_linear)));

    // TODO need to test these implementations
    // Pre-multiply alpha or not
    float alpha = 1 - (1 - layer_color.a) * multiply_alpha;
    // Determine whether the layer should be fully opaque or with alpha blending
    float blend = layer_color.a + (1 - layer_color.a) * is_opaque;
    output_color = float4(output_color.rgb * alpha, blend);

    return output_color;
    //return float4(input.uv.x, input.uv.y, 0.f, 1.f);
    //return float4(uvmax_x, 0.f, 0.f, 1.f);
}