#include "compositor.h"

#include <array>
#include <fstream>
#include <filesystem>

#include "swapchain.h"
#include "settings.h"


#include "instance.h"
namespace XRGameBridge {

    const std::string LAYERING_VERTEX_DEBUG = "../../runtime_openxr/shaders/layering_vertex.cso";
    const std::string LAYERING_PIXEL_DEBUG = "../../runtime_openxr/shaders/layering_pixel.cso";

    const std::string LAYERING_VERTEX_NAME = "shaders/layering_vertex.cso";
    const std::string LAYERING_PIXEL_NAME = "shaders/layering_pixel.cso";


    std::vector<char> LoadBinaryFile(std::string path) {
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

    void GB_Compositor::Initialize(const ComPtr<ID3D12Device>& device, const ComPtr<ID3D12CommandQueue>& queue, uint32_t back_buffer_count) {

        d3d12_device = device;
        command_queue = queue;

        // Create the root signature.
        {
            D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};

            // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
            feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

            if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data)))) {
                feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
            }

            CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
            // Remark descriptors are static now, not sure I can copy them. The data can be changed when not executing command lists
            ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
            ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

            CD3DX12_ROOT_PARAMETER1 root_parameters[3];
            root_parameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
            root_parameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
            root_parameters[2].InitAsConstants(3, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);



            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
            root_signature_desc.Init_1_1(_countof(root_parameters), root_parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error));
            ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
            root_signature->SetName(L"Compositor Root Signature");
        }

        // Create the pipeline state, which includes loading shaders.
        {
            std::vector<char>vertex_shader;
            std::vector<char>pixel_shader;

            fs::path shader_dir = fs::path(runtime_path).parent_path();
            if (fs::exists(shader_dir / LAYERING_VERTEX_NAME)) {
                fs::path vertex = shader_dir / LAYERING_VERTEX_NAME;
                fs::path pixel = shader_dir / LAYERING_PIXEL_NAME;
                vertex_shader = LoadBinaryFile(vertex.string());
                pixel_shader = LoadBinaryFile(pixel.string());

                if (vertex_shader.empty()) {
                    LOG(ERROR) << "Couldn't find shaders";
                }
            }
            else {
                vertex_shader = LoadBinaryFile(LAYERING_VERTEX_DEBUG);
                pixel_shader = LoadBinaryFile(LAYERING_PIXEL_DEBUG);
            }

            CD3DX12_RASTERIZER_DESC rasterizerStateDesc(D3D12_DEFAULT);
            rasterizerStateDesc.CullMode = D3D12_CULL_MODE_FRONT;

            // Define the vertex input layout.
            std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs = {
                //{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                //{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };

            // Describe and create the graphics pipeline state object (PSO).
            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.InputLayout = { inputElementDescs.data(),static_cast<uint32_t>(inputElementDescs.size()) };
            psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertex_shader.data(), vertex_shader.size());
            psoDesc.pRootSignature = root_signature.Get();
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixel_shader.data(), pixel_shader.size());
            psoDesc.RasterizerState = rasterizerStateDesc;
            psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            //psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            psoDesc.SampleMask = UINT_MAX;
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; //TODO choose format from the client
            //psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            psoDesc.SampleDesc.Count = 1;

            ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline_state)));
            pipeline_state->SetName(L"Compositor Pipeline State");
        }

        // Describe and create a sampler descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
        samplerHeapDesc.NumDescriptors = 1;
        samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&sampler_heap)));

        // Describe and create a sampler.
        D3D12_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        device->CreateSampler(&samplerDesc, sampler_heap->GetCPUDescriptorHandleForHeapStart());

        command_allocators.resize(back_buffer_count);
        command_lists.resize(back_buffer_count);
        for (uint32_t i = 0; i < back_buffer_count; i++) {
            // Create present command allocator and command list resources
            ThrowIfFailed(d3d12_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators[i])));
            // TODO use initial pipeline state here later. First check if it works without.
            ThrowIfFailed(d3d12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators[i].Get(), pipeline_state.Get(), IID_PPV_ARGS(&command_lists[i])));

            std::wstring name = std::format(L"Compositor Command List {}", i);
            command_lists[i]->SetName(name.c_str());
            command_lists[i]->Close();
        }
    }

    void GB_Compositor::ComposeImage(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list) {
        // TODO uses the command queue and the frame struct from endframe to compose the whole frame
        // TODO after that it executes the command list to render to the actual swapchain and set the fences on every proxy swapchain image

        if (frameEndInfo->layerCount == 0) {
            // TODO clear the screen when no layers are present
        }

        for (uint32_t layer_num = 0; layer_num < frameEndInfo->layerCount; layer_num++) {
            if (frameEndInfo->layers[layer_num]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
                auto layer = reinterpret_cast<const XrCompositionLayerProjection*>(frameEndInfo->layers[layer_num]);
                auto& ref_space = g_reference_spaces[layer->space]; // pose in spaces of the view over time

                // Render every view to the resource
                for (int32_t view_num = 0; view_num < layer->viewCount; view_num++) {
                    auto& view = layer->views[view_num];

                    // TODO do something with rectangles
                    auto& rect = view.subImage.imageRect;

                    auto& gb_swapchain = g_proxy_swapchains[view.subImage.swapchain];
                    auto proxy_resource = gb_swapchain.GetBuffers()[view.subImage.imageArrayIndex];

                    // Viewport settings
                    const float width = static_cast<float>(rect.extent.width);
                    const float height = static_cast<float>(rect.extent.height);
                    D3D12_VIEWPORT view_port{ (width * view_num), 0, width, height, 0.0f, 1.0f };
                    D3D12_RECT scissor_rect{ 0, 0, g_platform_manager->GetScreen()->getPhysicalResolutionWidth(), g_platform_manager->GetScreen()->getPhysicalResolutionHeight() };
                    cmd_list->RSSetViewports(1, &view_port);
                    cmd_list->RSSetScissorRects(1, &scissor_rect);

                    // TODO Maybe transition all buffers at once, maybe with split barriers, so we transition barriers at the same time?
                    // Transition proxy swapchain resource to pixel shader resource
                    TransitionImage(cmd_list, proxy_resource.Get(),gb_swapchain.resource_usage, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

                    std::array heaps = { gb_swapchain.GetSrvHeap().Get(), sampler_heap.Get() };
                    cmd_list->SetDescriptorHeaps(heaps.size(), heaps.data());

                    struct {
                        uint32_t is_opaque;
                        uint32_t multiply_alpha;
                        float convert_to_linear;
                    } layering_constants;
                    // Make opaque if XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT is not set
                    layering_constants.is_opaque = (layer->layerFlags& XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT) != XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                    // Multiply alpha if XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT is set
                    layering_constants.multiply_alpha = (layer->layerFlags & XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT) == XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
                    layering_constants.convert_to_linear = 0;
                    cmd_list->SetGraphicsRootSignature(root_signature.Get());
                    cmd_list->SetPipelineState(pipeline_state.Get());
                    cmd_list->SetGraphicsRoot32BitConstants(2, 3, &layering_constants, 0);

                    // Setting descriptor tables is optional if there is only a single texture. For multiple sets of textures, you want to move this index.
                    cmd_list->SetGraphicsRootDescriptorTable(0, gb_swapchain.GetSrvHeap()->GetGPUDescriptorHandleForHeapStart()); // Set offset in the heap for the shader (descriptor tables)
                    cmd_list->SetGraphicsRootDescriptorTable(1, sampler_heap->GetGPUDescriptorHandleForHeapStart());

                    cmd_list->DrawInstanced(3, 1, 0, 0);

                    // Transition proxy swapchain resource back to render target
                    TransitionImage(cmd_list, proxy_resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, gb_swapchain.resource_usage);
                }
            }
            else if (frameEndInfo->layers[layer_num]->type == XR_TYPE_COMPOSITION_LAYER_QUAD) {
                // TODO this is for viewing 2dimensional content in VR space. We could project this in 2d to the screen perhaps...
                // TODO maybe this is also used to display 3d videos without lookaround?
            }
        }
    }

    void GB_Compositor::ExecuteCommandLists(ID3D12GraphicsCommandList* cmd_list, const XrFrameEndInfo* frameEndInfo) {
        ID3D12CommandList* lists[]{ cmd_list };
        command_queue->ExecuteCommandLists(1, lists);

        // Go over every layer to signal all proxy swapchain fences
        for (uint32_t layer_num = 0; layer_num < frameEndInfo->layerCount; layer_num++) {
            if (frameEndInfo->layers[layer_num]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
                auto layer = reinterpret_cast<const XrCompositionLayerProjection*>(frameEndInfo->layers[layer_num]);
                    // In every layer get every view
                for (uint32_t view_num = 0; view_num < layer->viewCount; view_num++) {
                    // Get the swapchain from the view and signal its fence
                    auto& view = layer->views[view_num];
                    auto& gb_swapchain = g_proxy_swapchains[view.subImage.swapchain];
                    command_queue->Signal(gb_swapchain.fence.Get(), gb_swapchain.fence_values[gb_swapchain.current_frame_index]);
                }
            }
        }
    }

    void GB_Compositor::TransitionImage(ID3D12GraphicsCommandList* cmd_list, ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after) {
        if (state_before == state_after) {
            return;
        }

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, state_before, state_after);
        cmd_list->ResourceBarrier(1, &barrier);
    }

    ComPtr<ID3D12GraphicsCommandList>& GB_Compositor::GetCommandList(uint32_t index) {
        return command_lists[index];
    }

    ComPtr<ID3D12CommandAllocator>& GB_Compositor::GetCommandAllocator(uint32_t index) {
        return command_allocators[index];
    }

    ComPtr<ID3D12PipelineState>& GB_Compositor::GetPipelineState()
    {
        return pipeline_state;
    }
}
