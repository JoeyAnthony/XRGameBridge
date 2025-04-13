#include "compositor.h"

#include <array>
#include <fstream>
#include <filesystem>

#include "instance.h"
#include "swapchain.h"
#include "settings.h"
#include "session.h"


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

    bool GB_D3D12Compositor::Initialize(const XrGraphicsBindingD3D12KHR* d3d12, uint32_t back_buffer_count) {
        d3d12_device = d3d12->device;
        command_queue = d3d12->queue;
        HRESULT res = 0;
        // Create the root signature.
        {
            D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};

            // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
            feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

            if (FAILED(d3d12_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data)))) {
                feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
                LOG(ERROR) << "D3D12 Failed checking support for root signature 1.1, falling back to 1.0";
                return false;

                //if (FAILED(d3d12_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data)))) {
                //    LOG(ERROR) << "D3D12 Failed checking support for root signature 1.0";
                //}
            }

            CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
            // Remark descriptors are static now, not sure I can copy them. The data can be changed when not executing command lists
            ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
            ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

            CD3DX12_ROOT_PARAMETER1 root_parameters[3];
            root_parameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
            root_parameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
            root_parameters[2].InitAsConstants(8, 0, 0, D3D12_SHADER_VISIBILITY_ALL);

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
            if (feature_data.HighestVersion == D3D_ROOT_SIGNATURE_VERSION_1_1) {
                root_signature_desc.Init_1_1(_countof(root_parameters), root_parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
            }
            else if (feature_data.HighestVersion == D3D_ROOT_SIGNATURE_VERSION_1_0) {
               //root_signature_desc.Init_1_0(_countof(root_parameters), root_parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
            }

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            res = D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error);
            if(FAILED(res))
            {
                LOG(ERROR) << "D3D12 Error, failed serializing root signature";
                ThrowIfFailed(res);
                return false;
            }
            res = d3d12_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature));
            if (FAILED(res)) {
                LOG(ERROR) << "D3D12 Error, failed creating root signature";
                ThrowIfFailed(res);
                return false;
            }
            root_signature->SetName(L"Compositor Root Signature");
        }

        // Create pipeline states
        D3D12_BLEND_DESC  blend_state_opaque = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        D3D12_BLEND_DESC  blend_state_blend;
        blend_state_blend.AlphaToCoverageEnable = false;
        blend_state_blend.IndependentBlendEnable = false;

        // Inverse blend layered blend state
        blend_state_blend.RenderTarget[0].BlendEnable = true;
        blend_state_blend.RenderTarget[0].LogicOpEnable = false;
        blend_state_blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend_state_blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend_state_blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blend_state_blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blend_state_blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        blend_state_blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend_state_blend.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
        blend_state_blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        // TODO Loads the shaders twice this way
        if (CreatePipelineStateObject(d3d12_device, root_signature, blend_state_opaque, pipeline_state_opaque) == false) {
            // Error logged inside function
            return false;
        }

        if (CreatePipelineStateObject(d3d12_device, root_signature, blend_state_blend, pipeline_state_blend) == false) {
            // Error logged inside function
            return false;
        }

        // Describe and create a sampler descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
        samplerHeapDesc.NumDescriptors = 1;
        samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        res = d3d12_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&sampler_heap));
        if (FAILED(res)) {
            LOG(ERROR) << "D3D12 Error, failed to create descriptor heap";
            ThrowIfFailed(res);
            return false;
        }

        // Describe and create a sampler.
        D3D12_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        samplerDesc.BorderColor;
        d3d12_device->CreateSampler(&samplerDesc, sampler_heap->GetCPUDescriptorHandleForHeapStart());

        return true;
    }

    bool GB_D3D12Compositor::CreatePipelineStateObject(ComPtr<ID3D12Device>& device, ComPtr<ID3D12RootSignature>& root, D3D12_BLEND_DESC blend_state, ComPtr<ID3D12PipelineState>& pipeline_state)
    {
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
                    return false;
                }
            }
            else {
                vertex_shader = LoadBinaryFile(LAYERING_VERTEX_DEBUG);
                pixel_shader = LoadBinaryFile(LAYERING_PIXEL_DEBUG);
                LOG(INFO) << "Loading shaders with debug paths";
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
            psoDesc.pRootSignature = root.Get();
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixel_shader.data(), pixel_shader.size());
            psoDesc.RasterizerState = rasterizerStateDesc;
            psoDesc.BlendState = blend_state;
            //psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            psoDesc.SampleMask = UINT_MAX;
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; //TODO choose format from the client
            //psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            psoDesc.SampleDesc.Count = 1;

            HRESULT res = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline_state));
            if (FAILED(res)) {
                LOG(ERROR) << "D3D12 Error, failed to create graphics pipeline state";
                ThrowIfFailed(res);
                return false;
            }

            pipeline_state->SetName(L"Compositor Pipeline State");
        }

        return true;
    }

    void GB_D3D12Compositor::ComposeImage(const XrFrameEndInfo* frameEndInfo, ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, uint64_t new_fence_value) {
        // TODO uses the command queue and the frame struct from endframe to compose the whole frame
        // TODO after that it executes the command list to render to the actual swapchain and set the fences on every proxy swapchain image

        if (frameEndInfo->layerCount == 0) {
            // TODO clear the screen when no layers are present
        }

        for (uint32_t layer_num = 0; layer_num < frameEndInfo->layerCount; layer_num++) {
            if (frameEndInfo->layers[layer_num]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
                auto layer = reinterpret_cast<const XrCompositionLayerProjection*>(frameEndInfo->layers[layer_num]);
                ComposeProjectionLayer(cmd_list, system_width, system_height, layer, new_fence_value);
            }
            else if (frameEndInfo->layers[layer_num]->type == XR_TYPE_COMPOSITION_LAYER_QUAD) {
                auto layer = reinterpret_cast<const XrCompositionLayerQuad*>(frameEndInfo->layers[layer_num]);

                // TODO has to be done either after weaving, or also in both views
                ComposeQuadLayer(cmd_list, system_width, system_height, layer, new_fence_value);
            }
        }
    }

    void GB_D3D12Compositor::ComposeProjectionLayer(ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, const XrCompositionLayerProjection* layer, uint64_t new_fence_value) {
        auto& ref_space = g_reference_spaces[layer->space]; // pose in spaces of the view over time

        // Render every view to the resource
        for (int32_t view_num = 0; view_num < layer->viewCount; view_num++) {
            auto& view = layer->views[view_num];

            // This sets a pose to the session views, which breaks the positions. Not sure why this was here before.
            // Probably to update the positions before the update loop was there.
            //SetXrViewPose(session, view_num, view.pose);
            //SetXrViewFov(session, view_num, view.fov);

            // TODO do something with rectangles
            auto& rect = view.subImage.imageRect;

            auto& proxy_swapchain = g_proxy_swapchains[view.subImage.swapchain];
            auto proxy_resource = proxy_swapchain.GetBuffers()[proxy_swapchain.GetAwaitedImageIndex()];
            // Set new fence values for the used swapchain image.
            proxy_swapchain.SetReleasedImageFenceValue(proxy_swapchain.GetAwaitedImageIndex(), new_fence_value);

            // Viewport settings
            const float width = static_cast<float>(system_width) / 2;
            const float height = static_cast<float>(system_height);
            D3D12_VIEWPORT view_port{ view_num * width, 0, width, height, 0.0f, 1.0f };
            D3D12_RECT scissor_rect{ 0, 0, system_width, system_height };
            cmd_list->RSSetViewports(1, &view_port);
            cmd_list->RSSetScissorRects(1, &scissor_rect);

            // TODO Maybe transition all buffers at once, maybe with split barriers, so we transition barriers at the same time?
            // Transition proxy swapchain resource to pixel shader resource
            //TransitionImage(cmd_list, proxy_resource.Get(),proxy_swapchain.resource_usage, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            struct {
                uint32_t is_opaque;
                uint32_t multiply_alpha;
                float convert_to_linear;
                float uvmin_x;
                float uvmin_y;
                float uvmax_x;
                float uvmax_y;
                float pad;

            } layering_constants;
            // Make opaque if XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT is not set
            layering_constants.is_opaque = (layer->layerFlags & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT) != XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            // Multiply alpha if XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT is set
            layering_constants.multiply_alpha = (layer->layerFlags & XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT) == XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
            layering_constants.convert_to_linear = 1;

            // Normalize uv values
            layering_constants.uvmin_x = static_cast<float>(rect.offset.x) / static_cast<float>(proxy_swapchain.GetWidth());
            layering_constants.uvmin_y = static_cast<float>(rect.offset.y) / static_cast<float>(proxy_swapchain.GetHeight());
            layering_constants.uvmax_x = static_cast<float>(rect.offset.x + rect.extent.width) / static_cast<float>(proxy_swapchain.GetWidth());
            layering_constants.uvmax_y = static_cast<float>(rect.offset.y + rect.extent.height) / static_cast<float>(proxy_swapchain.GetHeight());

            std::array heaps = { proxy_swapchain.GetSrvHeap().Get(), sampler_heap.Get() };
            cmd_list->SetDescriptorHeaps(heaps.size(), heaps.data());

            cmd_list->SetGraphicsRootSignature(root_signature.Get());

            if (layering_constants.is_opaque) {
                cmd_list->SetPipelineState(pipeline_state_opaque.Get());
            }
            else {
                cmd_list->SetPipelineState(pipeline_state_blend.Get());
            }

            cmd_list->SetGraphicsRoot32BitConstants(2, 8, &layering_constants, 0);

            // Setting descriptor tables is optional if there is only a single texture. For multiple sets of textures, you want to move this index.
            auto proxy_resource_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(proxy_swapchain.GetSrvHeap()->GetGPUDescriptorHandleForHeapStart(), proxy_swapchain.GetAwaitedImageIndex(), proxy_swapchain.GetCbcSrvUavDescriptorSize());
            cmd_list->SetGraphicsRootDescriptorTable(0, proxy_resource_handle); // Set offset in the heap for the shader (descriptor tables)
            cmd_list->SetGraphicsRootDescriptorTable(1, sampler_heap->GetGPUDescriptorHandleForHeapStart());

            //float blend_factor[4]{ 0.f };
            //cmd_list->OMSetBlendFactor(blend_factor);

            cmd_list->DrawInstanced(3, 1, 0, 0);

            // Transition proxy swapchain resource back to render target
            //TransitionImage(cmd_list, proxy_resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, proxy_swapchain.resource_usage);
        }
    }

    void GB_D3D12Compositor::ComposeQuadLayer(ID3D12GraphicsCommandList* cmd_list, uint32_t system_width, uint32_t system_height, const XrCompositionLayerQuad* layer, uint64_t new_fence_value) {
        // TODO do something with rectangles
        auto& rect = layer->subImage.imageRect;

        // Since we don't care about the 'VR' space, we may not really have a need for this
        auto& ref_space = g_reference_spaces[layer->space]; // pose in spaces of the view over time
        layer->pose; // position and orientation of the quad in the reference frame of the space
        layer->size; // Width and height in meters

        uint8_t view_count = 0;
        uint8_t view_num = 0;

        if (layer->eyeVisibility == XR_EYE_VISIBILITY_BOTH) {
            view_count = 2;
            view_num = 0;
        }
        else if (layer->eyeVisibility == XR_EYE_VISIBILITY_LEFT) {
            view_count = 1;
            view_num = 0;
        }
        else if (layer->eyeVisibility == XR_EYE_VISIBILITY_RIGHT) {
            view_count = 2;
            view_num = 1;
        }

        auto& proxy_swapchain = g_proxy_swapchains[layer->subImage.swapchain];
        auto proxy_resource = proxy_swapchain.GetBuffers()[proxy_swapchain.GetAwaitedImageIndex()];
        // Set new fence values for the used swapchain image.
        proxy_swapchain.SetReleasedImageFenceValue(proxy_swapchain.GetAwaitedImageIndex(), new_fence_value);

       //TransitionImage(cmd_list, proxy_resource.Get(), proxy_swapchain.resource_usage, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        for (; view_num < view_count; view_num++) {
            // Viewport settings
            const float width = static_cast<float>(system_width) / 2;
            const float height = static_cast<float>(system_height);
            D3D12_VIEWPORT view_port{ view_num * width, 0, width, height, 0.0f, 1.0f };
            D3D12_RECT scissor_rect{ 0, 0, system_width, system_height };
            cmd_list->RSSetViewports(1, &view_port);
            cmd_list->RSSetScissorRects(1, &scissor_rect);

            struct {
                uint32_t is_opaque;
                uint32_t multiply_alpha;
                float convert_to_linear;
                float uvmin_x;
                float uvmin_y;
                float uvmax_x;
                float uvmax_y;
                float pad;

            } layering_constants;
            // Make opaque if XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT is not set
            layering_constants.is_opaque = (layer->layerFlags & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT) != XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            // Multiply alpha if XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT is set
            layering_constants.multiply_alpha = (layer->layerFlags & XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT) == XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
            layering_constants.convert_to_linear = 1;

            // Normalize uv values
            layering_constants.uvmin_x = static_cast<float>(rect.offset.x) / static_cast<float>(proxy_swapchain.GetWidth());
            layering_constants.uvmin_y = static_cast<float>(rect.offset.y) / static_cast<float>(proxy_swapchain.GetHeight());
            layering_constants.uvmax_x = static_cast<float>(rect.offset.x + rect.extent.width) / static_cast<float>(proxy_swapchain.GetWidth());
            layering_constants.uvmax_y = static_cast<float>(rect.offset.y + rect.extent.height) / static_cast<float>(proxy_swapchain.GetHeight());

            std::array heaps = { proxy_swapchain.GetSrvHeap().Get(), sampler_heap.Get() };
            cmd_list->SetDescriptorHeaps(heaps.size(), heaps.data());

            cmd_list->SetGraphicsRootSignature(root_signature.Get());

            if (layering_constants.is_opaque) {
                cmd_list->SetPipelineState(pipeline_state_opaque.Get());
            }
            else {
                cmd_list->SetPipelineState(pipeline_state_blend.Get());
            }

            cmd_list->SetGraphicsRoot32BitConstants(2, 8, &layering_constants, 0);

            // Setting descriptor tables is optional if there is only a single texture. For multiple sets of textures, you want to move this index.
            auto proxy_resource_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(proxy_swapchain.GetSrvHeap()->GetGPUDescriptorHandleForHeapStart(), proxy_swapchain.GetAwaitedImageIndex(), proxy_swapchain.GetCbcSrvUavDescriptorSize());
            cmd_list->SetGraphicsRootDescriptorTable(0, proxy_resource_handle); // Set offset in the heap for the shader (descriptor tables)
            cmd_list->SetGraphicsRootDescriptorTable(1, sampler_heap->GetGPUDescriptorHandleForHeapStart());

            cmd_list->DrawInstanced(3, 1, 0, 0);

            //TransitionImage(cmd_list, proxy_resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, proxy_swapchain.resource_usage);
        }
    }

    ComPtr<ID3D12PipelineState>& GB_D3D12Compositor::GetDefaultPipelineState()
    {
        return pipeline_state_opaque;
    }

    GB_D3D12Compositor::GB_D3D12Compositor() {
    }
}
