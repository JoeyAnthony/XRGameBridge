#pragma once

#include "types.h"
#include "compositor.h"
#include "swapchain.h"
#include "window.h"
#include "D3D12Renderer.h"
#include "openxr_includes.h"

// 3D Game Bridge forward declaration
class DirectX12Weaver;

namespace XRGameBridge {
    class GB_Instance;

    class Renderer {
    public:
        virtual XrResult Initialize(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding) = 0;
        virtual void RenderFrame(const XrFrameEndInfo* frameEndInfo) = 0;
        virtual void EnableSrWindow(bool enable) = 0;
        virtual void EnableWeaving(bool enable = true) = 0;
        virtual void Update() = 0;
        virtual GraphicsBackend GetGraphicsBackend() = 0;
        virtual GB_Compositor* const GetCompositor() = 0;
    };

class D3D12Renderer : public Renderer{
        const uint8_t backbuffer_count = 2;
        GB_System gb_system;

        // Graphics
        GB_D3D12Compositor compositor;
        DirectX12Weaver* d3d12weaver;
        GB_D3D12ProxySwapchain* intermediate_resource;
        // Windowing
        GB_Window window;
        GB_D3D12WindowSwapchain window_swapchain;

        XrResult CreateIntermediateTexture();
        XrResult CreateWeaver(GB_Instance* instance);
        XrResult CreateSystemWindow();
        XrResult CreateWindowSwapchain();

    public:
        // D3D12
        ComPtr<ID3D12Device> d3d12_device;
        ComPtr<ID3D12CommandQueue> d3d12_command_queue;

        XrResult Initialize(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding) override;

        void RenderFrame(const XrFrameEndInfo* frameEndInfo) override;
        void EnableSrWindow(bool enable) override;
        void EnableWeaving(bool enable = true) override;
        void Update() override;
        GraphicsBackend GetGraphicsBackend() override;
        GB_Compositor* const GetCompositor() override;

        ~D3D12Renderer();
    };
}
