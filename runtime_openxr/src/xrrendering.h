#pragma once
#include <string>
#include <filesystem>

#include "openxr_includes.h"
#include "types.h"

class GB_Instance;

constexpr unsigned short back_buffer_count = 2;

// Path defined in Cmake
#ifndef DEBUG_SHADER_PATH
#define DEBUG_SHADER_PATH ""
#endif

class Compositor {
public:


    const std::string shader_path = "shaders/";
    const std::string dx11_vs = "dx11.vs";
    const std::string dx11_ps = "dx11.ps";
    const std::string dx12_vs = "dx12.vs";
    const std::string dx12_ps = "dx12.ps";

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
