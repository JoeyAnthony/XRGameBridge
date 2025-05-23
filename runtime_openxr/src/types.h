#pragma once
#include <stdint.h>

enum class GraphicsBackend {
    undefined = 0,
    D3D11 = 1,
    D3D12 = 2,
    Vulkan = 3,
    OpenGL = 4
};

// Data types
struct GBVector2i {
    uint32_t x;
    uint32_t y;
};
