#include "d3d11compositor.h"

#include "d3d11renderer.h"
#include "instance.h"
#include "filesystem"
#include "settings.h"
#include "d3d11swapchain.h"

namespace fs = std::filesystem;

bool D3D11Compositor::Initialize(D3D11Renderer* renderer) {
    d3d11_device = renderer->GetDevice();

    // Load shaders
    std::vector<char>v_shader_buffer;
    std::vector<char>p_shader_buffer;

    // Try shader path in shipping location, otherwise the debug location
    fs::path shader_dir = fs::path(runtime_path).parent_path();
    if (fs::exists(shader_dir / shader_path)) {
        fs::path vertex = shader_dir / dx11_vs;
        fs::path pixel = shader_dir / dx11_ps;
        v_shader_buffer = LoadBinaryFile(vertex.string());
        p_shader_buffer = LoadBinaryFile(pixel.string());
    }
    else {
        fs::path vertex = fs::path(DEBUG_SHADER_PATH) / dx11_vs;
        fs::path pixel = fs::path(DEBUG_SHADER_PATH) / dx11_ps;
        v_shader_buffer = LoadBinaryFile(vertex.string());
        p_shader_buffer = LoadBinaryFile(pixel.string());
        spdlog::info("Loading shaders with debug paths");
    }

    if (v_shader_buffer.empty() || p_shader_buffer.empty()) {
        spdlog::error("Couldn't find shaders");
        return false;
    }

    ThrowIfFailed(d3d11_device->CreateVertexShader(v_shader_buffer.data(), v_shader_buffer.size(), nullptr, vertex_shader.GetAddressOf()));
    ThrowIfFailed(d3d11_device->CreatePixelShader(p_shader_buffer.data(), p_shader_buffer.size(), nullptr, pixel_shader.GetAddressOf()));

    // Create a sampler state
    D3D11_SAMPLER_DESC sampler_desc;
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.MipLODBias = 0.0f;
    sampler_desc.MaxAnisotropy = 1;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampler_desc.BorderColor[0] = sampler_desc.BorderColor[1] = sampler_desc.BorderColor[2] = sampler_desc.BorderColor[3] = 0;
    sampler_desc.MinLOD = 0;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
    ThrowIfFailed(d3d11_device->CreateSamplerState(&sampler_desc, sampler_state.GetAddressOf()));

    // Create constant buffer
    D3D11_BUFFER_DESC buffer_desc;
    buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    buffer_desc.MiscFlags = 0;
    buffer_desc.ByteWidth = sizeof(LayeringConstants);

    ThrowIfFailed(d3d11_device->CreateBuffer(&buffer_desc, nullptr, shader_constant_buffer.GetAddressOf()));

    // Create opaque blend state
    D3D11_BLEND_DESC blend_state {};
    ThrowIfFailed(d3d11_device->CreateBlendState(&blend_state, blend_state_opaque.GetAddressOf()));

    // Create layer blending blend state
    blend_state.RenderTarget->BlendEnable = true;
    blend_state.RenderTarget->SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend_state.RenderTarget->DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_state.RenderTarget->BlendOp = D3D11_BLEND_OP_ADD;
    blend_state.RenderTarget->SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_state.RenderTarget->DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend_state.RenderTarget->BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_state.RenderTarget->RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ThrowIfFailed(d3d11_device->CreateBlendState(&blend_state, blend_state_opaque.GetAddressOf()));

    // Create rasterizer state
    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_FRONT;

    d3d11_device->CreateRasterizerState(&rasterizerDesc, rasterizer_state.GetAddressOf());

    return true;
}

void D3D11Compositor::ComposeImage(const XrFrameEndInfo* frameEndInfo, ID3D11DeviceContext* context, uint32_t system_width, uint32_t system_height) {
    if (frameEndInfo->layerCount == 0) {
        // TODO clear the screen when no layers are present
    }

    for (uint32_t layer_num = 0; layer_num < frameEndInfo->layerCount; layer_num++) {
        if (frameEndInfo->layers[layer_num]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
            auto layer = reinterpret_cast<const XrCompositionLayerProjection*>(frameEndInfo->layers[layer_num]);
            ComposeProjectionLayer(context, system_width, system_height, layer);
        }
        else if (frameEndInfo->layers[layer_num]->type == XR_TYPE_COMPOSITION_LAYER_QUAD) {
            auto layer = reinterpret_cast<const XrCompositionLayerQuad*>(frameEndInfo->layers[layer_num]);

            // TODO has to be done either after weaving, or also in both views
            ComposeQuadLayer(context, system_width, system_height, layer);
        }
    }
}

void D3D11Compositor::ComposeProjectionLayer(ID3D11DeviceContext* context, uint32_t system_width, uint32_t system_height, const XrCompositionLayerProjection* layer) {
    auto& ref_space = g_reference_spaces[layer->space]; // pose in spaces of the view over time

    // Render every view to the resource
    for (int32_t view_num = 0; view_num < layer->viewCount; view_num++) {
        auto& view = layer->views[view_num];

        // This sets a pose to the session views, which breaks the positions. Not sure why this was here before.
        // Probably to update the positions before the update loop was there.
        //SetXrViewPose(session, view_num, view.pose);
        //SetXrViewFov(session, view_num, view.fov);

        auto& rect = view.subImage.imageRect;

        auto proxy_swapchain = reinterpret_cast<D3D11ProxySwapchain*>(g_proxy_swapchains[view.subImage.swapchain]);
        auto proxy_resource = proxy_swapchain->GetShaderResourceViews()[proxy_swapchain->GetAwaitedImageIndex()];

        // Viewport settings
        const float width = static_cast<float>(system_width) / 2;
        const float height = static_cast<float>(system_height);
        D3D11_VIEWPORT view_port{ view_num * width, 0, width, height, 0.0f, 1.0f };
        D3D11_RECT scissor_rect{ 0, 0, system_width, system_height };
        context->RSSetViewports(1, &view_port);
        context->RSSetScissorRects(1, &scissor_rect);
        context->RSSetState(rasterizer_state.Get());

        // Update shader constants
        bool is_opaque = (layer->layerFlags & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT) != XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;

        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        context->VSSetShader(vertex_shader.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, shader_constant_buffer.GetAddressOf());

        context->PSSetShader(pixel_shader.Get(), nullptr, 0);
        context->PSSetConstantBuffers(0, 1, shader_constant_buffer.GetAddressOf());
        context->PSSetShaderResources(0, 1, proxy_resource.GetAddressOf());

        context->PSSetSamplers(0, 1, sampler_state.GetAddressOf());

        if (is_opaque) {
            context->OMSetBlendState(blend_state_opaque.Get(), {}, 0xffffffff);
        }
        else {
            context->OMSetBlendState(blend_state_blend.Get(), {}, 0xffffffff);
        }

        D3D11_MAPPED_SUBRESOURCE mapped_constants;
        context->Map(shader_constant_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_constants);
        {
            LayeringConstants* layering_constants = reinterpret_cast<LayeringConstants*>(mapped_constants.pData);
            // Make opaque if XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT is not set
            is_opaque = (layer->layerFlags & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT) != XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            layering_constants->is_opaque = is_opaque;
            // Multiply alpha if XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT is set
            layering_constants->multiply_alpha = (layer->layerFlags & XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT) == XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
            layering_constants->convert_to_linear = 1;

            // Normalize uv values
            layering_constants->uvmin_x = static_cast<float>(rect.offset.x) / static_cast<float>(proxy_swapchain->GetWidth());
            layering_constants->uvmin_y = static_cast<float>(rect.offset.y) / static_cast<float>(proxy_swapchain->GetHeight());
            layering_constants->uvmax_x = static_cast<float>(rect.offset.x + rect.extent.width) / static_cast<float>(proxy_swapchain->GetWidth());
            layering_constants->uvmax_y = static_cast<float>(rect.offset.y + rect.extent.height) / static_cast<float>(proxy_swapchain->GetHeight());

        }
        context->Unmap(shader_constant_buffer.Get(), 0);

        context->Draw(3, 0);
    }
}

void D3D11Compositor::ComposeQuadLayer(ID3D11DeviceContext* context, uint32_t system_width, uint32_t system_height, const XrCompositionLayerQuad* layer) {
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

    auto proxy_swapchain = reinterpret_cast<D3D11ProxySwapchain*>(g_proxy_swapchains[layer->subImage.swapchain]);
    auto proxy_resource = proxy_swapchain->GetShaderResourceViews()[proxy_swapchain->GetAwaitedImageIndex()];

    for (; view_num < view_count; view_num++) {
        // Viewport settings
        const float width = static_cast<float>(system_width) / 2;
        const float height = static_cast<float>(system_height);
        D3D11_VIEWPORT view_port{ view_num * width, 0, width, height, 0.0f, 1.0f };
        D3D11_RECT scissor_rect{ 0, 0, system_width, system_height };
        context->RSSetViewports(1, &view_port);
        context->RSSetScissorRects(1, &scissor_rect);

        // Update shader constants
        bool is_opaque = false;
        D3D11_MAPPED_SUBRESOURCE mapped_constants;
        context->Map(shader_constant_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_constants);
        {
            LayeringConstants* layering_constants = reinterpret_cast<LayeringConstants*>(mapped_constants.pData);
            // Make opaque if XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT is not set
            is_opaque = (layer->layerFlags & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT) != XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            layering_constants->is_opaque = is_opaque;
            // Multiply alpha if XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT is set
            layering_constants->multiply_alpha = (layer->layerFlags & XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT) == XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
            layering_constants->convert_to_linear = 1;

            // Normalize uv values
            layering_constants->uvmin_x = static_cast<float>(rect.offset.x) / static_cast<float>(proxy_swapchain->GetWidth());
            layering_constants->uvmin_y = static_cast<float>(rect.offset.y) / static_cast<float>(proxy_swapchain->GetHeight());
            layering_constants->uvmax_x = static_cast<float>(rect.offset.x + rect.extent.width) / static_cast<float>(proxy_swapchain->GetWidth());
            layering_constants->uvmax_y = static_cast<float>(rect.offset.y + rect.extent.height) / static_cast<float>(proxy_swapchain->GetHeight());

        }
        context->Unmap(shader_constant_buffer.Get(), 0);

        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        context->VSSetShader(vertex_shader.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, shader_constant_buffer.GetAddressOf());

        context->PSSetShader(pixel_shader.Get(), nullptr, 0);
        context->PSSetConstantBuffers(0, 1, shader_constant_buffer.GetAddressOf());
        context->PSSetShaderResources(view_num, 1, &proxy_resource);

        context->PSSetSamplers(0, 1, sampler_state.GetAddressOf());

        if (is_opaque) {
            context->OMSetBlendState(blend_state_opaque.Get(), {}, 0xffffffff);
        }
        else {
            context->OMSetBlendState(blend_state_blend.Get(), {}, 0xffffffff);
        }

        context->Draw(3, 0);
    }
}
