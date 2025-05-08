#pragma once
#include "xrrendering.h"

class D3D11Renderer : public Renderer {
    uint64_t xr_system;
    ComPtr<ID3D11Device> d3d111_device;

public:
    static XrResult CreateD3D11Renderer(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding, D3D11Renderer* renderer);

    D3D11Renderer() = delete;
    explicit D3D11Renderer(GB_Instance* instance, XrSystemId systemId, const XrGraphicsBindingD3D11KHR* graphics_binding);

    XrResult Initialize(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding) override;

    XrResult RenderFrame(const XrFrameEndInfo* frameEndInfo) override;
    void EnableSrWindow(bool enable) override;
    void EnableWeaving(bool enable) override;
    void Update() override;
    GraphicsBackend GetGraphicsBackend() override;
    GB_Compositor* const GetCompositor() override;

    ID3D11Device* GetDevice();
};
