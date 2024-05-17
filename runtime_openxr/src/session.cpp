#include "session.h"

#include <stdexcept>
#include <shellscalingapi.h>

#include "easylogging++.h"
#include "openxr_functions.h"
#include "instance.h"
#include "system.h"
#include "settings.h"
#include "compositor.h"
#include "swapchain.h"
#include  "instance.h"


XrResult xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session) {
    // TODO refactor local scope static variables
    static uint64_t session_creation_count = 1;

    LOG(INFO) << "Creating session: " << session_creation_count;

    try {
        XRGameBridge::GB_System& system = XRGameBridge::g_systems.at(createInfo->systemId);
        if (!system.features_enumerated) {
            LOG(ERROR) << "Graphics requirements call missing";
            return XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING;
        }

        if (system.instance != instance) {
            LOG(ERROR) << "Couldn't find system. System invalid";
            return XR_ERROR_SYSTEM_INVALID;
        }
    }
    catch (std::out_of_range& e) {
        LOG(ERROR) << "Couldn't find system. System invalid";
        return XR_ERROR_SYSTEM_INVALID;
    }
    catch (std::exception& e) {
        LOG(ERROR) << "Runtime failure when getting system";
        return XR_ERROR_RUNTIME_FAILURE;
    }

    // Create entry if it doesn't exist
    // Note: This means it overwrite all except for the id member it if it does exist
    XrSession handle = reinterpret_cast<XrSession>(session_creation_count);
    XRGameBridge::GB_Session& new_session = XRGameBridge::g_sessions[handle];

    // Initialize session with state idle
    new_session.id = handle;
    new_session.instance = instance;
    new_session.system = createInfo->systemId;
    new_session.session_state = XR_SESSION_STATE_IDLE;
    new_session.session_epoch = std::chrono::high_resolution_clock::now();

    // Set default values for the eye pairs
    float fovx = M_PI / 4.0f;
    float fovy = M_PI / 6.0f;

    new_session.stereo_views[0].type = XR_TYPE_VIEW;
    new_session.stereo_views[0].next = nullptr;
    new_session.stereo_views[0].pose = { {0.0f, 0.0f, 0.0f, 1.0f}, {-0.070f, 0, 0} }; // Orientation, Position
    new_session.stereo_views[0].fov = { -fovx, fovx, fovy, -fovy }; // FOV angle left, right, up, down

    new_session.stereo_views[1].type = XR_TYPE_VIEW;
    new_session.stereo_views[1].next = nullptr;
    new_session.stereo_views[1].pose = { {0.0f, 0.0f, 0.0f, 1.0f}, {0.070f, 0, 0} }; // Orientation, Position
    new_session.stereo_views[1].fov = { -fovx, fovx, fovy, -fovy }; // FOV angle left, right, up, down

    // DirectX 12
    if (XRGameBridge::g_runtime_settings.support_d3d12) {
        const XrGraphicsBindingD3D12KHR* d3d12_bindings = static_cast<const XrGraphicsBindingD3D12KHR*> (createInfo->next);
        new_session.d3d12_device = d3d12_bindings->device;
        new_session.command_queue = d3d12_bindings->queue;
        LOG(INFO) << "Create session with DirectX 12";
    }
    else {
        LOG(ERROR) << "Trying to create session with unsupported graphics api";
    }

    // Get hot-key event stream reader
    new_session.hotkey_events_reader = XRGameBridge::g_gamebridge_instance->GetEventManager().GetEventStreamReader(GB_EVENT_STREAM_TYPE_HOTKEY);
    XRGameBridge::g_hotkey_manager->AddHotkey(GB_EVENT_HOTKEY_TOGGLE_WEAVING, VK_LCONTROL, VK_F1);

    XRGameBridge::g_hotkey_manager->AddHotkey(GB_EVENT_HOTKEY_DECREASE_SEPARATION, VK_LCONTROL, VK_F5);
    XRGameBridge::g_hotkey_manager->AddHotkey(GB_EVENT_HOTKEY_INCREASE_SEPARATION, VK_LCONTROL, VK_F6);

    XRGameBridge::g_hotkey_manager->AddHotkey(GB_EVENT_HOTKEY_DECREASE_CONVERGENCE, VK_LCONTROL, VK_F7);
    XRGameBridge::g_hotkey_manager->AddHotkey(GB_EVENT_HOTKEY_INCREASE_CONVERGENCE, VK_LCONTROL, VK_F8);

    *session = handle;
    session_creation_count++;

    // TODO Not sure where to put the compositor, it has to be initialized by the session, but you render to a system
    // Maybe a system should own a compositor, but it is created and destroyed by the client?
    if (new_session.compositor.Initialize(new_session.d3d12_device, new_session.command_queue, 2) == false) {
        LOG(ERROR) << "Failed to create compositor";
        return XR_ERROR_RUNTIME_FAILURE;
    }

    // Create sr context, blocks till there is a connection
    XRGameBridge::GB_Instance* gb_instance = reinterpret_cast<XRGameBridge::GB_Instance*>(XRGameBridge::g_xr_instance);
    new_session.sr_context = gb_instance->sr_context;

    // Start session idle thread
    new_session.StartSessionIdle();

    LOG(INFO) << "Successfully created session: " << session_creation_count;
    return XR_SUCCESS;
}

XrResult xrDestroySession(XrSession session) {
    // TODO Should probably destroy all objects related to a session.
    // Swap chains depend on the session since it's holds the device and command queue, so swap chains should be destroyed on session destroy.
    // Also action sets/g_actions attached to the session should be destroyed
    XRGameBridge::GB_Session& gb_session = XRGameBridge::g_sessions[session];

    if (gb_session.d3d12weaver) {
        delete gb_session.d3d12weaver;
    }

    gb_session.compositor = {};
    gb_session.window_swapchain = {};
    gb_session.intermediate_resource = {};
    gb_session.display = {};
    gb_session.sr_context = nullptr;
    gb_session.command_queue.Reset();
    gb_session.d3d12_device.Reset();

    try {
        XRGameBridge::g_sessions.erase(session);
    }
    catch (std::exception& e) {
        LOG(ERROR) << "" << e.what();
    }
    catch (...) {
        LOG(ERROR) << "Error occurred while destroying the session";
    }

    return XR_SUCCESS;
}

XrResult xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo) {
    // TODO check if view configuration type is supported
    // TODO, move SESSION_READY logic to here, check here whether all components are initialized for the session to be put on READY.

    XRGameBridge::GB_Session& gb_session = XRGameBridge::g_sessions[session];
    XRGameBridge::GB_System& gb_system = XRGameBridge::g_systems[gb_session.system];

    gb_session.idle_thread.join();

    if (gb_session.session_state == XR_SESSION_STATE_IDLE) {
        LOG(ERROR) << "Session not ready";
        return XR_ERROR_SESSION_NOT_READY;
    }
    if (gb_session.session_state != XR_SESSION_STATE_READY) {
        LOG(ERROR) << "Session is already running";
        return XR_ERROR_SESSION_RUNNING;
    }

    gb_session.view_configuration = beginInfo->primaryViewConfigurationType;

    // TODO Move creation of objects to CreateSession, except for the creation of the window swapchain and the window perhaps.

    if(gb_session.display.TryGetExternalDisplay() != nullptr)
    {
        LOG(INFO) << "Got window";
    }

    // Create debug window
    auto system_resolution = XRGameBridge::GetSystemResolution(gb_system);
    gb_session.display.CreateApplicationWindow(XRGameBridge::g_runtime_settings.hInst, system_resolution.x, system_resolution.y, true, true);
    //gb_session.display.CreateApplicationWindow(XRGameBridge::g_runtime_settings.hInst, system_resolution.x, system_resolution.y, true, false);
    // Debugging with non full screen mode


    // Create swapchain info
    XrSwapchainCreateInfo swapchain_info;
    swapchain_info.width = system_resolution.x;
    swapchain_info.height = system_resolution.y;
    swapchain_info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

    // Create intermediate resources for weaving render target
    gb_session.intermediate_resource.CreateResources(gb_session.d3d12_device, system_resolution.x, system_resolution.y, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET, L"Intermediate resource");

    // Create swapchain for debug window
    gb_session.window_swapchain.CreateSwapChain(gb_session.d3d12_device, gb_session.command_queue, &swapchain_info, gb_session.display.GetWindowHandle());

    // Initialize weaver params
    DX12WeaverInitialize params{};
    params.command_queue = gb_session.command_queue.Get();
    params.device = gb_session.d3d12_device.Get();
    params.game_bridge = XRGameBridge::g_gamebridge_instance;
    params.input_resource = gb_session.intermediate_resource.GetBuffers()[0].Get();
    params.render_target = gb_session.window_swapchain.GetImages()[0].Get();
    params.window = gb_session.display.GetWindowHandle();

    // Create weaver
    gb_session.d3d12weaver = new DirectX12Weaver(params);
    gb_session.d3d12weaver->InitializeWeaver(gb_session.sr_context);
    gb_session.sr_context->initialize();

    // Send all state changes
    XRGameBridge::ChangeSessionState(gb_session, XR_SESSION_STATE_SYNCHRONIZED);
    XRGameBridge::ChangeSessionState(gb_session, XR_SESSION_STATE_VISIBLE);
    XRGameBridge::ChangeSessionState(gb_session, XR_SESSION_STATE_FOCUSED);

    // TODO runtime cannot handle shoulde_render = false yet. If false, layerCount = 0 in xrwaitframe and no resources will be signaled. Waitimage will timeout
    gb_session.should_render = true;

    return XR_SUCCESS;
}

XrResult xrEndSession(XrSession session) {
    XRGameBridge::GB_Session& gb_session = XRGameBridge::g_sessions[session];

    if (gb_session.session_state & XR_SESSION_STATE_SYNCHRONIZED & XR_SESSION_STATE_VISIBLE & XR_SESSION_STATE_FOCUSED & XR_SESSION_STATE_STOPPING == false) {
        return XR_ERROR_SESSION_NOT_RUNNING;
    }

    std::unique_lock unique_guard(gb_session.mutex_wait_frame_state, std::try_to_lock);
    if (unique_guard.owns_lock() == false) {
        LOG(WARNING) << "Trying to stop the session but the frame mutex is in use";
        return XR_ERROR_SESSION_NOT_STOPPING;
    }

    // Destroy resources created by BeginSession
    delete gb_session.d3d12weaver;
    gb_session.d3d12weaver = nullptr;

    // Not necessary as it uses ComPtr for resources
    gb_session.intermediate_resource.DestroyResources();

    // Reset window swapchain
    gb_session.window_swapchain = {};

    // Destroy window
    gb_session.display.DestroyApplicationWindow();

    // Reset state
    gb_session.wait_frame_state = XRGameBridge::NewFrameAllowed;
    gb_session.waited_frame = 0;
    gb_session.started_frame = 0;
    gb_session.end_frame_called = 0;
    gb_session.end_frame_called = false;
    gb_session.should_render = true;

    // Save profiles maybe

    // Change session state to idle
    if (gb_session.session_state != XR_SESSION_STATE_EXITING) {
        XRGameBridge::ChangeSessionState(gb_session, XR_SESSION_STATE_IDLE);
        XRGameBridge::UpdateSession(gb_session);

        // Start Session Idle thread
        gb_session.StartSessionIdle();
    }

    return XR_SUCCESS;
}

XrResult xrRequestExitSession(XrSession session) {
    XRGameBridge::GB_Session& gb_session = XRGameBridge::g_sessions[session];
    if (gb_session.session_state & XR_SESSION_STATE_SYNCHRONIZED & XR_SESSION_STATE_VISIBLE & XR_SESSION_STATE_FOCUSED == false) {
        return XR_ERROR_SESSION_NOT_RUNNING;
    }

    // Change session state to stopping
    XRGameBridge::ChangeSessionState(gb_session, XR_SESSION_STATE_SYNCHRONIZED);
    ChangeSessionState(gb_session, XR_SESSION_STATE_STOPPING);
    ChangeSessionState(gb_session, XR_SESSION_STATE_EXITING);

    return XR_SUCCESS;
}

// TODO Use frame display time as frame ids
XrResult xrWaitFrame(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState) {
    // TODO simple implementation so the application can continue. Should when I understand this part better
    XRGameBridge::GB_Session& gb_session = XRGameBridge::g_sessions[session];
    bool should_wait = true;

    // Blocking wait, blocks until BeginFrame was called
    while (should_wait) {
        if (gb_session.mutex_wait_frame_state.try_lock()) {
            if (gb_session.wait_frame_state == XRGameBridge::NewFrameAllowed) {
                gb_session.wait_frame_state = XRGameBridge::NewFrameBusy;
                break;
            }
            gb_session.mutex_wait_frame_state.unlock();
        }
        std::this_thread::sleep_for(ch::nanoseconds(10));
    }

    // Time point since session epoch + 16 milliseconds
    // Super simple version of this for now I guess

    /* As far as I understand:
     * predictedDisplayTime: The future time point the next image will be displayed at
     * predictedDisplayPeriod: The amount of time the next image will be visible (presented) on the screen
     */

     // 1/60th in nanoseconds
    uint64_t nanoseconds = 1.0f / 60.0f * 1000.f * 1000.f * 1000.f;
    auto refresh_rate = ch::nanoseconds(nanoseconds);
    // Image should be displayed for the <refresh rate> amount of time
    auto display_period = ch::nanoseconds(refresh_rate);
    // Time since the epoch the application is running now, add the refresh rate to predict the time the next image will be displayed.
    auto display_time = ch::nanoseconds(ch::high_resolution_clock::now() - gb_session.session_epoch + refresh_rate);

    XRGameBridge::UpdateSession(gb_session);

    frameState->predictedDisplayPeriod = display_period.count();
    frameState->predictedDisplayTime = display_time.count();
    frameState->shouldRender = gb_session.should_render;

    gb_session.waited_frame = frameState->predictedDisplayTime;

    gb_session.mutex_wait_frame_state.unlock();

    //LOG(INFO) << "PredictedDisplayTime: " << frameState->predictedDisplayTime;

    return XR_SUCCESS;
}

XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo) {
    XRGameBridge::GB_Session& gb_session = XRGameBridge::g_sessions[session];

    std::lock_guard guard(gb_session.mutex_wait_frame_state);

    if (gb_session.waited_frame == 0) {
        // Call order invalid
        return XR_ERROR_CALL_ORDER_INVALID;
    }
    if (gb_session.end_frame_called == false) {
        // Skip frame
        // TODO If no layers are provided then the display must be cleared.
        gb_session.started_frame = 0;
        gb_session.wait_frame_state = XRGameBridge::FrameState::NewFrameAllowed;
        return XR_FRAME_DISCARDED;
    }

    if (gb_session.ended_frame > gb_session.started_frame) {
        // Should be impossible
        LOG(WARNING) << "Previous frame is later than current";
    }

    if (gb_session.wait_frame_state != XRGameBridge::NewFrameBusy) {
        return XR_ERROR_CALL_ORDER_INVALID;
    }

    gb_session.wait_frame_state = XRGameBridge::FrameState::NewFrameAllowed;
    gb_session.started_frame = gb_session.waited_frame;

    gb_session.end_frame_called = false;

    // Log time left
    //uint64_t time_now = ch::nanoseconds(ch::high_resolution_clock::now() - gb_session.session_epoch).count();
    //uint64_t time_left = gb_session.started_frame - time_now;
    //LOG(INFO) << "Frame started. Time left: " << time_left;

    return XR_SUCCESS;
}

XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) {
    // TODO If no layers are provided then the display must be cleared.
    // Present the frame for session
    XRGameBridge::GB_Session& gb_session = XRGameBridge::g_sessions[session];
    auto& gb_compositor = gb_session.compositor;

    uint64_t time_now = ch::nanoseconds(ch::high_resolution_clock::now() - gb_session.session_epoch).count();
    long long time_left = gb_session.started_frame - time_now;
    //LOG(INFO) << "EndFrame, Time left: " << time_left;

    if (frameEndInfo->layerCount == 0) {
        return XR_ERROR_LAYER_INVALID;
    }

    // Frame too late, signal fences and return success
    //if (time_now > gb_session.started_frame) {
    //    // Application too late
    //    LOG(INFO) << "Application too late, skipping compose";
    //    gb_compositor.SignalSwapchainsForFrame(frameEndInfo);
    //    return XR_SUCCESS;
    //}
    //if(gb_session.started_frame == 0)
    //{
    //    // Call order invalid
    //    LOG(INFO) << "No frame started";
    //    return XR_SUCCESS;
    //}
    //if(gb_session.started_frame == gb_session.ended_frame)
    //{
    //    // Same frame to be re-presented, can choose to only weave here.
    //}

    auto display_time = ch::high_resolution_clock::now() - gb_session.session_epoch;
    //LOG(INFO) << "xrEndFrame Called: " << display_time.count();

    // TODO Don't want to keep swapchains in the swapchain anymore, either move them to the compositor, or the system.
    auto& window_swapchain = gb_session.window_swapchain;
    int32_t index = window_swapchain.AcquireNextImage();
    auto& cmd_list = gb_compositor.GetCommandList(index);
    auto& cmd_allocator = gb_compositor.GetCommandAllocator(index);
    float clear_color[4] = { 0.5f, 0.0f, 0.5f, 1.0f };

    // Prepare command list
    cmd_allocator->Reset();
    cmd_list->Reset(cmd_allocator.Get(), gb_compositor.GetPipelineState().Get());

    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor_handle_to_compose;

    if(gb_session.should_weave)
    {
        // Set intermediate resource as render target
        descriptor_handle_to_compose = CD3DX12_CPU_DESCRIPTOR_HANDLE(gb_session.intermediate_resource.GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), 0, gb_session.intermediate_resource.GetRtvDescriptorSize());
    }
    else
    {
        // Transition to render target
        gb_compositor.TransitionImage(cmd_list.Get(), window_swapchain.GetImages()[index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        // Set window swapchain as render target
        descriptor_handle_to_compose = CD3DX12_CPU_DESCRIPTOR_HANDLE(window_swapchain.GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), index, window_swapchain.GetRtvDescriptorSize());
    }

    cmd_list->OMSetRenderTargets(1, &descriptor_handle_to_compose, true, nullptr);
    cmd_list->ClearRenderTargetView(descriptor_handle_to_compose, clear_color, 0, nullptr);
    cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Compose and draw to the intermediate resource
    gb_compositor.ComposeImage(gb_session, frameEndInfo, cmd_list.Get(), gb_session.intermediate_resource.GetWidth(), gb_session.intermediate_resource.GetHeight());

    if (gb_session.should_weave) {
        // Transition intermediate resource to unordered access for the weaver
        gb_compositor.TransitionImage(cmd_list.Get(), gb_session.intermediate_resource.GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        // Transition window swapchain to render target
        gb_compositor.TransitionImage(cmd_list.Get(), window_swapchain.GetImages()[index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);


        // Set window swapchain as render target
        CD3DX12_CPU_DESCRIPTOR_HANDLE back_buffer_rtv_handle(window_swapchain.GetRtvHeap()->GetCPUDescriptorHandleForHeapStart(), index, window_swapchain.GetRtvDescriptorSize());
        cmd_list->OMSetRenderTargets(1, &back_buffer_rtv_handle, true, nullptr);
        cmd_list->ClearRenderTargetView(back_buffer_rtv_handle, clear_color, 0, nullptr);


        // Set viewport for weaving to window swapchain
        auto native_resolution = XRGameBridge::GetSystemResolution(XRGameBridge::g_systems[gb_session.system]);
        D3D12_VIEWPORT view_port{ 0, 0, static_cast<float>(native_resolution.x) , static_cast<float>(native_resolution.y), 0.0f, 1.0f };
        D3D12_RECT scissor_rect{ 0, 0, static_cast<long>(native_resolution.x) , static_cast<long>(native_resolution.y) };
        cmd_list->RSSetViewports(1, &view_port);
        cmd_list->RSSetScissorRects(1, &scissor_rect);


        // Do weaving
        gb_session.d3d12weaver->Weave(cmd_list.Get(), native_resolution.x, native_resolution.y, 0, 0);

        // Transition to render target
        gb_compositor.TransitionImage(cmd_list.Get(), gb_session.intermediate_resource.GetBuffers()[0].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
    }

    // Transition swapchain to present
    gb_compositor.TransitionImage(cmd_list.Get(), window_swapchain.GetImages()[index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    // Todo: maybe use split barriers at the end here instead of regular ones. Then also initialize the resources in the correct state.

    // Close command list
    cmd_list->Close();

    // Execute command lists
    gb_compositor.ExecuteCommandList(cmd_list.Get());
    gb_compositor.SignalSwapchainsForFrame(frameEndInfo);

    // Present to window
    window_swapchain.PresentFrame();

    // Update window
    gb_session.display.UpdateWindow();

    gb_session.ended_frame = gb_session.started_frame;

    gb_session.end_frame_called = true;

    return XR_SUCCESS;
}

void XRGameBridge::GB_Session::StartSessionIdle() {
    idle_thread = std::thread(&GB_Session::IdleFunc, this);
}

void XRGameBridge::GB_Session::IdleFunc() {
    while (session_state == XR_SESSION_STATE_IDLE) {
        if (g_proxy_swapchains.size() > 0) {
            ChangeSessionState(*this, XR_SESSION_STATE_READY);
        }

        UpdateSession(*this);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void XRGameBridge::ChangeSessionState(GB_Session& session, XrSessionState state) {
    if (session.session_state == state) {
        return;
    }

    std::lock_guard guard_session_state_queue(session.mutex_session_state_queue);
    session.session_state_queue.push_back(state);
}

void XRGameBridge::RenderFrameWeaving()
{
}

void XRGameBridge::RenderFrameSideBySide()
{
}

void XRGameBridge::UpdateSession(GB_Session& session) {
    // Only allowed to send messages between event submission and processing
    EventManager& event_manager = g_gamebridge_instance->GetEventManager();
    event_manager.PrepareForEventStreamSubmission();

    {
        std::lock_guard guard_session_state_queue(session.mutex_session_state_queue);

        for (auto& state : session.session_state_queue) {


            // Update session state
            XrEventDataSessionStateChanged state_change;
            state_change.type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
            state_change.session = session.id;
            state_change.state = state;
            state_change.time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - session.session_epoch).count();
            g_openxr_event_stream_writer->SubmitEvent(state, sizeof(XrEventDataSessionStateChanged), &state_change);

            // Set new session state
            session.session_state = state;
        }

        // Clear session state queue
        session.session_state_queue.clear();
    }

    // Register hot-key events
    g_hotkey_manager->PollHotkeys();
    g_hotkey_manager->SendHotkeyEvents();

    // Not allowed to send messages after this function
    event_manager.PrepareForEventStreamProcessing();// TODO FOR DEBUG PURPOSES SHOULD BE REMOVED ASAP

    LPMSG msg = nullptr;
    if (session.display.PeekMessageExternal(msg)) {
        switch (msg->message) {
        case WM_KEYDOWN:
            if (GetAsyncKeyState(VK_F1) & 0x80) {
                LOG(INFO) << "Pressed";
            }
            break;
        case WM_KEYUP:
            if (GetAsyncKeyState(VK_F1) & 0x00) {
                LOG(INFO) << "Released";
            }
            break;
        }
    }

    // Check if the F1 key is up
    static bool f1_pressed = false;
    if ((GetAsyncKeyState(VK_F1) & 0x80) == 0) {
        f1_pressed = false;
    }

    // Process input events
    GB_EVENT event_type;
    while (session.hotkey_events_reader->GetNextEvent(event_type)) {
        // Toggle buttons
        if(event_type == GB_EVENT_HOTKEY_TOGGLE_WEAVING && f1_pressed == false)
        {
            session.should_weave = session.should_weave ? false : true;
            f1_pressed = true;
        }

        // Separation buttons
        bool value_changed = false;
        float incremental_value_pose = 0.002f;
        float incremental_value_orientation = M_PI / 50.0f;
        int factor_pose = 1.0f;
        int factor_orientation = 1.0f;
        XrView view_l = session.stereo_views[0];
        XrView view_r = session.stereo_views[1];

        if (event_type == GB_EVENT_HOTKEY_INCREASE_SEPARATION) {
            float addition = incremental_value_pose * factor_pose;

            view_l.pose.position.x += addition * -1.0f;
            view_r.pose.position.x += addition;

            value_changed = true;
        }

        if (event_type == GB_EVENT_HOTKEY_DECREASE_SEPARATION) {
            factor_pose = -1.0f;
            float addition = incremental_value_pose * factor_pose;

            view_l.pose.position.x += addition * -1.0f;
            view_r.pose.position.x += addition;

            value_changed = true;
        }

        if (event_type == GB_EVENT_HOTKEY_INCREASE_CONVERGENCE) {
            float addition = incremental_value_orientation * factor_orientation;
            view_l.pose.orientation.y += addition * -1.0f;
            view_r.pose.orientation.y += addition;

            value_changed = true;
        }

        if (event_type == GB_EVENT_HOTKEY_DECREASE_CONVERGENCE) {
            factor_orientation = 1.0f;

            float addition = incremental_value_orientation * factor_orientation;
            view_l.pose.orientation.y += addition * -1.0f;
            view_r.pose.orientation.y += addition;

            value_changed = true;
        }

        if (value_changed) {
            SetXrViewPose(session, 0, view_l.pose);
            SetXrViewPose(session, 1, view_r.pose);
        }
    }
}

void XRGameBridge::SetXrViewPose(GB_Session& session, uint32_t index, const XrPosef& pose)
{
    if (index > session.stereo_views.size() - 1) {
        LOG(WARNING) << "Session view array index out of bounds";
        return;
    }

    session.stereo_views[index].pose = pose;
}

void XRGameBridge::SetXrViewFov(GB_Session& session, uint32_t index, const XrFovf& fov)
{
    if (index > session.stereo_views.size() - 1) {
        LOG(WARNING) << "Session view array index out of bounds";
        return;
    }

    session.stereo_views[index].fov = fov;
}
