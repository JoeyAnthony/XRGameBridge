#include "Renderer.h"
#include "renderer.h"

namespace XRGameBridge {

    Renderer::Renderer(GraphicsBackend backend, void* graphics_binding) {
        // Manage window
        // Manage compositors
        // Manage weaver

        if (backend == GraphicsBackend::D3D11) {

        }
        else if (backend == GraphicsBackend::D3D12) {
            const XrGraphicsBindingD3D12KHR* d3d12_bindings = static_cast<const XrGraphicsBindingD3D12KHR*> (graphics_binding);
            GB_DX12Compositor* d3d12_compositor = new GB_DX12Compositor();
            if (d3d12_compositor->Initialize(d3d12_bindings, backbuffer_count) == false) {
                LOG(ERROR) << "Failed to create compositor";
                return XR_ERROR_RUNTIME_FAILURE;
            }
        }
    }

    Renderer::~Renderer() {
        // Destroy resources created by BeginSession
        delete d3d12weaver;
        d3d12weaver = nullptr;

        intermediate_resource.DestroyResources();

        compositor->Deinitialize();

        //gb_session.window = {};
        delete gb_session.d3d12weaver;

        // Reset window swapchain
        gb_session.window_swapchain = {};

        // Destroy window
        gb_session.window.DestroyApplicationWindow();

        gb_session.command_queue.Reset();

        gb_session.d3d12_device.Reset();
    }

    void Renderer::CreateIntermediateTexture() {
        // Create intermediate resources for weaving render target
        intermediate_resource = GB_ProxySwapchain(0, session); // Handle 0 is not being used by xrCreateSwapchain
        intermediate_resource.CreateResources(gb_session.d3d12_device, system_resolution.x, system_resolution.y, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET, L"Intermediate resource");
    }

    void Renderer::CreateWeaver() {
        // Initialize weaver params
        DX12WeaverInitialize params{};
        params.command_queue = command_queue;
        params.device = d3d12_device;
        params.game_bridge = GetGameBridgeInstane();
        params.input_resource = intermediate_resource.GetBuffers()[0];
        params.render_target = window_swapchain.GetImages()[0];
        params.window = window.GetWindowHandle();

        d3d12weaver = new DirectX12Weaver(params);
        d3d12weaver->InitializeWeaver(gb_session.sr_context);
        sr_context->initialize();
    }
    void Renderer::CreateSystemWindow() {
        if (gb_session.window.TryGetExternalDisplay() != nullptr) {
            LOG(INFO) << "Got window";
        }

        // Create debug window
        auto system_resolution = GetSystemResolution(gb_system);

        gb_session.window.CreateApplicationWindow(XRGameBridge::g_runtime_settings.hInst, gb_system, system_resolution.x, system_resolution.y, true, true);
        // Debugging with non full screen mode
        //gb_session.display.CreateApplicationWindow(XRGameBridge::g_runtime_settings.hInst, 2560, 1440, true, false, true);
    }
    void Renderer::CreateWindowSwapchain() {
        // Create swapchain info for the window swapchain
        XrSwapchainCreateInfo window_swapchain_info;
        window_swapchain_info.width = system_resolution.x;
        window_swapchain_info.height = system_resolution.y;
        window_swapchain_info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        window_swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

        // Create swapchain for debug window
        gb_session.window_swapchain.CreateSwapChain(gb_session.d3d12_device, gb_session.command_queue, &window_swapchain_info, gb_session.window.GetWindowHandle());
    }
}