/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once
#include <string>
#include <filesystem>
#include <fstream>

#include "openxr_includes.h"
#include "types.h"

class GB_Instance;

constexpr unsigned short back_buffer_count = 2;

// Path defined in Cmake
#ifndef DEBUG_SHADER_PATH
#define DEBUG_SHADER_PATH ""
#endif

enum ImageState {
    IMAGE_STATE_WAITING,
    IMAGE_STATE_RELEASED,

    IMAGE_STATE_ACQUIRED,
    IMAGE_STATE_RENDER_TARGET,
    IMAGE_STATE_WEAVING,
    IMAGE_STATE_DONE_WEAVING
};

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
    static constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    virtual ~Renderer() = default;
    virtual XrResult RenderFrame(const XrFrameEndInfo* frameEndInfo) = 0;
    virtual void EnableSrWindow(bool enable) = 0;
    virtual void EnableWeaving(bool enable = true) = 0;
    virtual void Update() = 0;
    virtual GraphicsBackend GetGraphicsBackend() = 0;
    virtual Compositor* const GetCompositor() = 0;
    virtual void InitializePipeline(GB_Instance* instance) = 0;
};

class ProxySwapchain {
protected:
    XrSwapchain xr_handle = 0;
public:
    ProxySwapchain(XrSwapchain handle) : xr_handle(handle) {};
    virtual ~ProxySwapchain() = default;

    virtual bool CreateResources(const XrSwapchainCreateInfo* createInfo, std::string resource_name = "") = 0;
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
