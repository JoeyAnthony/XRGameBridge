/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "swapchain.h"

#include <set>

#include "instance.h"
#include "session.h"
#include "d3d11renderer.h"
#include "d3d11swapchain.h"
#include "d3d12renderer.h"
#include "d3d12swapchain.h"

XrResult xrEnumerateSwapchainFormats(XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats) {
    TraceLogFunctionCall(__func__, __LINE__);

    GraphicsBackend backend;
    std::set supported_formats{
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        DXGI_FORMAT_D16_UNORM,
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
        DXGI_FORMAT_D32_FLOAT,
        DXGI_FORMAT_D24_UNORM_S8_UINT,
    };

    try {
        XRSession& gb_session = g_sessions.at(session);
        backend = gb_session.renderer->GetGraphicsBackend();

        if (backend == GraphicsBackend::D3D11) {
            auto* d3d11_renderer = reinterpret_cast<D3D11Renderer*>(gb_session.renderer);
            for (auto it = supported_formats.begin(); it != supported_formats.end(); ++it) {
                unsigned int support;
                d3d11_renderer->GetDevice()->CheckFormatSupport(*it, &support);
                if (support & D3D11_FORMAT_SUPPORT_TEXTURE2D == 0) {
                    it = supported_formats.erase(it);
                }
            }
        }
    }
    catch (std::out_of_range& e) {
        LOG_RUNTIME_ERROR
            return XR_ERROR_SESSION_LOST;
    }
    catch (std::exception& e) {
        LOG_RUNTIME_ERROR
            return XR_ERROR_RUNTIME_FAILURE;
    }

    std::vector<int64_t> supported_swapchain_formats;
    if (backend == GraphicsBackend::D3D12 || backend == GraphicsBackend::D3D11) {
        supported_swapchain_formats.insert(supported_swapchain_formats.begin(), supported_formats.begin(), supported_formats.end());
    }
    else {
        // not implemented
        spdlog::error("Graphics backend not supported");
        LOG_RUNTIME_ERROR
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
    TraceLogFunctionCall(__func__, __LINE__);

    //TODO Get compositor from the session and create descriptor on it for the new swapchain
    XRSession& gb_session = g_sessions[session];

    /* TODO:
    * Get more consistent with error handling and logging.
    * Perhaps the following: Use exceptions where possible
    * Catch exceptions in these XR callbacks and return the error codes
    *
    * For feature checking, like with XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT here for example. It's easier to do that in the XR function directly
    * instead of doing that per graphics API for minimal duplicate work.
    *
    * Create logging functions for using in XR functions since throwing exceptions doesn't work here.
    *
    *
    * TODO:
    * Since this is the generic create swapchain function for the runtime, the image, resource_name can be generated here based on createInfo.
    */
    if (createInfo->createFlags & XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT) {
        spdlog::error("xrCreateSwapchain Error: XR_ERROR_FEATURE_UNSUPPORTED");
        return XR_ERROR_FEATURE_UNSUPPORTED;
    }

    ProxySwapchain* proxy_swapchain = nullptr;
    Renderer* renderer = gb_session.renderer;
    GraphicsBackend backend = renderer->GetGraphicsBackend();
    if (backend == GraphicsBackend::D3D12) {
        auto* d3d12_renderer = reinterpret_cast<D3D12Renderer*>(gb_session.renderer);
        proxy_swapchain = D3D12ProxySwapchain::Create(createInfo, d3d12_renderer);
    }
    else if (backend == GraphicsBackend::D3D11) {
        auto* d3d11_renderer = reinterpret_cast<D3D11Renderer*>(gb_session.renderer);
        proxy_swapchain = D3D11ProxySwapchain::Create(createInfo, d3d11_renderer);
    }
    else {
        // Not implemented
        spdlog::error("Graphics backend not supported");
        LOG_RUNTIME_ERROR
            return XR_ERROR_RUNTIME_FAILURE;
    }

    *swapchain = proxy_swapchain->GetHandle();
    g_proxy_swapchains[proxy_swapchain->GetHandle()] = proxy_swapchain;

    spdlog::info("Successfully created proxy swapchain {}: ", reinterpret_cast<size_t>(*swapchain));
    return XR_SUCCESS;
}

XrResult xrDestroySwapchain(XrSwapchain swapchain) {
    TraceLogFunctionCall(__func__, __LINE__);

    auto& gb_proxy = g_proxy_swapchains[swapchain];

    g_proxy_swapchains.erase(swapchain);

    return XR_SUCCESS;
}

XrResult xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images) {
    TraceLogFunctionCall(__func__, __LINE__);

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
            spdlog::error("Wrong proxy swapchain class type");
            LOG_RUNTIME_ERROR
                return XR_ERROR_RUNTIME_FAILURE;
        }

        if (images[0].type != XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR) {
            spdlog::error("structure type incompatible");
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
    else if (backend == GraphicsBackend::D3D11) {
        // Check casting
        D3D11ProxySwapchain* proxy = dynamic_cast<D3D11ProxySwapchain*>(gb_render_target);
        if (!proxy) {
            spdlog::error("Wrong proxy swapchain class type");
            LOG_RUNTIME_ERROR
                return XR_ERROR_RUNTIME_FAILURE;
        }

        // Check swapchain type
        if (images[0].type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
            spdlog::error("structure type incompatible");
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
        spdlog::error("Graphics backend not supported");
        LOG_RUNTIME_ERROR
            return XR_ERROR_RUNTIME_FAILURE;
    }
}

XrResult xrEnumerateBoundSourcesForAction(XrSession session, const XrBoundSourcesForActionEnumerateInfo* enumerateInfo, uint32_t sourceCapacityInput, uint32_t* sourceCountOutput, XrPath* sources) {
    TraceLogFunctionCall(__func__, __LINE__);

    // TODO don't think we need this function anytime soon
    spdlog::info("Unimplemented {}", __func__);
    LOG_RUNTIME_ERROR
        return XR_ERROR_RUNTIME_FAILURE;
}

XrResult xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index) {
    TraceLogFunctionCall(__func__, __LINE__);

    //TODO May only be called again AFTER xrReleaseSwapchainImage has been called. See specification.
    // return XR_ERROR_CALL_ORDER_INVALID

    auto& gb_proxy = g_proxy_swapchains[swapchain];
    uint32_t i = 0;
    XrResult res = gb_proxy->AcquireNextImage(i);
    *index = i;
    return res;
}

XrResult xrWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo) {
    TraceLogFunctionCall(__func__, __LINE__);

    //TODO see specification for other waiting requirements

    uint64_t wait_duration = INFINITE;
    if (waitInfo->timeout != XR_INFINITE_DURATION) {
        wait_duration = waitInfo->timeout;
    }

    auto& gb_proxy = g_proxy_swapchains[swapchain];

    XrResult xr_result;
    try {
        xr_result = gb_proxy->WaitForImage(wait_duration);
    }
    catch (XrException& e) {
        xr_result = e.GetResult();
    }

    return xr_result;
}

XrResult xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo) {
    TraceLogFunctionCall(__func__, __LINE__);

    // Basically tells the runtime that the application is done with an image

    auto& gb_proxy = g_proxy_swapchains[swapchain];
    return gb_proxy->ReleaseImage();
}
