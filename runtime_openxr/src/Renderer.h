#pragma once

#include "types.h"
#include "compositor.h"
#include "swapchain.h"
#include <weaver_directx_12.h>
#include "window.h"

namespace XRGameBridge {

    class Renderer {
        const uint8_t backbuffer_count = 2;
        GB_Compositor* compositor;

        GB_ProxySwapchain intermediate_resource;
        DirectX12Weaver* d3d12weaver;

        // Windowing
        GB_Window window;
        GB_GraphicsDevice window_swapchain;

        Renderer() = delete;

        void CreateIntermediateTexture();
        void CreateWeaver();
        void CreateSystemWindow();
        void CreateWindowSwapchain();

    public:
        Renderer(GraphicsBackend backend, void* graphics_binding);
        ~Renderer();

        void RenderFrame();
        void EnableSrWindow(bool enable);
        void EnableWeaving(bool enable = true);
    };
}
