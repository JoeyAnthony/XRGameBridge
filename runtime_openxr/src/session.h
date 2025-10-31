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

#include "sr/management/srcontext.h"
#include "graphics/xrrendering.h"
#include "events.h"

XrResult xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session);
XrResult xrDestroySession(XrSession session);

XrResult xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo);
XrResult xrEndSession(XrSession session);
XrResult xrRequestExitSession(XrSession session);

// Frames
XrResult xrWaitFrame(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState);
XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo);
XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo);

namespace ch = std::chrono;

typedef uint64_t Frame;

enum FrameState {
    NewFrameAllowed,
    NewFrameBusy,
    Waiting,
    Rendering,
    Ended
};

struct GB_Session {
    XrSession id;
    XrInstance instance;
    XrSystemId system;
    XrViewConfigurationType view_configuration;
    std::shared_ptr<EventStreamReader> hotkey_events_reader;
    std::shared_ptr<EventStreamWriter> instance_event_stream_writer;

    // Session state
    std::mutex mutex_session_state_queue;
    std::vector<XrSessionState> session_state_queue;
    XrSessionState session_state;

    //std
    std::chrono::high_resolution_clock::time_point session_epoch;

    // Frame logic
    FrameState wait_frame_state;
    std::mutex mutex_wait_frame_state;
    Frame waited_frame = 0;
    Frame started_frame = 0;
    Frame ended_frame = 0;
    bool end_frame_called = true;
    bool should_render = false;

    // Views
    std::array<XrView, 2> views;
    float leye_x = -0.0015f, reye_x = 0.0015f;
    float eye_z = 0.50f;
    // Weaving
    bool should_weave = true;

    // Compositor
    Renderer* renderer;

    // SR
    std::shared_ptr<SR::SRContext> sr_context;

    void IdleFunc();
    void InitializeView();
};

class GB_FrameTimer {
    uint32_t frame_index = 0;
    FrameState state = Waiting;

    std::chrono::high_resolution_clock::time_point last_frame_start;
    std::chrono::high_resolution_clock::duration last_frame_time;
    std::chrono::high_resolution_clock::duration next_frame_time;

    GB_FrameTimer() = delete;
    GB_FrameTimer(GB_FrameTimer& other) = delete;
    GB_FrameTimer(GB_FrameTimer&& other) = delete;

    explicit GB_FrameTimer(uint32_t frame_index) : frame_index(frame_index) {
    }

    void StartNewFrame(uint32_t frame_id) {
    }

    void EndFrame(uint32_t frame_id) {
    }
};

inline std::vector<GB_FrameTimer> g_frames;

void ChangeSessionState(GB_Session& session, XrSessionState state);

void UpdateSession(GB_Session& session);

void SetXrViewPose(GB_Session& session, uint32_t index, const XrPosef& pose);
void SetXrViewFov(GB_Session& session, uint32_t index, const XrFovf& fov);
