#include "compositor.h"

#include <array>
#include <fstream>
#include <filesystem>

#include "swapchain.h"


#include "instance.h"
namespace XRGameBridge {
    const std::string LAYERING_VERTEX = "../../runtime_openxr/shaders/layering_vertex.cso";
    const std::string LAYERING_PIXEL = "../../runtime_openxr/shaders/layering_pixel.cso";

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

            CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
            ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
            ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
            ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

            CD3DX12_ROOT_PARAMETER1 root_parameters[4];
            root_parameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
            root_parameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
            root_parameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_VERTEX);
            root_parameters[3].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

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
            //UINT8* pVertexShaderData;
            //UINT vertexShaderDataLength;
            auto vertex_shader = LoadBinaryFile(LAYERING_VERTEX);
            auto pixel_shader = LoadBinaryFile(LAYERING_PIXEL);

            //ThrowIfFailed(ReadDataFromFile(GetAssetFullPath(L"shader_mesh_simple_vert.cso").c_str(), &pVertexShaderData, &vertexShaderDataLength));


            CD3DX12_RASTERIZER_DESC rasterizerStateDesc(D3D12_DEFAULT);
            rasterizerStateDesc.CullMode = D3D12_CULL_MODE_NONE;

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
            psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            psoDesc.SampleMask = UINT_MAX;
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; //TODO choose format from the client
            psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            psoDesc.SampleDesc.Count = 1;

            ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline_state)));
            pipeline_state->SetName(L"Compositor Pipeline State");
        }

        command_allocators.resize(back_buffer_count);
        command_lists.resize(back_buffer_count);
        for (uint32_t i = 0; i < back_buffer_count; i++) {
            // Create present command allocator and command list resources
            ThrowIfFailed(d3d12_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators[i])));
            // TODO use initial pipeline state here later. First check if it works without.
            ThrowIfFailed(d3d12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators[i].Get(), nullptr, IID_PPV_ARGS(&command_lists[i])));

            command_lists[i]->Close();
        }
    }

    void GB_Compositor::ComposeImage(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list) {
        // TODO uses the command queue and the frame struct from endframe to compose the whole frame
        // TODO after that it executes the command list to render to the actual swapchain and set the fences on every proxy swapchain image

        if (frameEndInfo->layerCount == 0) {
            // TODO clear the screen when no layers are present
        }

        for (uint32_t i = 0; i < frameEndInfo->layerCount; i++) {
            if (frameEndInfo->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
                auto layer = reinterpret_cast<const XrCompositionLayerProjection*>(frameEndInfo->layers[i]);

                // TODO use alpha bits
                layer->layerFlags& XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;

                // If alpha should not be blended, make the alpha completely opaque in the shader
                uint32_t uniform_alpha = 1;
                if ((layer->layerFlags & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT) == 0) {
                    uniform_alpha = 1;
                }
                else {
                    uniform_alpha = 0;
                }

                auto& ref_space = g_reference_spaces[layer->space]; // pose in spaces of the view over time

                layer->viewCount;
                layer->views->subImage.imageRect;

                auto& gb_swapchain = g_application_render_targets[layer->views->subImage.swapchain];
                gb_swapchain.GetBuffers()[layer->views->subImage.imageArrayIndex]; // need srv's

                //cmd_list->OMSetRenderTargets() //TODO set render target to the swapchain
                cmd_list->SetDescriptorHeaps(1, gb_swapchain.GetSrvHeap().GetAddressOf());
                cmd_list->SetPipelineState(pipeline_state.Get());
                cmd_list->DrawInstanced(4, 1, 0, 0);
            }
            else if (frameEndInfo->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_QUAD) {
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
                    auto& gb_swapchain = g_application_render_targets[view.subImage.swapchain];
                    command_queue->Signal(gb_swapchain.fence.Get(), gb_swapchain.fence_values[gb_swapchain.current_frame_index]);
                }
            }
        }
    }

    void GB_Compositor::TransitionBackBufferImage(ID3D12GraphicsCommandList* cmd_list, ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after) {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, state_before, state_after);
        cmd_list->ResourceBarrier(1, &barrier);
    }

    ComPtr<ID3D12GraphicsCommandList>& GB_Compositor::GetCommandList(uint32_t index) {
        return command_lists[index];
    }

    ComPtr<ID3D12CommandAllocator>& GB_Compositor::GetCommandAllocator(uint32_t index) {
        return command_allocators[index];
    }
}