#pragma once

#include <vector>
#include <chrono>
#include <mutex>

#include "openxr_includes.h"
#include "window.h"
#include "swapchain.h"
#include "compositor.h"

#include "srhelpers.h"
#include "weaver_directx_12.h"

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

namespace XRGameBridge {
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

        // Session state
        std::mutex mutex_session_state_queue;
        std::vector<XrSessionState> session_state_queue;
        XrSessionState session_state;

        //std
        std::chrono::high_resolution_clock::time_point session_epoch;

        // Frame logic
        FrameState wait_frame_state;
        std::mutex wait_frame_state_mutex;
        Frame waited_frame = 0;
        Frame started_frame = 0;
        Frame ended_frame = 0;
        bool end_frame_called;
        bool should_render = false;

        // DirectX 12
        ComPtr<ID3D12Device> d3d12_device;
        ComPtr<ID3D12CommandQueue> command_queue;
        GB_Compositor compositor;
        GB_ProxySwapchain intermediate_resource;

        // Windows
        GB_Display display;
        GB_GraphicsDevice window_swapchain;

        // SR
        SR::SRContext* sr_context;
        DirectX12Weaver* d3d12weaver;
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
}
