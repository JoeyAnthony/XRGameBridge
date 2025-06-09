#include "d3d11swapchain.h"
#include "openxr_includes.h"

#include <format>
#include <glm/glm.hpp>
#include "d3d11renderer.h"

D3D11ProxySwapchain* D3D11ProxySwapchain::Create(const XrSwapchainCreateInfo* createInfo, D3D11Renderer* renderer) {
    // Create with swapchain index handle
    // Add to swapchain lists
    // Throw/rethrow errors that occur

    static size_t swapchain_creation_count = 1;
    // Create handle
    XrSwapchain handle = reinterpret_cast<XrSwapchain>(swapchain_creation_count);
    auto d3d11_proxy = new D3D11ProxySwapchain(handle, renderer);

    // Initialize resources
    XrResult result = XR_ERROR_RUNTIME_FAILURE;
    if (d3d11_proxy->CreateResources(createInfo) == false) {
         throw XrException(XR_ERROR_RUNTIME_FAILURE, "Failed to create proxy swapchain");
    }

    swapchain_creation_count++;
    return d3d11_proxy;
}

//XrResult D3D11ProxySwapchain::CreateD3D11ProxySwapchain(ProxySwapchain* proxy_swapchain) {
//    D3D11ProxySwapchain* proxy = nullptr;
//    CreateD3D11ProxySwapchain(proxy);
//    proxy_swapchain = proxy;
//}

D3D11ProxySwapchain::D3D11ProxySwapchain(XrSwapchain handle, D3D11Renderer* renderer) : ProxySwapchain(handle)   {
    d3d11_renderer = renderer;
    current_image_state = std::vector(1, IMAGE_STATE_WAITING);
}

bool D3D11ProxySwapchain::CreateResources(const XrSwapchainCreateInfo* createInfo, std::wstring resource_name) {
    D3D11_USAGE d3d11_usage;
    uint32_t bind_flags;

    GetResourceStateFlags(createInfo->usageFlags, d3d11_usage, bind_flags);
    CreateResources(createInfo, d3d11_usage, bind_flags, resource_name);

    return true;
}

bool D3D11ProxySwapchain::CreateResources(const XrSwapchainCreateInfo* createInfo, D3D11_USAGE usage, uint32_t bind_flags, std::wstring resource_name) {
    resolution_x = createInfo->width;
    resolution_y = createInfo->height;
    current_image_state.assign(back_buffer_count, IMAGE_STATE_RELEASED);
    current_image_state.shrink_to_fit();

    // Initialize resource vectors
    if (bind_flags & D3D11_BIND_DEPTH_STENCIL) {
        // TODO Check if a single depth map is enough
        back_buffers.resize(back_buffer_count);
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

    back_buffers.shrink_to_fit();
    render_target_views.shrink_to_fit();
    shader_resource_views.shrink_to_fit();
    depth_stencil_views.shrink_to_fit();

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

    auto device = d3d11_renderer->GetDevice();

    // Check multi sample quality
    uint32_t sample_quality;
    HRESULT res = device->CheckMultisampleQualityLevels(texture_desc.Format, texture_desc.SampleDesc.Count, &sample_quality);
    sample_quality--;
    if (SUCCEEDED(res)) {
        texture_desc.SampleDesc.Quality = glm::min<uint32_t>(texture_desc.SampleDesc.Quality, sample_quality);
    }
    else {
        LOG(WARNING) << "D3D11 Couldn't retrieve multi sample quality levels";
    }

    std::wstring com_name_prefix = L"";

    // Create resources
    for (uint32_t i = 0; i < back_buffers.size(); i++) {
        if (texture_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
            auto hr = device->CreateTexture2D(&texture_desc, nullptr, back_buffers[i].GetAddressOf());
            if (FAILED(hr)) {
                throw XrException(XR_ERROR_RUNTIME_FAILURE, "D3D11 Failed creating proxy swapchain depth texture");
            }

            D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
            ZeroMemory(&descDSV, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
            // Put all precision to the depth
            descDSV.Format = static_cast<DXGI_FORMAT>(createInfo->format);
            descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            descDSV.Texture2D.MipSlice = 0;

            hr = device->CreateDepthStencilView(back_buffers[i].Get(), &descDSV, depth_stencil_views[i].GetAddressOf());
            if (FAILED(hr)) {
                throw XrException(XR_ERROR_RUNTIME_FAILURE, "D3D11 Failed creating depth stencil view");
            }

            com_name_prefix = L"Depth ";

            // TODO Example releases the resource here?
            //back_buffers[i]->Release();
        }
        else {
            auto hr = device->CreateTexture2D(&texture_desc, nullptr, back_buffers[i].GetAddressOf());
            if (FAILED(hr)) {
                throw XrException(XR_ERROR_RUNTIME_FAILURE, "D3D11 Failed creating proxy swapchain texture");
            }

            D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
            ZeroMemory(&rtv_desc, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
            rtv_desc.Format = static_cast<DXGI_FORMAT>(createInfo->format);
            rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

            if (FAILED(device->CreateRenderTargetView(back_buffers[i].Get(), &rtv_desc, render_target_views[i].GetAddressOf()))) {
                throw XrException(XR_ERROR_RUNTIME_FAILURE, "D3D11 Failed creating proxy swapchain render target view");
            }

            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
            ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
            srv_desc.Format = static_cast<DXGI_FORMAT>(createInfo->format);
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = createInfo->mipCount;
            srv_desc.Texture2D.MostDetailedMip = 0;

            hr = device->CreateShaderResourceView(back_buffers[i].Get(), &srv_desc, shader_resource_views[i].GetAddressOf());
            if (FAILED(hr)) {
                throw XrException(XR_ERROR_RUNTIME_FAILURE, "D3D11 Failed creating proxy swapchain shader resource view");
            }

            // TODO Example releases the resource here?
            //back_buffers[i]->Release();
        }

        // Choose name for debugging
        std::wstring texname;
        std::wstring rtvname;
        std::wstring srvname;
        size_t handle = reinterpret_cast<size_t>(xr_handle);
        if (resource_name.empty()) {
            texname = std::format(L"Proxy Swapchain {} Texture {}", handle, i);
            texname = com_name_prefix + texname;

            rtvname = std::format(L"Proxy Swapchain {} Render Target View {}", handle, i);
            rtvname = com_name_prefix + texname;

            srvname = std::format(L"Proxy Swapchain {} Shader Resource View {}", handle, i);
            srvname = com_name_prefix + texname;

            proxy_name = texname;
        }
        else {
            texname = std::format(L"{} {} Texture {}", resource_name, handle, i);
            texname = com_name_prefix + texname;

            rtvname = std::format(L"{} {} Render Target View {}", resource_name, handle, i);
            rtvname = com_name_prefix + texname;

            srvname = std::format(L"{} {} Shader Resource View {}",resource_name, handle, i);
            srvname = com_name_prefix + texname;

            proxy_name = texname;
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

std::vector<ComPtr<ID3D11Texture2D>> D3D11ProxySwapchain::GetBuffers()
{
	return back_buffers;
}

std::vector<ComPtr<ID3D11ShaderResourceView>> D3D11ProxySwapchain::GetShaderResourceViews()
{
	return shader_resource_views;
}

std::vector<ComPtr<ID3D11RenderTargetView>> D3D11ProxySwapchain::GetRenderTargetViews()
{
	return render_target_views;
}

bool D3D11ProxySwapchain::IsDepthResource()
{
	return is_depth_resource;
}

uint32_t D3D11ProxySwapchain::GetAwaitedImageIndex()
{
    return awaited_frame_index;
}

void GetResourceStateFlags(XrSwapchainUsageFlags usage_flags, D3D11_USAGE& usage, uint32_t& bind_flags) {
    bind_flags = 1;
    if (XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT & usage_flags) {
        bind_flags = D3D11_BIND_RENDER_TARGET;
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
        // Ignored
        // usage = D3D11_USAGE_DEFAULT;
    }
    if (XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT & usage_flags) {
        // Ignored
        // usage = D3D11_USAGE_DEFAULT;
    }
    if (XR_SWAPCHAIN_USAGE_SAMPLED_BIT & usage_flags) {
        bind_flags |= D3D11_BIND_SHADER_RESOURCE;
        usage = D3D11_USAGE_DEFAULT;
    }
    if (XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT & usage_flags) {
        // Ignored
        // usage = D3D11_USAGE_DEFAULT;
    }
}

D3D11WindowSwapchain::D3D11WindowSwapchain(D3D11Renderer* renderer, const XrSwapchainCreateInfo* createInfo, uint32_t back_buffer_count, HWND hwnd)
{
    d3d11_renderer = renderer;
    auto device = renderer->GetDevice();
    width = createInfo->width;
    height = createInfo->height;
    render_target_views.resize(1);
    render_target_views.shrink_to_fit();

    // TODO On failure all objects here should be destroyed
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    D3D12WindowSwapchain::CreateDXGIFactory(&factory);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = createInfo->width;
    swapChainDesc.Height = createInfo->height;
    swapChainDesc.Format = static_cast<DXGI_FORMAT>(createInfo->format);
    swapChainDesc.Stereo = false;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER;
    swapChainDesc.BufferCount = back_buffer_count;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH; //DXGI_SCALING_ASPECT_RATIO_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
    fsSwapChainDesc.Windowed = TRUE;
    fsSwapChainDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;

    ComPtr<IDXGISwapChain1> swapChain;
    HRESULT res = factory->CreateSwapChainForHwnd(device.Get(), hwnd, &swapChainDesc, 0, nullptr, &swapChain);
    if (FAILED(res)) {
        LOG(ERROR) << "Failed to create d3d11 swap chain";
        throw std::runtime_error("Failed creating d3d11 swapchain");
    }
    if (FAILED(swapChain.As(&swap_chain))) {
        LOG(ERROR) << "Failed to get ComPtr object";
        throw std::runtime_error("Failed to get ComPtr object d3d11");
    }

    // Create render target view
    ID3D11Texture2D* back_buffer;
    // Since we are using DXGI_SWAP_EFFECT_FLIP_DISCARD, we can only access the first index of the buffers. See documentation for IDXGISwapChain::GetBuffer.
    swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
    D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    rtv_desc.Format = static_cast<DXGI_FORMAT>(createInfo->format);
    rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(back_buffer, &rtv_desc, &render_target_views[0]);
}

uint32_t D3D11WindowSwapchain::GetCurrentImageIndex()
{
    return swap_chain->GetCurrentBackBufferIndex();
}

void D3D11WindowSwapchain::PresentFrame()
{
    swap_chain->Present(1, 0);
}

uint32_t D3D11WindowSwapchain::GetWidth()
{
    return width;
}

uint32_t D3D11WindowSwapchain::GetHeight()
{
    return height;
}

uint32_t D3D11WindowSwapchain::GetBufferCount()
{
    return back_buffer_count;
}

Renderer* D3D11WindowSwapchain::GetRenderer()
{
    return d3d11_renderer;
}

std::vector<ComPtr<ID3D11Texture2D>> D3D11WindowSwapchain::GetBuffers()
{
    return std::vector<ComPtr<ID3D11Texture2D>>();
}

std::vector<ComPtr<ID3D11ShaderResourceView>> D3D11WindowSwapchain::GetShaderResourceViews()
{
    return std::vector<ComPtr<ID3D11ShaderResourceView>>();
}

std::vector<ComPtr<ID3D11RenderTargetView>> D3D11WindowSwapchain::GetRenderTargetViews()
{
    return render_target_views;
}
