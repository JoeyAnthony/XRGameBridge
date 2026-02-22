#pragma once
#include <stdint.h>

// TODO Move all the export definitions in the project to an common header file for internal use only (if that works)
#include <cstddef>
#include <cstdint>

typedef uint32_t GB_EVENT;

// Managers
enum GameBridgeManagerType {
    GB_MANAGER_HOTKEY,
    GB_MANAGER_EVENTS,
    GB_MANAGER_WEAVER_DX11,
    GB_MANAGER_WEAVER_DX12,
    GB_MANAGER_PLATFORM
};

// Initialization
enum GameBridgeEventManagerInitialize {
    GB_PROCESS_EVENTS_AT_THE_END_OF_CURRENT_FRAME,
    GB_PROCESS_EVENTS_ON_THE_NEXT_FRAME
};

enum GameBridgeHotkeyManagerInitialize {
    GB_MANAGER_HOTKEY_NO_FLAGS
};

// Event manager types
enum EventStreamType {
    GB_EVENT_STREAM_TYPE_HOTKEY,
    GB_EVENT_STREAM_TYPE_PLATFORM,
    GB_EVENT_STREAM_TYPE_WEAVER,
    GB_EVENT_STREAM_TYPE_XR_GAME_BRIDGE
};

// Messages
// Reserve 0 as the NULL EVENT
constexpr size_t GB_EVENT_NULL = 0;

enum GameBridgeHotKeyEvent {
    GB_EVENT_HOTKEY_TOGGLE_LENS = 1,
    GB_EVENT_HOTKEY_TOGGLE_WEAVING = 2,
    GB_EVENT_HOTKEY_INCREASE_CONVERGEANCE = 3,
    GB_EVENT_HOTKEY_DECREASE_CONVERGEANCE = 4,
    GB_EVENT_HOTKEY_INCREASE_SEPARATION = 5,
    GB_EVENT_HOTKEY_DECREASE_SEPARATION = 6,
    GB_EVENT_HOTKEY_INCREASE_FOV = 7,
    GB_EVENT_HOTKEY_DECREASE_FOV = 8,

    GB_EVENT_TEST_UP,
    GB_EVENT_TEST_DOWN,
    GB_EVENT_TEST_LEFT,
    GB_EVENT_TEST_RIGHT,
    GB_EVENT_TEST_RESET,
};

enum GameBridgePlatformEvent {
    GB_EVENT_PLATFORM_CONTEXT_INVALIDATED = 1
};

enum GameBridgeWeaverEvent {
    GB_EVENT_WEAVER_WEAVING_ENABLED = 1
};

// SR weaver flags
enum GameBridgeWeaverFlags {
    GB_ENABLE_DEBUG_OVERLAY_OR_WHATEVER,
    GB_MANUAL_SET_CAMERA_LATENCY_VALUE,
    GB_MANUAL_SET_CAMERA_LATENCY_IN_FRAMES__VALUE
};
