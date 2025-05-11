#include "openxr_includes.h"
#include "d3d11renderer.h"

XrResult D3D11Renderer::CreateD3D11Renderer(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding, D3D11Renderer* renderer) {
    const XrGraphicsBindingD3D11KHR* d3d11_bindings = static_cast<const XrGraphicsBindingD3D11KHR*> (graphics_binding);

    { // Check validity of the device
        const ID3D12Object* obj = dynamic_cast<ID3D12Object*> (d3d11_bindings->device);
        if (!obj) {
            return XR_ERROR_GRAPHICS_DEVICE_INVALID;
        }
    }

    renderer = new D3D11Renderer(instance, systemId, d3d11_bindings);

    return XR_SUCCESS;
}

D3D11Renderer::D3D11Renderer(GB_Instance* instance, XrSystemId systemId, const XrGraphicsBindingD3D11KHR* graphics_binding) {
    xr_system = systemId;
    d3d111_device = graphics_binding->device;
}

XrResult D3D11Renderer::Initialize(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding) {
}

XrResult D3D11Renderer::RenderFrame(const XrFrameEndInfo* frameEndInfo) {
}

void D3D11Renderer::EnableSrWindow(bool enable) {
}

void D3D11Renderer::EnableWeaving(bool enable) {
}

void D3D11Renderer::Update() {
}

GraphicsBackend D3D11Renderer::GetGraphicsBackend() {
}

Compositor* const D3D11Renderer::GetCompositor() {
}

ID3D11Device* D3D11Renderer::GetDevice() {
    return d3d111_device.Get();
}
