#include "D3D12Renderer.h"

#include "instance.h"
#include "settings.h"

namespace XRGameBridge {
    XrResult D3D12Renderer::Initialize(GB_Instance* instance, XrSystemId systemId, const void* graphics_binding) {
        // Manage window
        // Manage compositors
        // Manage weaver
        GB_System& system = g_systems[systemId];

        const XrGraphicsBindingD3D12KHR* d3d12_bindings = static_cast<const XrGraphicsBindingD3D12KHR*> (graphics_binding);

        { // Check validity of the device
            const ID3D12Object* obj = dynamic_cast<ID3D12Object*> (d3d12_bindings->device);
            if (!obj) {
                return XR_ERROR_GRAPHICS_DEVICE_INVALID;
            }
        }

        d3d12_device = d3d12_bindings->device;
        d3d12_command_queue = d3d12_bindings->queue;

        GB_DX12Compositor* d3d12_compositor = new GB_DX12Compositor();
        if (d3d12_compositor->Initialize(d3d12_bindings, backbuffer_count) == false) {
            LOG(ERROR) << "Failed to create compositor";
            return XR_ERROR_RUNTIME_FAILURE;
        }

        compositor = d3d12_compositor;


        CreateWeaver();
        CreateIntermediateTexture();
        CreateSystemWindow();
        CreateWindowSwapchain();

        return XR_SUCCESS;
    }

    XrResult D3D12Renderer::CreateIntermediateTexture() {
        // Create intermediate resources for weaving render target
        // TODO Remove session parameter

        // Handle 0 is not being used by xrCreateSwapchain
        intermediate_resource = new GB_D3D12ProxySwapchain(0, this);

        auto system_resolution = GetSystemResolution(gb_system);
        intermediate_resource->CreateResources(system_resolution.x, system_resolution.y, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET, L"Intermediate resource");

        return XR_SUCCESS;
    }

    XrResult D3D12Renderer::CreateWeaver() {
        // Initialize weaver params
        DX12WeaverInitialize params{};
        params.command_queue = d3d12_command_queue;
        params.device = d3d12_device;
        params.game_bridge = GetGameBridgeInstance();
        params.input_resource = intermediate_resource->GetBuffers()[0];
        params.render_target = window_swapchain.GetImages()[0];
        params.window = window.GetWindowHandle();

        d3d12weaver = new DirectX12Weaver(params);
        d3d12weaver->InitializeWeaver(gb_session.sr_context);
        sr_context->initialize();

        return XR_SUCCESS;
    }

    XrResult D3D12Renderer::CreateSystemWindow() {
        if (window.TryGetExternalDisplay() != nullptr) {
            LOG(INFO) << "Got window";
        }

        // Create debug window
        auto system_resolution = GetSystemResolution(gb_system);

        window.CreateApplicationWindow(XRGameBridge::g_runtime_settings.hInst, gb_system, system_resolution.x, system_resolution.y, true, true);
        // Debugging with non full screen mode
        //gb_session.display.CreateApplicationWindow(XRGameBridge::g_runtime_settings.hInst, 2560, 1440, true, false, true);

        return XR_SUCCESS;
    }

    XrResult D3D12Renderer::CreateWindowSwapchain() {
        // Create swapchain info for the window swapchain
        auto system_resolution = GetSystemResolution(gb_system);
        XrSwapchainCreateInfo window_swapchain_info;
        window_swapchain_info.width = system_resolution.x;
        window_swapchain_info.height = system_resolution.y;
        window_swapchain_info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        window_swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

        // Create swapchain for debug window
        window_swapchain.CreateSwapChain(d3d12_device, d3d12_command_queue, &window_swapchain_info, window.GetWindowHandle());

        return XR_SUCCESS;
    }

    GraphicsBackend D3D12Renderer::GetGraphicsBackend() {
        return GraphicsBackend::D3D12;
    }

    GB_Compositor* const D3D12Renderer::GetCompositor() {
        return compositor;
    }

    D3D12Renderer::~D3D12Renderer() {
        // Destroy resources created by BeginSession
        delete d3d12weaver;
        d3d12weaver = nullptr;

        intermediate_resource->DestroyResources();

        compositor->Destroy();

        //gb_session.window = {};
        delete d3d12weaver;

        // Reset window swapchain
        window_swapchain = {};

        // Destroy window
        window.DestroyApplicationWindow();

        d3d12_command_queue.Reset();

        d3d12_device.Reset();
    }
}
