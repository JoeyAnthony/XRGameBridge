#pragma once

enum class GraphicsBackend {
    undefined = 0,
    D3D11 = 1,
    D3D12 = 2,
    Vulkan = 3,
    OpenGL = 4
};

// Data types
struct GBVector2i {
    uint64_t x;
    uint64_t y;
};
