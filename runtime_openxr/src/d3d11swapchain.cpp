#include "d3d11swapchain.h"

#include <format>
#include <glm/glm.hpp>
#include "d3d11renderer.h"

XrResult D3D11ProxySwapchain::CreateD3D11ProxySwapchain(const XrSwapchainCreateInfo* createInfo, D3D11Renderer* renderer, const D3D11ProxySwapchain* proxy_swapchain) {
    static size_t swapchain_creation_count = 1;
    // Create handle
    XrSwapchain handle = reinterpret_cast<XrSwapchain>(swapchain_creation_count);

    // Create swap chain
    auto d3d11_proxy = new D3D11ProxySwapchain(handle, renderer);

    // Initialize resources
    XrResult result = XR_ERROR_RUNTIME_FAILURE;
    if (d3d11_proxy->CreateResources(createInfo) == false) {
        LOG(ERROR) << "Failed to create proxy swapchain";
        result = XR_ERROR_RUNTIME_FAILURE;
    }

    proxy_swapchain = d3d11_proxy;

    swapchain_creation_count++;
    return result;
}

XrResult D3D11ProxySwapchain::CreateD3D11ProxySwapchain(ProxySwapchain* proxy_swapchain) {
    D3D11ProxySwapchain* proxy = nullptr;
    CreateD3D11ProxySwapchain(proxy);
    proxy_swapchain = proxy;
}

D3D11ProxySwapchain::D3D11ProxySwapchain(XrSwapchain handle, D3D11Renderer* renderer) {
    xr_handle = handle;
    d3d11_renderer = renderer;
}

bool D3D11ProxySwapchain::CreateResources(const XrSwapchainCreateInfo* createInfo, std::wstring resource_name) {
    D3D11_USAGE d3d11_usage;
    uint32_t bind_flags;

    GetResourceStateFlags(createInfo->usageFlags, d3d11_usage, bind_flags);
    CreateResources(createInfo, d3d11_usage, bind_flags, resource_name);
}

bool D3D11ProxySwapchain::CreateResources(const XrSwapchainCreateInfo* createInfo, D3D11_USAGE usage, uint32_t bind_flags, std::wstring resource_name) {
    resolution_x = createInfo->width;
    resolution_y = createInfo->height;

    // Initialize resource vectors
    current_image_state.resize(back_buffer_count);
    if (bind_flags & D3D11_BIND_DEPTH_STENCIL) {
        back_buffers.resize(0);
        render_target_views.resize(0);
        shader_resource_views.resize(0);
        depth_stencil_views.resize(back_buffer_count);

        is_depth_resource = true;
    }
    else {
        back_buffers.resize(back_buffer_count);
        render_target_views.resize(back_buffer_count);
        shader_resource_views.resize(back_buffer_count);
        depth_stencil_views.resize(back_buffer_count);

        is_depth_resource = false;
    }

    D3D11_TEXTURE2D_DESC texture_desc;
    ZeroMemory(&texture_desc, sizeof(D3D11_TEXTURE2D_DESC));
    texture_desc.Width = createInfo->width;
    texture_desc.Height = createInfo->height;
    texture_desc.MipLevels = createInfo->mipCount;
    texture_desc.ArraySize = createInfo->arraySize;
    texture_desc.Format = static_cast<DXGI_FORMAT>(createInfo->format);
    texture_desc.SampleDesc.Count = createInfo->sampleCount;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Usage = usage;
    texture_desc.BindFlags = bind_flags;
    texture_desc.CPUAccessFlags = 0;
    texture_desc.MiscFlags = 0;

    auto* device = d3d11_renderer->GetDevice();

    // Check multi sample quality
    uint32_t sample_quality;
    if (SUCCEEDED(device->CheckMultisampleQualityLevels(texture_desc.Format, texture_desc.SampleDesc.Count, &sample_quality))) {
        texture_desc.SampleDesc.Quality = glm::min<uint32_t>(sample_quality, D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT);
    }
    else {
        LOG(WARNING) << "D3D11 Couldn't retrieve multi sample quality levels";
    }

    std::wstring com_name_prefix = L"";

    // Create resources
    for (uint32_t i = 0; i < back_buffers.size(); i++) {
        if (texture_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
            auto hr = device->CreateTexture2D(&texture_desc, nullptr, back_buffers[i].GetAddressOf());
            if (FAILED(hr))
                return hr;

            D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
            ZeroMemory(&descDSV, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
            // Put all precision to the depth
            descDSV.Format = static_cast<DXGI_FORMAT>(createInfo->format);
            descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            descDSV.Texture2D.MipSlice = 0;

            hr = device->CreateDepthStencilView(back_buffers[i].Get(), &descDSV, depth_stencil_views[i].GetAddressOf());
            if (FAILED(hr))
                return hr;

            com_name_prefix = L"Depth ";

            // TODO Example releases the resource here?
            //back_buffers[i]->Release();
        }
        else {
            if (FAILED(device->CreateTexture2D(&texture_desc, nullptr, back_buffers[i].GetAddressOf()))) {
                LOG(ERROR) << "D3D11 Failed creating proxy swapchain texture";
                return false;
            }

            D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
            ZeroMemory(&rtv_desc, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
            rtv_desc.Format = static_cast<DXGI_FORMAT>(createInfo->format);
            rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

            if (FAILED(device->CreateRenderTargetView(back_buffers[i].Get(), &rtv_desc, render_target_views[i].GetAddressOf()))) {
                LOG(ERROR) << "D3D11 Failed creating proxy swapchain render target view";
                return false;
            }

            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
            ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
            srv_desc.Format = static_cast<DXGI_FORMAT>(createInfo->format);
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = createInfo->mipCount;
            srv_desc.Texture2D.MostDetailedMip = 0;

            if (FAILED(device->CreateShaderResourceView(back_buffers[i].Get(), &srv_desc, shader_resource_views[i].GetAddressOf()))) {
                LOG(ERROR) << "D3D11 Failed creating proxy swapchain shader resource view";
                return false;
            }

            // TODO Example releases the resource here?
            //back_buffers[i]->Release();
        }

        // Choose name for debugging
        std::wstring texname;
        std::wstring rtvname;
        std::wstring srvname;
        if (resource_name.empty()) {
            texname = std::format(L"Proxy Swapchain {} {} {}", reinterpret_cast<size_t>(xr_handle), "Texture", i);
            texname = com_name_prefix + texname;

            rtvname = std::format(L"Proxy Swapchain {} {} {}", reinterpret_cast<size_t>(xr_handle), "Render Target View", i);
            rtvname = com_name_prefix + texname;

            srvname = std::format(L"Proxy Swapchain {} {} {}", reinterpret_cast<size_t>(xr_handle), "Shader Resource View", i);
            srvname = com_name_prefix + texname;

            proxy_name = texname;
        }
        else {
            std::wstring name = std::format(L"{} {} Resource {}", resource_name, reinterpret_cast<size_t>(xr_handle), i);

            texname = std::format(L"{} {} {} {}", resource_name, reinterpret_cast<size_t>(xr_handle), "Texture", i);
            texname = com_name_prefix + texname;

            rtvname = std::format(L"{} {} {} {}", resource_name, reinterpret_cast<size_t>(xr_handle), "Render Target View", i);
            rtvname = com_name_prefix + texname;

            srvname = std::format(L"{} {} {} {}", resource_name, reinterpret_cast<size_t>(xr_handle), "Shader Resource View", i);
            srvname = com_name_prefix + texname;

            proxy_name = name;
        }

        // Give name to the buffer
        if (FAILED(back_buffers[i]->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof(wchar_t) * texname.size(), texname.c_str()))) {
            LOG(WARNING) << "D3D11 Failed naming swapchain resource";
        }
        if (FAILED(back_buffers[i]->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof(wchar_t) * rtvname.size(), rtvname.c_str()))) {
            LOG(WARNING) << "D3D11 Failed naming swapchain resource";
        }
        if (FAILED(back_buffers[i]->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof(wchar_t) * srvname.size(), srvname.c_str()))) {
            LOG(WARNING) << "D3D11 Failed naming swapchain resource";
        }
    }

    return true;
}

void D3D11ProxySwapchain::DestroyResources() {
    back_buffers.clear();
    render_target_views.clear();
    shader_resource_views.clear();
    depth_stencil_views.clear();
    current_image_state.clear();
}

XrResult D3D11ProxySwapchain::AcquireNextImage(uint32_t& index) {
    uint32_t next_index = (current_frame_index + 1) % back_buffer_count;

    if (current_image_state[next_index] != IMAGE_STATE_RELEASED) {
        return XR_ERROR_CALL_ORDER_INVALID;
    }

    // Set a new current frame index
    current_frame_index = next_index;
    current_image_state[current_frame_index] = IMAGE_STATE_ACQUIRED;
    index = current_frame_index;
    return XR_SUCCESS;
}

XrResult D3D11ProxySwapchain::WaitForImage(const XrDuration& timeout) {
    if (current_image_state[current_frame_index] != IMAGE_STATE_ACQUIRED) {
        return XR_ERROR_CALL_ORDER_INVALID;
    }

    // TODO should wait when it's done weaving/presenting

    // Set the image state to render target because we have waited for the image to be freed so it can be used by the application again.
    current_image_state[current_frame_index] = IMAGE_STATE_RENDER_TARGET;
    awaited_frame_index = current_frame_index;

    return XR_SUCCESS;
}

XrResult D3D11ProxySwapchain::ReleaseImage() {
    if (current_image_state[awaited_frame_index] != IMAGE_STATE_RENDER_TARGET) {
        return XR_ERROR_CALL_ORDER_INVALID;
    }

    current_image_state[awaited_frame_index] = IMAGE_STATE_RELEASED;

    released_frame_index = awaited_frame_index;

    //LOG(INFO) << "px - "
    //    << " swapchain: " << handle
    //    << " aqcuired index " << current_frame_index
    //    << " awaited index " << awaited_frame_index
    //    << " released index " << released_frame_index
    //    ;

    return XR_SUCCESS;
}

uint32_t D3D11ProxySwapchain::GetWidth() {
    return resolution_x;
}

uint32_t D3D11ProxySwapchain::GetHeight() {
    return resolution_y;
}

uint64_t D3D11ProxySwapchain::GetBufferCount() {
    if(is_depth_resource) {
        return depth_stencil_views.size();
    }

    return render_target_views.size();
}

Renderer* D3D11ProxySwapchain::GetRenderer() {
    return d3d11_renderer;
}

void GetResourceStateFlags(XrSwapchainUsageFlags usage_flags, D3D11_USAGE& usage, uint32_t& bind_flags) {
    if (XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT & usage_flags) {
        bind_flags |= D3D11_BIND_RENDER_TARGET;
        usage = D3D11_USAGE_DEFAULT;
    }
    if (XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT & usage_flags) {
        bind_flags |= D3D11_BIND_DEPTH_STENCIL;
        usage = D3D11_USAGE_DEFAULT;
    }
    if (XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT & usage_flags) {
        bind_flags |= D3D11_BIND_UNORDERED_ACCESS;
        usage = D3D11_USAGE_DEFAULT;
    }
    if (XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT & usage_flags) {
        usage = D3D11_USAGE_DEFAULT;
    }
    if (XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT & usage_flags) {
        usage = D3D11_USAGE_DEFAULT;
    }
    if (XR_SWAPCHAIN_USAGE_SAMPLED_BIT & usage_flags) {
        bind_flags |= D3D11_BIND_SHADER_RESOURCE;
        usage = D3D11_USAGE_DEFAULT;
    }
    if (XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT & usage_flags) {
        usage = D3D11_USAGE_DEFAULT;
    }
}
