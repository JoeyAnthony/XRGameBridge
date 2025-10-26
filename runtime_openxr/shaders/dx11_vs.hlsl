/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

static float4 vertices[3] =
{
    { -1.0f,  3.0f, 0, 1 }, // top-left
    { -1.0f, -1.0f, 0, 1 }, // bottom-left
    { 3.0f,  -1.0f, 0, 1 }  // bottom-right
};

// Draw the texture square in the lower left 'half' of the triangle.
// Since the upper left and lower right vertices are outside the screen, the texture has to be 'squeezed' inwards from the top and the right.
// So wrapping the texture at the top and right sides we move it to the visible area of the triangle.
static float2 uvcoords[3] =
{
    { 0.0f, -1.0f},
    { 0.0f, 1.0f},
    { 2.0f, 1.0f}
};

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

struct VSInput
{
    uint VertexIndex : SV_VertexID;
};

// Pixel shader input
struct PSInput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

// Vertex shader
PSInput main(VSInput input)
{
    PSInput result;

    float2 uvmin = { uvmin_x, uvmin_y };
    float2 uvmax = { uvmax_x, uvmax_y };

    result.pos = vertices[input.VertexIndex];

    // Scale the uvcoords to a 0, 1 system
    float2 uv_scaled = { uvcoords[input.VertexIndex].x, uvcoords[input.VertexIndex].y + 1 };
    uv_scaled = uvmin + uv_scaled * (uvmax - uvmin);
    float2 uv_final = { uv_scaled.x, uv_scaled.y -1 };

    result.uv = uv_final;

    return result;
}
