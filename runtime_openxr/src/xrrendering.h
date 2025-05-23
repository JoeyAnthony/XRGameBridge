#pragma once
#include <string>
#include <filesystem>

#include "openxr_includes.h"
#include "types.h"

class GB_Instance;

constexpr unsigned short back_buffer_count = 2;

class Compositor {
public:
    const std::string LAYERING_VERTEX_DEBUG = "../../runtime_openxr/shaders/layering_vertex.cso";
    const std::string LAYERING_PIXEL_DEBUG = "../../runtime_openxr/shaders/layering_pixel.cso";
    const std::string LAYERING_VERTEX_NAME = "shaders/layering_vertex.cso";
    const std::string LAYERING_PIXEL_NAME = "shaders/layering_pixel.cso";

    virtual ~Compositor() = default;

    /*
    * Check if a specific fence value for a frame has been reached, and wait for it when that's not the case.
    */
    //virtual void Destroy() = 0;

    static std::vector<char> LoadBinaryFile(std::string path) {
        std::filesystem::path file_path(path);
        std::string abs_path = std::filesystem::absolute(file_path).string();

        std::ifstream file(abs_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return std::vector<char>(0);
        }

        // Get size and reset cursor
        uint32_t size = file.tellg();
        file.seekg(0);

        // Load into buffer
        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size)) {
            return std::vector<char>(0);
        }

        return buffer;
    }
};

class Renderer {
public:
    virtual ~Renderer() = default;
    virtual XrResult RenderFrame(const XrFrameEndInfo* frameEndInfo) = 0;
    virtual void EnableSrWindow(bool enable) = 0;
    virtual void EnableWeaving(bool enable = true) = 0;
    virtual void Update() = 0;
    virtual GraphicsBackend GetGraphicsBackend() = 0;
    virtual Compositor* const GetCompositor() = 0;
};

class ProxySwapchain {
protected:
    XrSwapchain xr_handle = 0;
public:
    ProxySwapchain(XrSwapchain handle) : xr_handle(handle) {};
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

    XrSwapchain GetHandle() {
        return xr_handle;
    }
};
