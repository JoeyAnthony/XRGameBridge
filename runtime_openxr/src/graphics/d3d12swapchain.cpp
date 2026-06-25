/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "d3d12swapchain.h"

#include <stdexcept>
#include <exception>
#include <vector>

#include <wrl/client.h>

#include "debug.h"
#include "dxhelpers.h"
#include "d3d12renderer.h"

D3D12ProxySwapchain::~D3D12ProxySwapchain()
{
    DestroyResources();
}

D3D12ProxySwapchain* D3D12ProxySwapchain::Create(const XrSwapchainCreateInfo* createInfo, D3D12Renderer* renderer, std::string resource_name) {
    static size_t swapchain_creation_count = 1;
    // Create handle
    XrSwapchain handle = reinterpret_cast<XrSwapchain>(swapchain_creation_count);
    auto d3d12_proxy = new D3D12ProxySwapchain(handle, renderer);

    // Initialize resources. Double buffering is standard.
    if (d3d12_proxy->CreateResources(createInfo, standard_swapchain_buffer_count) == false) {
        throw XrException(XR_ERROR_RUNTIME_FAILURE, "Failed to create D3D12 proxy swapchain");
    }

    swapchain_creation_count++;
    return d3d12_proxy;
}

D3D12ProxySwapchain::D3D12ProxySwapchain(XrSwapchain handle, D3D12Renderer* renderer) : ProxySwapchain(handle) {
    d3d12_renderer = renderer;
}

bool D3D12ProxySwapchain::CreateResources(const XrSwapchainCreateInfo* createInfo, uint32_t num_resources, std::string resource_name) {
    DXGI_FORMAT format = static_cast<DXGI_FORMAT>(createInfo->format);
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_COMMON;
    ID3D12Device* device = d3d12_renderer->GetDevice().Get();
    HRESULT res = 0;

    GetResourceStateFlags(createInfo->usageFlags, flags, states);

    if (createInfo->createFlags & XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT) {
        if (num_resources != 1) {
            spdlog::warn("Swapchain is static but num_resources does not equal 1. Forcing resources to equal 1");
        }
        back_buffer_count = 1;
    }
    else {
        back_buffer_count = num_resources;
    }

    resolution_x = createInfo->width;
    resolution_y = createInfo->height;
    // Reinitialize the values in the array
    back_buffer_fence_values.assign(back_buffer_count, 0);
    current_image_state.assign(back_buffer_count, IMAGE_STATE_RELEASED);
    back_buffer_fence_values.shrink_to_fit();
    current_image_state.shrink_to_fit();
    back_buffers.resize(back_buffer_count);

    for (uint32_t i = 0; i < back_buffer_count; i++) {
        spdlog::info("Creating Depth resource");
        // Set resource_usage to save the state the application expects the buffer to be in
        resource_usage = states;

        std::string com_name_prefix = "";

        // TODO For depth resources only a single one is needed. For simplicity and to save time, I'll leave it to the back_buffer count for now.
        // Create depth stencil
        if (states == D3D12_RESOURCE_STATE_DEPTH_WRITE) {
            is_depth_resource = true;

            // Describe and create a Texture2D.
            D3D12_RESOURCE_DESC texture_desc = {};
            texture_desc.Format = format; // DXGI_FORMAT_D32_FLOAT;
            texture_desc.Width = createInfo->width;
            texture_desc.Height = createInfo->height;
            texture_desc.DepthOrArraySize = createInfo->arraySize;
            texture_desc.MipLevels = createInfo->mipCount;
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
                spdlog::error("D3D12 Error, failed creating proxy swapchain depth resource: {}", proxy_name);
                ThrowIfFailed(res);
                return false;
            }

            D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_desc = {};
            depth_stencil_desc.Format = format;
            depth_stencil_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            depth_stencil_desc.Flags = D3D12_DSV_FLAG_NONE;

            // Dsv descriptors are not necessary for now
            //device->CreateDepthStencilView(m_depthStencil.Get(), &depth_stencil_desc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

            com_name_prefix = "Depth ";
        }
        // Create render target
        else {
            spdlog::info("Creating Color resource");
            // Describe and create a Texture2D.
            D3D12_RESOURCE_DESC texture_desc = {};
            texture_desc.Format = format;
            texture_desc.Width = createInfo->width;
            texture_desc.Height = createInfo->height;
            texture_desc.DepthOrArraySize = createInfo->arraySize;
            texture_desc.MipLevels = createInfo->mipCount;
            texture_desc.Flags = flags;
            texture_desc.SampleDesc.Count = 1;
            texture_desc.SampleDesc.Quality = 0;
            texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

            D3D12_CLEAR_VALUE clear_value{
                format
            };

            memcpy(clear_value.Color, Renderer::clear_color, sizeof(float) * 4);

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
                spdlog::error("D3D12 Error, failed creating proxy swapchain resource: {}", proxy_name);
                ThrowIfFailed(res);
                return false;
            }
        }

        // Choose name for debugging
        if (resource_name.empty()) {
            std::string name = com_name_prefix + std::format("Proxy Swapchain {} Resource {}", reinterpret_cast<size_t>(xr_handle), i);
            proxy_name = com_name_prefix + std::format("Proxy Swapchain {}", reinterpret_cast<size_t>(xr_handle));
        }
        else {
            std::string name = com_name_prefix + std::format("{} {} Resource {}", resource_name, reinterpret_cast<size_t>(xr_handle), i);
            proxy_name = com_name_prefix + std::format("{} {}", resource_name, reinterpret_cast<size_t>(xr_handle));
        }

        // Give name to the buffer
        std::wstring wide_name;
        wide_name.resize(proxy_name.size()+1);
        int convertResult = MultiByteToWideChar(CP_UTF8, 0, proxy_name.c_str(), -1, wide_name.data(), wide_name.size());
        back_buffers[i]->SetName(wide_name.c_str());
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
                spdlog::error("Failed to create d3d12 rtv descriptor heap");
                return false;
            }

            // TODO we create an srv heap here but not srv's themselves later on
            // Describe and create a shader resource view (SRV) heap for the texture.
            D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
            srvHeapDesc.NumDescriptors = back_buffer_count;
            srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srv_heap)))) {
                spdlog::error("Failed to create d3d12 srv descriptor heap");
                return false;
            }
        }

        rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        cbc_srv_uav_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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

    std::stringstream ss;
    ss << std::format("Successfully created proxy swapchain resources:") << "\n";
    ss << std::format("swapchain {}", proxy_name) << "\n";
    ss << std::format("width: {}", GetWidth()) << "\n";
    ss << std::format("height: {}", GetHeight()) << "\n";
    ss << std::format("buffer count: {}", GetBufferCount()) << "\n";
    spdlog::info(ss.str());

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

    std::stringstream ss;
    ss << std::format("Destroyed proxy swapchain resources:") << "\n";
    ss << std::format("swapchain {}", reinterpret_cast<size_t>(xr_handle)) << "\n";
    spdlog::info(ss.str());
}

size_t D3D12ProxySwapchain::GetBufferCount() {
    return back_buffers.size();
}

const std::vector<ComPtr<ID3D12Resource>> D3D12ProxySwapchain::GetBuffers() {
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

    spdlog::debug("Acquired Proxy Swapchain {} Image {}", reinterpret_cast<size_t>(xr_handle), index);
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

    spdlog::debug("Awaited Proxy Swapchain {} Image {}", reinterpret_cast<size_t>(xr_handle), current_frame_index);
    return XR_SUCCESS;
}

XrResult D3D12ProxySwapchain::ReleaseImage() {
    if (current_image_state[awaited_frame_index] != IMAGE_STATE_RENDER_TARGET) {
        return XR_ERROR_CALL_ORDER_INVALID;
    }

    current_image_state[awaited_frame_index] = IMAGE_STATE_RELEASED;

    released_frame_index = awaited_frame_index;

    //std::stringstream ss; ss << "px - "
    //    << " swapchain: " << proxy_name
    //    << " aqcuired index " << current_frame_index
    //    << " awaited index " << awaited_frame_index
    //    << " released index " << released_frame_index
    //    ;
    //spdlog::debug(ss.str());

    spdlog::debug("Released Proxy Swapchain {} Image {}", reinterpret_cast<size_t>(xr_handle), released_frame_index);
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

bool D3D12WindowSwapchain::CreateSwapChain(const XrSwapchainCreateInfo* createInfo, HWND hwnd) {
    spdlog::info("Creating DX12 window swapchain");
    ID3D12Device* device = d3d12_renderer->GetDevice().Get();
    ID3D12CommandQueue* queue = d3d12_renderer->GetCommandQueue().Get();

    // Use double buffering for rendering to the window for now.
    const uint32_t back_buffer_count = standard_swapchain_buffer_count;
    back_buffers.resize(back_buffer_count);

    // TODO On failure all objects here should be destroyed
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    DxHelpers::CreateDXGIFactory(&factory);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = createInfo->width;
    swapChainDesc.Height = createInfo->height;
    swapChainDesc.Format = static_cast<DXGI_FORMAT>(createInfo->format);
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER;
    swapChainDesc.BufferCount = back_buffer_count;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
    fsSwapChainDesc.Windowed = TRUE;

    // Swap chain needs the queue so that it can force a flush on it.
    ComPtr<IDXGISwapChain1> swapChain;
    HRESULT res = factory->CreateSwapChainForHwnd(queue, hwnd, &swapChainDesc, &fsSwapChainDesc, nullptr, &swapChain);
    if (FAILED(res)) {
        spdlog::error("Failed to create d3d12 swap chain");
        return false;
    }
    if (FAILED(swapChain.As(&swap_chain))) {
        spdlog::error("Failed to get ComPtr object");
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
            spdlog::error("Failed to create d3d12 rtv descriptor heap");
            return false;
        }

        // TODO we create an srv heap here but not srv's themselves later on
        // Describe and create a shader resource view (SRV) heap for the texture.
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = back_buffer_count;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)))) {
            spdlog::error("Failed to create d3d12 srv descriptor heap");
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
                spdlog::error("Failed to create rtv");
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

    std::stringstream ss;
    ss << std::format("Successfully created window swapchain resources:") << "\n";
    ss << std::format("width: {}", createInfo->width) << "\n";
    ss << std::format("height: {}", createInfo->height) << "\n";
    ss << std::format("buffer count: {}", back_buffers.size()) << "\n";
    spdlog::info(ss.str());

    return true;
}

void D3D12WindowSwapchain::Initialize(D3D12Renderer* renderer) {
    d3d12_renderer = renderer;
}

const std::vector<ComPtr<ID3D12Resource>> D3D12WindowSwapchain::GetImages() {
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

uint32_t D3D12WindowSwapchain::AcquireNextImage() {
    // TODO get image index from the swapchain
    return swap_chain->GetCurrentBackBufferIndex();
}

void D3D12WindowSwapchain::PresentFrame() {
    swap_chain->Present(1, 0);
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
        spdlog::info("Test");
    }
    if (XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT & usage_flags) {
        // Ignored for D3D12
        spdlog::info("Test");
    }
    if (XR_SWAPCHAIN_USAGE_SAMPLED_BIT & usage_flags) {
        // Omitted for D3D12
        //states = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        spdlog::info("Test");
    }
    if (XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT & usage_flags) {
        // Ignored for D3D12
        //usage |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
        spdlog::info("Test");
    }
}
