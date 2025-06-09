#include "swapchain.h"

#include <stdexcept>
#include <exception>
#include <vector>

#include <wrl/client.h>

#include "easylogging++.h"
#include "openxr_functions.h"
#include "instance.h"
#include "settings.h"
#include "d3d11swapchain.h"
#include "D3D12Renderer.h"

XrResult xrEnumerateSwapchainFormats(XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats) {
    GraphicsBackend backend;

    try {
        GB_Session& gb_session = g_sessions.at(session);
        backend = gb_session.renderer->GetGraphicsBackend();
    }
    catch (std::out_of_range& e) {
        return XR_ERROR_SESSION_LOST;
    }
    catch (std::exception& e) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    std::vector<int64_t> supported_swapchain_formats;
    if (backend == GraphicsBackend::D3D12 || backend == GraphicsBackend::D3D11) {
        supported_swapchain_formats.push_back(DXGI_FORMAT_R8G8B8A8_UNORM);
        supported_swapchain_formats.push_back(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    }
    else {
        // not implemented
        LOG(ERROR) << "Graphics backend not supported";
        return XR_ERROR_RUNTIME_FAILURE;
    }

    uint32_t count = supported_swapchain_formats.size();
    *formatCountOutput = count;

    if (formatCapacityInput == 0) {
        return XR_SUCCESS;
    }
    // Passed array not large enough
    if (formatCapacityInput < count) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    // Fill array
    memcpy_s(formats, formatCapacityInput * sizeof(int64_t), supported_swapchain_formats.data(), supported_swapchain_formats.size() * sizeof(int64_t));

    return XR_SUCCESS;
}

XrResult xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain) {
    //TODO Get compositor from the session and create descriptor on it for the new swapchain
    GB_Session& gb_session = g_sessions[session];

    ProxySwapchain* proxy_swapchain = nullptr;
    Renderer* renderer = gb_session.renderer;
    GraphicsBackend backend = renderer->GetGraphicsBackend();
    if (backend == GraphicsBackend::D3D12) {
        auto* d3d12_renderer = reinterpret_cast<D3D12Renderer*>(gb_session.renderer);
        proxy_swapchain = D3D12ProxySwapchain::Create(createInfo, d3d12_renderer);

        // Create swap chain
        if (proxy_swapchain->CreateResources(createInfo) == false) {
            LOG(ERROR) << "Failed to create proxy swapchain";
            return XR_ERROR_RUNTIME_FAILURE;
        }
    }
    else if (backend == GraphicsBackend::D3D11) {
        auto* d3d11_renderer = reinterpret_cast<D3D11Renderer*>(gb_session.renderer);
        proxy_swapchain = D3D11ProxySwapchain::Create(createInfo, d3d11_renderer);
    }
    else {
        // Not implemented
        LOG(ERROR) << "Graphics backend not supported";
        return XR_ERROR_RUNTIME_FAILURE;
    }

    *swapchain = proxy_swapchain->GetHandle();
    g_proxy_swapchains[proxy_swapchain->GetHandle()] = proxy_swapchain;

    LOG(INFO) << "Successfully created proxy swapchain";
    return XR_SUCCESS;
}

XrResult xrDestroySwapchain(XrSwapchain swapchain) {
    auto& gb_proxy = g_proxy_swapchains[swapchain];

    gb_proxy->DestroyResources();

    g_proxy_swapchains.erase(swapchain);

    return XR_SUCCESS;
}

XrResult xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images) {
    //TODO Create actual swap chains over here

    auto& gb_render_target = g_proxy_swapchains[swapchain];
    uint32_t count = gb_render_target->GetBufferCount();
    GraphicsBackend backend = gb_render_target->GetRenderer()->GetGraphicsBackend();

    *imageCountOutput = count;

    if (imageCapacityInput == 0) {
        return XR_SUCCESS;
    }
    // Passed array not large enough
    if (imageCapacityInput < count) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    if (backend == GraphicsBackend::D3D12) {
        // Check requested images with the swapchain type
        D3D12ProxySwapchain* proxy = dynamic_cast<D3D12ProxySwapchain*>(gb_render_target);
        if (!proxy) {
            LOG(ERROR) << "Wrong proxy swapchain class type";
            return XR_ERROR_RUNTIME_FAILURE;
        }

        if (images[0].type != XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR) {
            LOG(ERROR) << "structure type incompatible";
            return XR_ERROR_VALIDATION_FAILURE;
        }

        std::vector<XrSwapchainImageD3D12KHR> xr_images;
        auto directx_images = proxy->GetBuffers();
        for (uint32_t i = 0; i < count; i++) {
            XrSwapchainImageD3D12KHR image{};
            image.type = XrStructureType::XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR;
            image.texture = directx_images[i].Get();
            xr_images.push_back(image);
        }

        // Fill array
        memcpy_s(images, imageCapacityInput * sizeof(XrSwapchainImageD3D12KHR), xr_images.data(), xr_images.size() * sizeof(XrSwapchainImageD3D12KHR));
        return XR_SUCCESS;
    }
    else if (backend == GraphicsBackend::D3D11){
        // Check casting
        D3D11ProxySwapchain* proxy = dynamic_cast<D3D11ProxySwapchain*>(gb_render_target);
        if(!proxy) {
            LOG(ERROR) << "Wrong proxy swapchain class type";
            return XR_ERROR_RUNTIME_FAILURE;
        }

        // Check swapchain type
        if (images[0].type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
            LOG(ERROR) << "structure type incompatible";
            return XR_ERROR_VALIDATION_FAILURE;
        }

        std::vector<XrSwapchainImageD3D11KHR> xr_images;
        auto directx_images = proxy->GetBuffers();
        for (uint32_t i = 0; i < count; i++) {
            XrSwapchainImageD3D11KHR image{};
            image.type = XrStructureType::XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR;
            image.texture = directx_images[i].Get();
            xr_images.push_back(image);
        }

        // Fill array
        memcpy_s(images, imageCapacityInput * sizeof(XrSwapchainImageD3D11KHR), xr_images.data(), xr_images.size() * sizeof(XrSwapchainImageD3D11KHR));
        return XR_SUCCESS;
    }
    else {
        // Not implemented
        LOG(ERROR) << "Graphics backend not supported";
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return XR_ERROR_RUNTIME_FAILURE;
}

XrResult xrEnumerateBoundSourcesForAction(XrSession session, const XrBoundSourcesForActionEnumerateInfo* enumerateInfo, uint32_t sourceCapacityInput, uint32_t* sourceCountOutput, XrPath* sources) {
    // TODO don't think we need this function anytime soon
    LOG(INFO) << "Unimplemented " << __func__;
    return XR_ERROR_RUNTIME_FAILURE;
}

XrResult xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index) {
    //TODO May only be called again AFTER xrReleaseSwapchainImage has been called. See specification.
    // return XR_ERROR_CALL_ORDER_INVALID

    auto& gb_proxy = g_proxy_swapchains[swapchain];
    uint32_t i = 0;
    XrResult res = gb_proxy->AcquireNextImage(i);
    *index = i;
    return res;
}

XrResult xrWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo) {
    //TODO see specification for other waiting requirements

    uint64_t wait_duration = INFINITE;
    if (waitInfo->timeout != XR_INFINITE_DURATION) {
        wait_duration = waitInfo->timeout;
    }

    auto& gb_proxy = g_proxy_swapchains[swapchain];

    return gb_proxy->WaitForImage(wait_duration);
}

XrResult xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo) {
    // Basically tells the runtime that the application is done with an image

    auto& gb_proxy = g_proxy_swapchains[swapchain];
    return gb_proxy->ReleaseImage();
}

D3D12ProxySwapchain* D3D12ProxySwapchain::Create(const XrSwapchainCreateInfo* createInfo, D3D12Renderer* renderer) {
    static size_t swapchain_creation_count = 1;
    // Create handle
    XrSwapchain handle = reinterpret_cast<XrSwapchain>(swapchain_creation_count);
    auto d3d12_proxy = new D3D12ProxySwapchain(handle, renderer);

    // Initialize resources
    // TODO fix later to use create info
    //XrResult result = XR_ERROR_RUNTIME_FAILURE;
    //if (d3d12_proxy->CreateResources(createInfo) == false) {
    //    throw XrException("Failed to create D3D12 proxy swapchain", XR_ERROR_RUNTIME_FAILURE);
    //}

    swapchain_creation_count++;
    return d3d12_proxy;
}

D3D12ProxySwapchain::D3D12ProxySwapchain(XrSwapchain handle, D3D12Renderer* renderer) : ProxySwapchain(handle) {
    d3d12_renderer = renderer;
    back_buffer_fence_values.fill(0);
    current_image_state.fill(ImageState::IMAGE_STATE_WAITING);
}

bool D3D12ProxySwapchain::CreateResources(const XrSwapchainCreateInfo* createInfo, std::wstring resource_name) {
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_COMMON;
    GetResourceStateFlags(createInfo->usageFlags, flags, states);
    if (states == D3D12_RESOURCE_STATE_COMMON) {
        states = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    return CreateResources(createInfo->width, createInfo->height, static_cast<DXGI_FORMAT>(createInfo->format), flags, states, resource_name);
}

bool D3D12ProxySwapchain::CreateResources(uint32_t width, uint32_t height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES states, std::wstring resource_name) {
    ID3D12Device* device = d3d12_renderer->GetDevice().Get();

    HRESULT res = 0;
    // Reinitialize the values in the array
    current_image_state.fill(IMAGE_STATE_RELEASED);

    for (uint32_t i = 0; i < back_buffer_count; i++) {
        // Set resource_usage to save the state the application expects the buffer to be in
        resource_usage = states;

        std::wstring com_name_prefix = L"";

        // TODO For depth resources only a single one is needed. For simplicity and to save time, I'll leave it to the back_buffer count for now.
        // Create depth stencil
        if (states == D3D12_RESOURCE_STATE_DEPTH_WRITE) {
            is_depth_resource = true;

            // Describe and create a Texture2D.
            D3D12_RESOURCE_DESC texture_desc = {};
            texture_desc.Format = format; // DXGI_FORMAT_D32_FLOAT;
            texture_desc.Width = width;
            texture_desc.Height = height;
            texture_desc.DepthOrArraySize = 1;
            texture_desc.MipLevels = 1;
            texture_desc.Flags = flags;
            texture_desc.SampleDesc.Count = 1;
            texture_desc.SampleDesc.Quality = 0;
            texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            D3D12_CLEAR_VALUE depth_optimized_clear_value = {};
            depth_optimized_clear_value.Format = format; // DXGI_FORMAT_D32_FLOAT;
            depth_optimized_clear_value.DepthStencil.Depth = 1.0f;
            depth_optimized_clear_value.DepthStencil.Stencil = 0;

            auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

            res = device->CreateCommittedResource(
                &heap_properties,
                D3D12_HEAP_FLAG_NONE,
                &texture_desc,
                states,
                &depth_optimized_clear_value,
                IID_PPV_ARGS(&back_buffers[i])
            );
            if (FAILED(res)) {
                HRESULT reason = device->GetDeviceRemovedReason();
                //D3D12_ERROR_ADAPTER_NOT_FOUND
                LOG(ERROR) << "D3D12 Error, failed creating proxy swapchain depth resource: " << proxy_name;
                ThrowIfFailed(res);
                return false;
            }

            D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_desc = {};
            depth_stencil_desc.Format = format;
            depth_stencil_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            depth_stencil_desc.Flags = D3D12_DSV_FLAG_NONE;

            // Dsv descriptors are not necessary for now
            //device->CreateDepthStencilView(m_depthStencil.Get(), &depth_stencil_desc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

            com_name_prefix = L"Depth ";
        }
        // Create render target
        else {
            // Describe and create a Texture2D.
            D3D12_RESOURCE_DESC texture_desc = {};
            texture_desc.Format = format;
            texture_desc.Width = width;
            texture_desc.Height = height;
            texture_desc.DepthOrArraySize = 1;
            texture_desc.MipLevels = 1;
            texture_desc.Flags = flags;
            texture_desc.SampleDesc.Count = 1;
            texture_desc.SampleDesc.Quality = 0;
            texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

            D3D12_CLEAR_VALUE clear_value{
                format
            };

            memcpy(clear_value.Color, clear_color, sizeof(float) * 4);

            res = device->CreateCommittedResource(
                &heap_properties,
                D3D12_HEAP_FLAG_NONE,
                &texture_desc,
                states,
                &clear_value,
                IID_PPV_ARGS(&back_buffers[i]));
            if (FAILED(res)) {
                HRESULT reason = device->GetDeviceRemovedReason();
                //D3D12_ERROR_ADAPTER_NOT_FOUND
                LOG(ERROR) << "D3D12 Error, failed creating proxy swapchain resource: " << proxy_name;
                ThrowIfFailed(res);
                return false;
            }
        }

        // Choose name for debugging
        if (resource_name.empty()) {
            std::wstring name = std::format(L"Proxy Swapchain {} Resource {}", reinterpret_cast<size_t>(xr_handle), i);
            name = com_name_prefix + name;
            proxy_name = name;
        }
        else {
            std::wstring name = std::format(L"{} {} Resource {}", resource_name, reinterpret_cast<size_t>(xr_handle), i);
            proxy_name = name;
        }

        // Give name to the buffer
        back_buffers[i]->SetName(proxy_name.c_str());
    }

    // Don't create descriptors for depth resources
    if (states == D3D12_RESOURCE_STATE_DEPTH_WRITE) {
        is_depth_resource = true;
    }
    else {
        // Create descriptor heaps.
        {

            // Describe and create a render target view (RTV) descriptor heap.
            D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
            rtvHeapDesc.NumDescriptors = back_buffer_count;
            rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            if (FAILED(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtv_heap)))) {
                LOG(ERROR) << "Failed to create d3d12 rtv descriptor heap";
                return false;
            }

            // TODO we create an srv heap here but not srv's themselves later on
            // Describe and create a shader resource view (SRV) heap for the texture.
            D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
            srvHeapDesc.NumDescriptors = back_buffer_count;
            srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srv_heap)))) {
                LOG(ERROR) << "Failed to create d3d12 srv descriptor heap";
                return false;
            }
        }

        rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        cbc_srv_uav_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        resolution_x = width;
        resolution_y = height;

        // Create descriptors
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_heap->GetCPUDescriptorHandleForHeapStart());
            CD3DX12_CPU_DESCRIPTOR_HANDLE srv_handle(srv_heap->GetCPUDescriptorHandleForHeapStart());

            for (int32_t i = 0; i < back_buffer_count; i++) {
                //std::wstringstream ss; ss << "Swap Container Resource: " << i;
                //back_buffers[i]->SetName(ss.str().c_str());

                // Create a RTV for each resource.
                device->CreateRenderTargetView(back_buffers[i].Get(), nullptr, rtv_handle);
                rtv_handle.Offset(1, rtv_descriptor_size);

                D3D12_TEX2D_SRV tex2d{};
                tex2d.MipLevels = 1;
                tex2d.MostDetailedMip = 0;
                tex2d.PlaneSlice = 0;
                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
                srv_desc.Format = format;
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE2D;
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.Texture2D = tex2d;
                // Create SRV for each resource
                device->CreateShaderResourceView(back_buffers[i].Get(), &srv_desc, srv_handle);
                srv_handle.Offset(1, cbc_srv_uav_descriptor_size);
            }
        }
    }

    return true;
}

void D3D12ProxySwapchain::DestroyResources() {
    d3d12_renderer->ResetCommandLists();

    for (int32_t i = 0; i < GetBufferCount(); i++) {
        d3d12_renderer->WaitFenceSwapchain(back_buffer_fence_values[i], XR_INFINITE_DURATION);
        back_buffers[i].Reset();
    }

    rtv_heap.Reset();
    srv_heap.Reset();
}

size_t D3D12ProxySwapchain::GetBufferCount() {
    return back_buffers.size();
}

std::array<ComPtr<ID3D12Resource>, back_buffer_count> D3D12ProxySwapchain::GetBuffers() {
    return back_buffers;
}

ComPtr<ID3D12DescriptorHeap>& D3D12ProxySwapchain::GetRtvHeap() {
    return rtv_heap;
}

ComPtr<ID3D12DescriptorHeap>& D3D12ProxySwapchain::GetSrvHeap() {
    return srv_heap;
}

uint32_t D3D12ProxySwapchain::GetRtvDescriptorSize() {
    return rtv_descriptor_size;
}

uint32_t D3D12ProxySwapchain::GetCbcSrvUavDescriptorSize() {
    return cbc_srv_uav_descriptor_size;
}

uint32_t D3D12ProxySwapchain::GetAwaitedImageIndex() {
    return awaited_frame_index;
}

XrResult D3D12ProxySwapchain::AcquireNextImage(uint32_t& index) {
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

XrResult D3D12ProxySwapchain::WaitForImage(const XrDuration& timeout) {
    if (current_image_state[current_frame_index] != IMAGE_STATE_ACQUIRED) {
        return XR_ERROR_CALL_ORDER_INVALID;
    }

    // TODO should wait when it's done presenting as well
    d3d12_renderer->WaitFenceSwapchain(back_buffer_fence_values[current_frame_index], timeout);

    // Set the image state to render target because we have waited for the image to be freed so it can be used by the application again.
    current_image_state[current_frame_index] = IMAGE_STATE_RENDER_TARGET;
    awaited_frame_index = current_frame_index;

    return XR_SUCCESS;
}

XrResult D3D12ProxySwapchain::ReleaseImage() {
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

uint32_t D3D12ProxySwapchain::GetWidth() {
    return resolution_x;
}

uint32_t D3D12ProxySwapchain::GetHeight() {
    return resolution_y;
}

void D3D12ProxySwapchain::SetReleasedImageFenceValue(uint32_t back_buffer_frame_num, uint64_t fence_value) {
    assert(fence_value >= back_buffer_fence_values[back_buffer_frame_num]); // New fence value should always be larger.
    back_buffer_fence_values[back_buffer_frame_num] = fence_value;
}

Renderer* D3D12ProxySwapchain::GetRenderer() {
    return d3d12_renderer;
}

void D3D12WindowSwapchain::CreateDXGIFactory(IDXGIFactory4** factory) {
    // Create a DXGIFactory object.
    UINT dxgi_factory_flags = 0;
    HRESULT err = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(factory));
    if (FAILED(err)) {
        LOG(ERROR) << "Could not create DXGIFactory with error: " << err;
    }
}

void D3D12WindowSwapchain::GetGraphicsAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter) {
    *ppAdapter = nullptr;

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6)))) {
        for (UINT adapterIndex = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(adapterIndex, requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED, IID_PPV_ARGS(&adapter))); ++adapterIndex) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                // Don't select the Basic Render Driver adapter.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
                break;
            }
        }
    }

    if (adapter.Get() == nullptr) {
        for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                // Don't select the Basic Render Driver adapter.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
                break;
            }
        }
    }

    *ppAdapter = adapter.Detach();
}

bool D3D12WindowSwapchain::CreateSwapChain(const XrSwapchainCreateInfo* createInfo, HWND hwnd) {
    ID3D12Device* device = d3d12_renderer->GetDevice().Get();
    ID3D12CommandQueue* queue = d3d12_renderer->GetCommandQueue().Get();

    // TODO On failure all objects here should be destroyed
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    D3D12WindowSwapchain::CreateDXGIFactory(&factory);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = createInfo->width;
    swapChainDesc.Height = createInfo->height;
    swapChainDesc.Format = static_cast<DXGI_FORMAT>(createInfo->format);
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER;
    swapChainDesc.BufferCount = back_buffer_count;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
    fsSwapChainDesc.Windowed = TRUE;

    // Swap chain needs the queue so that it can force a flush on it.
    ComPtr<IDXGISwapChain1> swapChain;
    HRESULT res = factory->CreateSwapChainForHwnd(queue, hwnd, &swapChainDesc, &fsSwapChainDesc, nullptr, &swapChain);
    if (FAILED(res)) {
        LOG(ERROR) << "Failed to create d3d12 swap chain";
        return false;
    }
    if (FAILED(swapChain.As(&swap_chain))) {
        LOG(ERROR) << "Failed to get ComPtr object";
        return false;
    }

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = back_buffer_count;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)))) {
            LOG(ERROR) << "Failed to create d3d12 rtv descriptor heap";
            return false;
        }

        // TODO we create an srv heap here but not srv's themselves later on
        // Describe and create a shader resource view (SRV) heap for the texture.
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = back_buffer_count;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)))) {
            LOG(ERROR) << "Failed to create d3d12 srv descriptor heap";
            return false;
        }

        rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (int32_t i = 0; i < back_buffer_count; i++) {
            if (FAILED(swap_chain->GetBuffer(i, IID_PPV_ARGS(&back_buffers[i])))) {
                LOG(ERROR) << "Failed to create rtv";
                return false;
            }
            std::wstringstream ss; ss << "Swapchain Buffer " << i;
            back_buffers[i]->SetName(ss.str().c_str());
            device->CreateRenderTargetView(back_buffers[i].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, rtv_descriptor_size);

            //Give name to swapchain buffers
            std::wstring name = std::format(L"GB Swapchain Resource {}", i);
            back_buffers[i]->SetName(name.c_str());
        }
    }

    return true;
}

void D3D12WindowSwapchain::Initialize(D3D12Renderer* renderer) {
    d3d12_renderer = renderer;
}

std::array<ComPtr<ID3D12Resource>, back_buffer_count> D3D12WindowSwapchain::GetImages() {
    return back_buffers;
}

ComPtr<ID3D12DescriptorHeap>& D3D12WindowSwapchain::GetRtvHeap() {
    return m_rtvHeap;
}

ComPtr<ID3D12DescriptorHeap>& D3D12WindowSwapchain::GetSrvHeap() {
    return m_srvHeap;
}

uint32_t D3D12WindowSwapchain::GetRtvDescriptorSize() {
    return rtv_descriptor_size;
}

uint32_t D3D12WindowSwapchain::GetCbcSrvUavDescriptorSize() {
    return GetCbcSrvUavDescriptorSize();
}

uint32_t D3D12WindowSwapchain::AcquireNextImage() {
    // TODO get image index from the swapchain
    return swap_chain->GetCurrentBackBufferIndex();
}

// Called from xrEndFrame, cause then we know the application is done with rendering this image.
void D3D12WindowSwapchain::PresentFrame() {
    // TODO Transitioning images state without waiting on the queue to finish, not sure this will break eventually. Maybe dx12 is synchronizing implicitly?
    //TransitionBackBufferImage(COMMAND_RESOURCE_INDEX_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    swap_chain->Present(1, 0);


    // barrier to render target
}

D3D12WindowSwapchain::D3D12WindowSwapchain() : d3d12_renderer(nullptr) {
}

void GetResourceStateFlags(XrSwapchainUsageFlags usage_flags, D3D12_RESOURCE_FLAGS& flags, D3D12_RESOURCE_STATES& states) {
    if (XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT & usage_flags) {
        flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        states = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    if (XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT & usage_flags) {
        flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        states = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    if (XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT & usage_flags) {
        flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    if (XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT & usage_flags) {
        // Ignored for D3D12
        LOG(INFO) << "Test";
    }
    if (XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT & usage_flags) {
        // Ignored for D3D12
        LOG(INFO) << "Test";
    }
    if (XR_SWAPCHAIN_USAGE_SAMPLED_BIT & usage_flags) {
        // Omitted for D3D12
        //states = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        LOG(INFO) << "Test";
    }
    if (XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT & usage_flags) {
        // Ignored for D3D12
        //usage |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
        LOG(INFO) << "Test";
    }
}
