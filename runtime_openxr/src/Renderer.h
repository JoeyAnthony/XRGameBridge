#pragma once

#include "types.h"
#include "compositor.h"

namespace XRGameBridge {

    class Renderer {

        const uint8_t backbuffer_count = 2;
        GB_Compositor* compositor;

        Renderer() = delete;

        Renderer(GraphicsBackend backend, void* graphics_binding);
        ~Renderer();
    };
}
