static float4 vertices[3] =
{
    { -1.0f,  3.0f, 0, 1 }, // top-left
    { -1.0f, -1.0f, 0, 1 }, // bottom-left
    { 3.0f,  -1.0f, 0, 1 }  // bottom-right
};

// Draw the texture square in the lower left 'half' of the triangle.
// Since the upper left and lower right vertices are outside the screen, the texture has to be 'pushed' inwards from the top and the right.
// So wrapping the texture at the top and right sides we move it to the visible area of the triangle.
static float2 uvcoords[3] =
{
    { 0.0f, -1.0f},
    { 0.0f, 1.0f},
    { 2.0f, 1.0f}
};

// Constant buffer
struct temp
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
ConstantBuffer<temp> settings : register(b0, space0);

// Pixel shader input
struct PSInput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

// Vertex shader
PSInput main(uint VertexIndex : SV_VertexID)
{
    PSInput result;

    float2 uvmin = { settings.uvmin_x, settings.uvmin_y };
    float2 uvmax = { settings.uvmax_x, settings.uvmax_y };

    result.pos = vertices[VertexIndex];

    // Scale the uvcoords to a 0, 1 system
    float2 uv_scaled = { uvcoords[VertexIndex].x, uvcoords[VertexIndex].y + 1 };
    uv_scaled = uvmin + uv_scaled * (uvmax - uvmin);
    float2 uv_final = { uv_scaled.x, uv_scaled.y -1 };

    result.uv = uv_final;

    return result;
}
