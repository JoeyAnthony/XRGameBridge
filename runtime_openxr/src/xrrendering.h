#pragma once
#include "openxr_includes.h"
#include "types.h"


class GB_Instance;
class GB_Compositor;

constexpr unsigned short back_buffer_count = 2;

class Renderer {
public:

    virtual ~Renderer() = default;
    virtual XrResult Initialize(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding) = 0;
    virtual XrResult RenderFrame(const XrFrameEndInfo* frameEndInfo) = 0;
    virtual void EnableSrWindow(bool enable) = 0;
    virtual void EnableWeaving(bool enable = true) = 0;
    virtual void Update() = 0;
    virtual GraphicsBackend GetGraphicsBackend() = 0;
    virtual GB_Compositor* const GetCompositor() = 0;
};

class GB_Compositor {
public:
    virtual ~GB_Compositor() = default;

    /*
    * Check if a specific fence value for a frame has been reached, and wait for it when that's not the case.
    */
    //virtual void Destroy() = 0;
};

class ProxySwapchain {
public:
    virtual ~ProxySwapchain() = default;

    virtual bool CreateResources(const XrSwapchainCreateInfo* createInfo, std::wstring resource_name = L"") = 0;
    virtual void DestroyResources() = 0;

    // Returns the oldest image index
    virtual XrResult AcquireNextImage(uint32_t& index) = 0;

    // Waits for an image that has been weaved
    virtual XrResult WaitForImage(const XrDuration& timeout) = 0;

    // Make the image available for weaving
    virtual XrResult ReleaseImage() = 0;

    virtual uint32_t GetWidth() = 0;
    virtual uint32_t GetHeight() = 0;
    virtual size_t GetBufferCount() = 0;

    virtual Renderer* GetRenderer() = 0;
};
