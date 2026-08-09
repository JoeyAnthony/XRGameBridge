/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "session.h"

#include <stdexcept>
#include <shellscalingapi.h>

#include <glm/glm.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <numeric>

#include "debug.h"
#include "openxr_functions.h"
#include "instance.h"
#include "system.h"
#include "settings.h"
#include "graphics/d3d11renderer.h"
#include "graphics/d3d12renderer.h"
#include "hotkeys/hotkeymanager.h"
//#include "swapchain.h"

XrResult xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session) {
    TraceLogFunctionCall(__func__, __LINE__);

    // TODO refactor local scope static variables
    static uint64_t session_creation_count = 1;
    GB_Instance* gb_instance = reinterpret_cast<GB_Instance*>(instance);
    // TODO make this the bas type
    auto system = std::dynamic_pointer_cast<SRSystem>( g_systems[createInfo->systemId]);
    spdlog::info("Creating session: {}", session_creation_count);

    if (gb_instance->GetActiveGraphicsAPI() == GraphicsBackend::Uninitialized) {
        spdlog::error("Graphics requirements call missing");
        return XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING;
    }

    if (!system) {
        spdlog::error("Couldn't find system. System invalid");
        return XR_ERROR_SYSTEM_INVALID;
    }

    // Create entry if it doesn't exist
    // Note: This means it overwrite all except for the id member it if it does exist
    XrSession handle = reinterpret_cast<XrSession>(session_creation_count);
    XRSession& new_session = g_sessions[handle];

    // Initialize session with state idle
    new_session.id = handle;
    new_session.instance = instance;
    new_session.system = createInfo->systemId;

    // Create Renderer
    if (gb_instance->GetActiveGraphicsAPI() == GraphicsBackend::D3D12) {
        new_session.renderer = D3D12Renderer::Create(createInfo->systemId, createInfo->next);;
    }
    else if (gb_instance->GetActiveGraphicsAPI() == GraphicsBackend::D3D11) {
        new_session.renderer = D3D11Renderer::Create(createInfo->systemId, createInfo->next);
    }
    else {
        spdlog::error("Trying to create session with unsupported graphics api");
        LOG_RUNTIME_ERROR
        return XR_ERROR_RUNTIME_FAILURE;
    }

    // Get hot-key event stream reader
    new_session.hotkey_events_reader = gb_instance->GetEventManager().GetEventStreamReader(GB_EVENT_STREAM_TYPE_HOTKEY);
    g_hotkey_manager->AddHotkey(GB_EVENT_HOTKEY_TOGGLE_WEAVING, VK_LCONTROL, VK_F1);

    g_hotkey_manager->AddHotkey(GB_EVENT_HOTKEY_DECREASE_SEPARATION, VK_LCONTROL, VK_F5);
    g_hotkey_manager->AddHotkey(GB_EVENT_HOTKEY_INCREASE_SEPARATION, VK_LCONTROL, VK_F6);

    g_hotkey_manager->AddHotkey(GB_EVENT_HOTKEY_DECREASE_CONVERGEANCE, VK_LCONTROL, VK_F7);
    g_hotkey_manager->AddHotkey(GB_EVENT_HOTKEY_INCREASE_CONVERGEANCE, VK_LCONTROL, VK_F8);

    g_hotkey_manager->AddHotkey(GB_EVENT_HOTKEY_DECREASE_FOV, VK_LCONTROL, VK_F9);
    g_hotkey_manager->AddHotkey(GB_EVENT_HOTKEY_INCREASE_FOV, VK_LCONTROL, VK_F10);

    // Get instance even stream writer
    new_session.instance_event_stream_writer = gb_instance->GetInstanceEventStreamWriter();

    *session = handle;
    session_creation_count++;

    new_session.ChangeSessionState(XR_SESSION_STATE_READY);
    new_session.UpdateSession();

    spdlog::info("Successfully created session: {}", session_creation_count);
    return XR_SUCCESS;
}

XrResult xrDestroySession(XrSession session) {
    spdlog::info("Destroying Session");
    TraceLogFunctionCall(__func__, __LINE__);

    // TODO Should probably destroy all objects related to a session.
    // Also action sets/g_actions attached to the session should be destroyed
    XRSession& gb_session = g_sessions[session];

    delete gb_session.renderer;

    try {
        g_sessions.erase(session);
    }
    catch (std::exception& e) {
        spdlog::error("{}", e.what());
    }
    catch (...) {
        spdlog::error("Error occurred while destroying the session");
    }

    return XR_SUCCESS;
}

XrResult xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo) {
    spdlog::info("Beginning Session");
    TraceLogFunctionCall(__func__, __LINE__);

    // TODO check if view configuration type is supported
    // TODO, move SESSION_READY logic to here, check here whether all components are initialized for the session to be put on READY.

    XRSession& gb_session = g_sessions[session];
    const auto gb_system = g_systems[gb_session.system];

    if (gb_session.session_state == XR_SESSION_STATE_IDLE) {
        spdlog::error("Session not ready");
        return XR_ERROR_SESSION_NOT_READY;
    }
    if (gb_session.session_state != XR_SESSION_STATE_READY) {
        spdlog::error("Session is already running");
        return XR_ERROR_SESSION_RUNNING;
    }

    if (!std::ranges::any_of(
        gb_system->GetViewConfigurationProperties(),
        [&](const auto& prop) {
            return prop.viewConfigurationType == beginInfo->primaryViewConfigurationType;
        })) {
        return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
    }
    gb_session.view_configuration = beginInfo->primaryViewConfigurationType;

    gb_session.face_tracking = gb_system->GetFaceTracking();

    // Send all state changes
    gb_session.ChangeSessionState(XR_SESSION_STATE_SYNCHRONIZED);
    gb_session.ChangeSessionState(XR_SESSION_STATE_VISIBLE);
    gb_session.ChangeSessionState(XR_SESSION_STATE_FOCUSED);
    gb_session.UpdateSession();

    // TODO runtime cannot handle shoulde_render = false yet. If false, layerCount = 0 in xrwaitframe and no resources will be signaled. Waitimage will timeout
    gb_session.should_render = true;

    return XR_SUCCESS;
}

XrResult xrEndSession(XrSession session) {
    spdlog::info("Ending Session");
    TraceLogFunctionCall(__func__, __LINE__);

    XRSession& gb_session = g_sessions[session];

    if (gb_session.session_state & XR_SESSION_STATE_SYNCHRONIZED & XR_SESSION_STATE_VISIBLE & XR_SESSION_STATE_FOCUSED & XR_SESSION_STATE_STOPPING == false) {
        return XR_ERROR_SESSION_NOT_RUNNING;
    }

    std::unique_lock unique_guard(gb_session.mutex_wait_frame_state, std::try_to_lock);
    if (unique_guard.owns_lock() == false) {
        spdlog::warn("Trying to stop the session but the frame mutex is in use");
        return XR_ERROR_SESSION_NOT_STOPPING;
    }

    gb_session.ResetFrameState();

    // Save profiles maybe

    // Change session state to idle
    if (gb_session.session_state != XR_SESSION_STATE_EXITING) {
        gb_session.ChangeSessionState(XR_SESSION_STATE_IDLE);
        gb_session.UpdateSession();
    }

    return XR_SUCCESS;
}

XrResult xrRequestExitSession(XrSession session) {
    spdlog::info("Exiting Session");
    TraceLogFunctionCall(__func__, __LINE__);

    XRSession& gb_session = g_sessions[session];
    if (gb_session.session_state & XR_SESSION_STATE_SYNCHRONIZED & XR_SESSION_STATE_VISIBLE & XR_SESSION_STATE_FOCUSED == false) {
        return XR_ERROR_SESSION_NOT_RUNNING;
    }

    // Change session state to stopping
    gb_session.ChangeSessionState(XR_SESSION_STATE_SYNCHRONIZED);
    gb_session.ChangeSessionState(XR_SESSION_STATE_STOPPING);
    gb_session.ChangeSessionState(XR_SESSION_STATE_EXITING);
    gb_session.UpdateSession();

    return XR_SUCCESS;
}

// TODO Use frame display time as frame ids
XrResult xrWaitFrame(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState) {
    TraceLogFunctionCall(__func__, __LINE__);

    // TODO simple implementation so the application can continue. Should when I understand this part better
    XRSession& gb_session = g_sessions[session];
    bool should_wait = true;

    // Blocking wait, blocks until BeginFrame was called
    while (should_wait) {
        if (gb_session.mutex_wait_frame_state.try_lock()) {
            if (gb_session.wait_frame_state == NewFrameAllowed) {
                gb_session.wait_frame_state = NewFrameBusy;
                break;
            }
            gb_session.mutex_wait_frame_state.unlock();
        }
        std::this_thread::sleep_for(ch::nanoseconds(10));
    }

    gb_session.UpdateSession();

    // TODO calculate time based on the screen's v-sync
    XrDuration average_frame_time = std::accumulate(gb_session.frame_times.begin(), gb_session.frame_times.end(), 0LL) / gb_session.frame_times.size();
    XrTime current_time = (std::chrono::high_resolution_clock::now() - g_runtime_settings->GetRuntimeEpoch()).count();

    frameState->predictedDisplayPeriod = average_frame_time;
    frameState->predictedDisplayTime = current_time + average_frame_time * 1.5;
    frameState->shouldRender = gb_session.should_render;

    gb_session.waited_frame = frameState->predictedDisplayTime;

    gb_session.mutex_wait_frame_state.unlock();

    return XR_SUCCESS;
}

XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo) {
    TraceLogFunctionCall(__func__, __LINE__);

    XRSession& gb_session = g_sessions[session];

    std::lock_guard guard(gb_session.mutex_wait_frame_state);

    if (gb_session.waited_frame == 0) {
        // Call order invalid
        return XR_ERROR_CALL_ORDER_INVALID;
    }
    if (gb_session.end_frame_called == false) {
        // Skip frame
        // TODO If no layers are provided then the display must be cleared.
        gb_session.started_frame = 0;
        gb_session.wait_frame_state = FrameState::NewFrameAllowed;
        return XR_FRAME_DISCARDED;
    }

    if (gb_session.wait_frame_state != NewFrameBusy) {
        return XR_ERROR_CALL_ORDER_INVALID;
    }

    gb_session.wait_frame_state = FrameState::NewFrameAllowed;
    gb_session.started_frame = gb_session.waited_frame;
    gb_session.waited_frame = 0;

    gb_session.end_frame_called = false;

    // Log time left
    //uint64_t time_now = ch::nanoseconds(ch::high_resolution_clock::now() - gb_session.session_epoch).count();
    //uint64_t time_left = gb_session.started_frame - time_now;
    //spdlog::info("Frame started. Time left: " << time_left;

    return XR_SUCCESS;
}

XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) {
    TraceLogFunctionCall(__func__, __LINE__);

    if (frameEndInfo->layerCount == 0) {
        return XR_ERROR_LAYER_INVALID;
    }

    // TODO If no layers are provided then the display must be cleared.
    // Present the frame for session
    XRSession& gb_session = g_sessions[session];

    // Initialize resources according to swapchain settings from the application so we don't have to guess the texture format
    if (!gb_session.is_renderer_initialized) {
		if (frameEndInfo->layers[0]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
			auto layer = reinterpret_cast<const XrCompositionLayerProjection*>(frameEndInfo->layers[0]);
			if (layer->viewCount > 0) {
		    // Initialize rendering pipeline
			gb_session.renderer->InitializePipeline(layer->views[0].subImage.swapchain);
		    gb_session.is_renderer_initialized = true;
			}
		}
    }

    // TODO Frame too late, signal fences and return success. Skipping frame?
    //if (time_now > gb_session.started_frame) {
    //    // Application too late
    //    spdlog::info("Application too late, skipping compose";
    //    gb_compositor.SignalSwapchainsForFrame(frameEndInfo);
    //    return XR_SUCCESS;
    //}
    if(gb_session.started_frame == 0)
    {
        // Call order invalid
        spdlog::info("No frame started");
        gb_session.end_frame_called = true;
        return XR_SUCCESS;
    }
    if(gb_session.started_frame == gb_session.ended_frame)
    {
        // Same frame to be re-presented, can choose to only weave here.
    }

    gb_session.renderer->RenderFrame(frameEndInfo);
    // Store frame time
    // RenderFrame presents the image, so we can assume the image is on the new screen now.
    gb_session.frame_time_index = (gb_session.frame_time_index + 1) % gb_session.frame_times.size();
    gb_session.frame_times[gb_session.frame_time_index] = gb_session.frame_timer.GetTimeDelta();

    // Update window
    gb_session.renderer->Update();

    gb_session.ended_frame = gb_session.started_frame;
    gb_session.started_frame = 0;
    gb_session.end_frame_called = true;

    return XR_SUCCESS;
}

XrResult xrgbGetReleasedBufferHandle(XrSession session, uint64_t* resourceHandle) {
	XRSession& gb_session = g_sessions[session];
	*resourceHandle = gb_session.renderer->GetWeavedBufferHandle();
	if (*resourceHandle == 0) {
		return XR_ERROR_RUNTIME_FAILURE;
    }

	return XR_SUCCESS;
}

const std::shared_ptr<XRSystem>& XRSession::GetSystem() {
    return g_systems[system];
}

std::vector<XrView> XRSession::GetViewPositions() const {
    auto& sys =  *static_cast<SRSystem*>(g_systems[system].get());
    auto [left, right] = face_tracking->GetEyePositions(0);

    // Derive ipd_scaling
    float phys_eye_fov = glm::atan(sys.PhysicalSizeWidth() / 2 / left.z);
    float ipd_scale = phys_eye_fov / virtual_fov_rad;

    // Since we define a different fov for games, (let's say 90deg), which is usually larger than the physical fov (let's say 40deg), we need to compensate for that by making the ipd smaller.
    // For this we derive the ipd scale and multiply it with the ipd.
    const float ipd = glm::abs(left.x - right.x) * ipd_scale;
    auto left_eye = XrVector3f{ -(ipd / 2), 0, 0 };
    auto right_eye = XrVector3f{ (ipd / 2), 0, 0 };
    if(lookaround_xy) {
        left_eye = left;
        right_eye = right;
    }

    // Calculate the eye distance distance from the virtual fov that we want to use in games (for example 90deg). This distance can be used to calculate the fov angles.
    float left_distance = ((sys.PhysicalSizeWidth() - ipd) / 2) / glm::tan(virtual_fov_rad);
    float right_distance = left_distance;
    if(lookaround_z) {
        left_distance = left.z;
        right_distance = left.z;
    }

    auto vec = std::vector{
        XrView {
            .pose = XrPosef{{0}, {left_eye.x * popout_scale, 0, 0}},
            // Calculate angles with game fov and scaled ipd
            .fov = sys.GetConvergingFov({left_eye.x * separation_scale, left_eye.y, left_distance})
        },
        XrView {
            .pose = XrPosef{{0}, {right_eye.x * popout_scale, 0, 0}},
            .fov = sys.GetConvergingFov({right_eye.x * separation_scale, right_eye.y, right_distance})
        }
    };
    vec.shrink_to_fit();
    return vec;
}

void XRSession::ChangeSessionState(XrSessionState state) {
    if (session_state == state) {
        return;
    }

    std::lock_guard guard_session_state_queue(mutex_session_state_queue);
    session_state_queue.push_back(state);

    char buffer[XR_MAX_RESULT_STRING_SIZE];
    if (GetSessionStateString(state, buffer) == XR_SUCCESS) {
        spdlog::info("Session state queued: {}", std::string(buffer));
    }
}

void XRSession::UpdateSession() {
    // Only allowed to send messages between event submission and processing
    GB_Instance* gb_instance = reinterpret_cast<GB_Instance*>(instance);
    EventManager& event_manager = gb_instance->GetEventManager();
    event_manager.PrepareForEventStreamSubmission();

    {
        std::lock_guard guard_session_state_queue(mutex_session_state_queue);

        for (auto& state : session_state_queue) {
            // Update session state
            XrEventDataSessionStateChanged state_change;
            state_change.type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
            state_change.session = id;
            state_change.state = state;
            state_change.time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - g_runtime_settings->GetRuntimeEpoch()).count();
            instance_event_stream_writer->SubmitEvent(state, sizeof(XrEventDataSessionStateChanged), &state_change);

            // Set new session state
            session_state = state;

            char buffer[XR_MAX_RESULT_STRING_SIZE];
            if (GetSessionStateString(state, buffer) == XR_SUCCESS) {
                spdlog::info("Session state submitted: {}", std::string(buffer));
            }
        }

        // Clear session state queue
        session_state_queue.clear();
    }

    // Register hot-key events
    g_hotkey_manager->PollHotkeys();
    g_hotkey_manager->SendHotkeyEvents();

    // Not allowed to send messages after this function
    event_manager.PrepareForEventStreamProcessing();// TODO FOR DEBUG PURPOSES SHOULD BE REMOVED ASAP

    // Process input events
    GB_EVENT event_type;
    while (hotkey_events_reader->GetNextEvent(event_type)) {
        // Toggle buttons
        if (event_type == GB_EVENT_HOTKEY_TOGGLE_WEAVING) {
            should_weave = should_weave ? false : true;
			renderer->EnableWeaving(should_weave);
        }

        // Separation buttons
        constexpr float incremental_value_separation = 0.01f;
        constexpr float incremental_value_popout = 0.1f;
        constexpr  float incremental_value_fov = 0.05f;
        if (event_type == GB_EVENT_HOTKEY_INCREASE_SEPARATION) {
            separation_scale = glm::clamp(separation_scale + incremental_value_separation, scale_min, separation_scale_max);
        }

        if (event_type == GB_EVENT_HOTKEY_DECREASE_SEPARATION) {
            separation_scale = glm::clamp(separation_scale + incremental_value_separation * -1, scale_min, separation_scale_max);
        }

        if (event_type == GB_EVENT_HOTKEY_INCREASE_CONVERGEANCE) {
            popout_scale = glm::clamp(popout_scale + incremental_value_popout, scale_min, popout_scale_max);
        }

        if (event_type == GB_EVENT_HOTKEY_DECREASE_CONVERGEANCE) {
            popout_scale = glm::clamp(popout_scale + incremental_value_popout * -1, scale_min, popout_scale_max);
        }

        if (event_type == GB_EVENT_HOTKEY_INCREASE_FOV) {
            virtual_fov_rad = glm::clamp(virtual_fov_rad + glm::radians(incremental_value_fov), scale_min, fov_max);
        }

        if (event_type == GB_EVENT_HOTKEY_DECREASE_FOV) {
            virtual_fov_rad = glm::clamp(virtual_fov_rad - glm::radians(incremental_value_fov), scale_min, fov_max);
        }
    }
}

void XRSession::ResetFrameState() {
    // Reset state
    wait_frame_state = NewFrameAllowed;
    waited_frame = 0;
    started_frame = 0;
    end_frame_called = 0;
    end_frame_called = false;
    should_render = true;

    frame_times.fill(0);
}

XRSession::XRSession() {
    ResetFrameState();
}
