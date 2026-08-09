/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once

#include <vector>
#include <chrono>
#include <mutex>

#include "openxr_includes.h"
#include "window.h"

#include "graphics/xrrendering.h"
#include "events.h"
#include <dll.h>

XrResult xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session);
XrResult xrDestroySession(XrSession session);

XrResult xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo);
XrResult xrEndSession(XrSession session);
XrResult xrRequestExitSession(XrSession session);

// Frames
XrResult xrWaitFrame(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState);
XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo);
XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo);

// XRGB function
DllExport XrResult xrgbGetReleasedBufferHandle(XrSession session, uint64_t* resourceHandle);

namespace ch = std::chrono;

typedef uint64_t Frame;

enum FrameState {
    NewFrameAllowed,
    NewFrameBusy,
    Waiting,
    Rendering,
    Ended
};

class FrameTimer {
    using time_point = std::chrono::high_resolution_clock::time_point;
    time_point last_frame_time;

    FrameTimer(FrameTimer& other) = delete;
    FrameTimer(FrameTimer&& other) = delete;

public:
    XrDuration GetTimeDelta() {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto delta_time = current_time - last_frame_time;
        last_frame_time = current_time;
        return std::chrono::duration_cast<std::chrono::nanoseconds>(delta_time).count();
    }

    FrameTimer() {
        last_frame_time = std::chrono::high_resolution_clock::now();
    }
    FrameTimer(XrTime start_time) {
        last_frame_time = time_point{ std::chrono::duration_cast<time_point::duration>(std::chrono::nanoseconds(start_time)) };
    }
};

class XRSession {
public:
    XrSession id = nullptr;
    XrInstance instance = nullptr;
    XrViewConfigurationType view_configuration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    std::shared_ptr<EventStreamReader> hotkey_events_reader;
    std::shared_ptr<EventStreamWriter> instance_event_stream_writer;

    // System
    XrSystemId system = 0;
    const FaceTrackingModule* face_tracking = nullptr;

    // Session state
    std::mutex mutex_session_state_queue;
    std::vector<XrSessionState> session_state_queue;
    XrSessionState session_state = XR_SESSION_STATE_IDLE;

    // Timing
    int32_t frame_time_index = 0;
    std::array<uint32_t, 100> frame_times;
    FrameTimer frame_timer;

    // Frame logic
    FrameState wait_frame_state;
    std::mutex mutex_wait_frame_state;
    Frame waited_frame = 0;
    Frame started_frame = 0;
    Frame ended_frame = 0;
    bool end_frame_called = true;
    bool should_render = false;
	bool is_renderer_initialized = false;

    // Renderer
    Renderer* renderer = nullptr;

private:
    // Views
    float separation_scale = 0.6f;
    float popout_scale = 4.f;
    float separation_scale_max = 3.f;
    float fov_max = 0.5f * glm::pi<float>();
    float popout_scale_max = 30.f;
    float scale_min = 0.0001;


    bool lookaround_xy = false;
    bool lookaround_z = false; // Use fov when false
    float virtual_fov_rad = 1.f / 4.f * glm::pi<float>();
    float camera_lerp = 1.0f;
    bool should_weave = true;

public:
    const std::shared_ptr<XRSystem>& GetSystem();

    std::vector<XrView> GetViewPositions() const;

    void ChangeSessionState(XrSessionState state);

    void UpdateSession();

    void ResetFrameState();

    XRSession();
    ~XRSession() = default;
};
