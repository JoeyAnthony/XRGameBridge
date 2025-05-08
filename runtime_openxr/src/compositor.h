#pragma once
#include <filesystem>

#include "openxr_includes.h"
#include "xrrendering.h"

class D3D12Renderer;
class GB_Session;

constexpr std::string LAYERING_VERTEX_DEBUG = "../../runtime_openxr/shaders/layering_vertex.cso";
constexpr std::string LAYERING_PIXEL_DEBUG = "../../runtime_openxr/shaders/layering_pixel.cso";

constexpr std::string LAYERING_VERTEX_NAME = "shaders/layering_vertex.cso";
constexpr std::string LAYERING_PIXEL_NAME = "shaders/layering_pixel.cso";

class GB_D3D12Compositor : public GB_Compositor {
    ComPtr<ID3D12RootSignature> root_signature;

    ComPtr<ID3D12PipelineState> pipeline_state_opaque;
    ComPtr<ID3D12PipelineState> pipeline_state_blend;

    ComPtr<ID3D12DescriptorHeap> sampler_heap;

    // DirectX 12
    ComPtr<ID3D12Device> d3d12_device;
    ComPtr<ID3D12CommandQueue> command_queue;

public:
    bool Initialize(D3D12Renderer* renderer);

    bool CreatePipelineStateObject(ComPtr<ID3D12Device>& device, ComPtr<ID3D12RootSignature>& root, D3D12_BLEND_DESC blend_state, ComPtr<ID3D12PipelineState>& pipeline_state);

    //void InitShaders(const ComPtr<ID3D12Device>& device);
    void ComposeImage(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, uint64_t new_fence_value);
    void ComposeProjectionLayer(ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, const XrCompositionLayerProjection* layer, uint64_t new_fence_value);
    void ComposeQuadLayer(ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, const XrCompositionLayerQuad* layer, uint64_t new_fence_value);
    //void SignalSwapchainsForFrame(const XrFrameEndInfo* frameEndInfo);

    ComPtr<ID3D12PipelineState>& GetDefaultPipelineState();

    GB_D3D12Compositor();

    std::vector<char> GB_D3D12Compositor::LoadBinaryFile(std::string path) {
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

