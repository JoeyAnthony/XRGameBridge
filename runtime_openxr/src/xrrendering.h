#pragma once
#include "compositor.h"
#include "openxr_includes.h"
#include "types.h"

constexpr std::string LAYERING_VERTEX_DEBUG = "../../runtime_openxr/shaders/layering_vertex.cso";
constexpr std::string LAYERING_PIXEL_DEBUG = "../../runtime_openxr/shaders/layering_pixel.cso";

constexpr std::string LAYERING_VERTEX_NAME = "shaders/layering_vertex.cso";
constexpr std::string LAYERING_PIXEL_NAME = "shaders/layering_pixel.cso";


class GB_Instance;
class Compositor;

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
    virtual Compositor* const GetCompositor() = 0;
};

class Compositor {
public:
    virtual ~Compositor() = default;

    /*
    * Check if a specific fence value for a frame has been reached, and wait for it when that's not the case.
    */
    //virtual void Destroy() = 0;

    std::vector<char> Compositor::LoadBinaryFile(std::string path) {
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
