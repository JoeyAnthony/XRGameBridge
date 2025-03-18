#include "renderer.h"

XRGameBridge::Renderer::Renderer(GraphicsBackend backend, void* graphics_binding) {
    // Manage window
    // Manage compositors
    // Manage weaver

    if (backend == GraphicsBackend::D3D11) {

    }
    else if (backend == GraphicsBackend::D3D12) {
        const XrGraphicsBindingD3D12KHR* d3d12_bindings = static_cast<const XrGraphicsBindingD3D12KHR*> (graphics_binding);
        GB_DX12Compositor* d3d12_compositor = new GB_DX12Compositor();
        if (d3d12_compositor->Initialize(d3d12_bindings, backbuffer_count) == false) {
            LOG(ERROR) << "Failed to create compositor";
            return XR_ERROR_RUNTIME_FAILURE;
        }
    }
}
