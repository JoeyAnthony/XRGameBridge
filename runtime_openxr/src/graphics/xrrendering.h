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
#include <map>

#include "openxr_includes.h"
#include "settings.h"
#include "types.h"

#include "../generated/shaders_generated.h"

namespace fs = std::filesystem;

class GB_Instance;
// Use triple buffering
constexpr unsigned short standard_swapchain_buffer_count = 3;

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
    static constexpr std::string_view shader_path = "shaders/";
    static constexpr std::string_view dx11_vs = "dx11_vs_layering.vs";
    static constexpr std::string_view dx11_fs = "dx11_fs_layering.fs";
    static constexpr std::string_view dx12_vs = "dx12_vs_layering.vs";
    static constexpr std::string_view dx12_fs = "dx12_fs_layering.fs";

    virtual ~Compositor() = default;

    /*
    * Check if a specific fence value for a frame has been reached, and wait for it when that's not the case.
    */
    //virtual void Destroy() = 0;

    static std::vector<uint8_t> LoadBinaryFile(std::string path) {
        fs::path file_path(path);
        std::string abs_path = fs::absolute(file_path).string();

        std::ifstream file(abs_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return std::vector<uint8_t>(0);
        }

        // Get size and reset cursor
        uint32_t size = file.tellg();
        file.seekg(0);

        // Load into buffer
        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            return std::vector<uint8_t>(0);
        }

        return buffer;
    }

    static std::vector<uint8_t> LoadShader(const std::string_view shader_name) {
#ifdef EMBED_SHADERS
        std::map<std::string_view, decltype(EmbeddedShaders::shader_vertex_dx11)> map{
            {dx11_vs, EmbeddedShaders::shader_vertex_dx11},
            {dx11_fs, EmbeddedShaders::shader_fragment_dx11},
            { dx12_vs, EmbeddedShaders::shader_vertex_dx12 },
            { dx12_fs, EmbeddedShaders::shader_fragment_dx12 },
        };

        return map[shader_name];
#else
        // Try shader path in release location, otherwise the debug location
        fs::path shader_dir = fs::path(runtime_path).parent_path() / shader_path;
        if (fs::exists(shader_dir)) {
            fs::path shader = shader_dir / shader_name;
            return LoadBinaryFile(shader.string());
        }
        else {
            spdlog::info("Loading shader from debug location");
            fs::path shader = fs::path(DEBUG_SHADER_PATH) / shader_name;
            return LoadBinaryFile(shader.string());
        }
#endif
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
	virtual uint64_t GetWeavedBufferHandle() = 0;
    virtual void InitializePipeline(GB_Instance* instance) = 0;
};

class ProxySwapchain {
protected:
    XrSwapchain xr_handle = 0;
public:
    ProxySwapchain(XrSwapchain handle) : xr_handle(handle) {};
    virtual ~ProxySwapchain() = default;

    virtual bool CreateResources(const XrSwapchainCreateInfo* createInfo, uint32_t num_resources, std::string resource_name = "") = 0;
    virtual void DestroyResources() = 0;
	virtual bool Resize(int32_t width, int32_t height) = 0;

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
