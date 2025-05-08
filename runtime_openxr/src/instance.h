#pragma once
#include <string>
#include <unordered_map>

#include "dll.h"
#include "openxr_includes.h"
#include "session.h"
#include "swapchain.h"
#include "system.h"

#include <game_bridge.h>
#include <hooks.h>

#include "event_manager.h"
#include "hotkey_manager.h"
#include "platform_manager.h"

//TODO fix versioning
#define RUNTIME_VERSION_MAYOR 0
#define RUNTIME_VERSION_MINOR 0
#define RUNTIME_VERSION_PATCH 1

DllExport XrResult xrNegotiateLoaderRuntimeInterface(const XrNegotiateLoaderInfo* loaderInfo, XrNegotiateRuntimeRequest* runtimeRequest);

XrResult xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function);

XrResult xrEnumerateInstanceExtensionProperties(const char* layerName, uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrExtensionProperties* properties);
XrResult xrCreateInstance(const XrInstanceCreateInfo* createInfo, XrInstance* instance);

XrResult xrGetInstanceProperties(XrInstance instance, XrInstanceProperties* instanceProperties);

XrResult xrDestroyInstance(XrInstance instance);

// Directx functions
XrResult xrGetD3D11GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D11KHR* graphicsRequirements);
XrResult xrGetD3D12GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D12KHR* graphicsRequirements);

// Path function
XrResult xrStringToPath(XrInstance instance, const char* pathString, XrPath* path);
XrResult xrPathToString(XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer);

// Actions
XrResult xrCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet);
XrResult xrDestroyActionSet(XrActionSet actionSet);
XrResult xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action);
XrResult xrDestroyAction(XrAction action);
XrResult xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo);

// Suggested bindings
XrResult xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings);
XrResult xrGetCurrentInteractionProfile(XrSession session, XrPath topLevelUserPath, XrInteractionProfileState* interactionProfile);

// Events
inline XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData);

//// Handle functions
//template<typename T>
//T PtrToXrHandle() { return T(); };

//template<typename T>
//T XrhandleToPtr() { return T(); };

//template<typename T>
//T IntToXrHandle() { return T(); };

//inline size_t XrHandleToInt() { return 0; };

class GB_Instance {
    // Cannot be longer than XR_MAX_RUNTIME_NAME_SIZE
    const std::string runtime_name = "XR Game Bridge";
    const uint64_t runtime_version = XR_MAKE_VERSION(RUNTIME_VERSION_MAYOR, RUNTIME_VERSION_MINOR, RUNTIME_VERSION_PATCH);
    GraphicsBackend active_graphics_backend = GraphicsBackend::undefined;
    GameBridge* gamebridge_instance = nullptr;
    PlatformManager* platform_manager = nullptr;

    // Currently not being used
   // XrInteractionProfileSuggestedBinding suggested_bindings;
    void InitializeSR();
public:

    GB_Instance();
    ~GB_Instance();

    XrResult ActivateGraphicsAPI(GraphicsBackend api);

    GameBridge* GetGameBridgeInstance();
    PlatformManager* GetPlatformManager();
    std::string GetRuntimeName();
    uint64_t GetRuntimeVersion();
    GraphicsBackend GetActiveGraphicsAPI();
};

struct GB_ActionSet {
    XrInstance instance;
    XrSession session;
    uint32_t priority;
    std::string localized_name;
};

struct GB_Action {
    // TODO list of action sets so multiple action sets can register the same action? Not sure if the specification wants that.
    XrActionSet action_set;
    XrActionType type;
    std::vector<XrPath> sub_action_paths;
    std::string localized_name;
};

inline GB_Instance* g_xr_instance = nullptr;

#ifdef _DEBUG
// TODO Hooking tests
inline WindowHooks* window_hook;
#endif

inline HotkeyManager* g_hotkey_manager = nullptr;
inline std::shared_ptr<EventStreamWriter> g_openxr_event_stream_writer;
inline std::shared_ptr<EventStreamReader> g_openxr_event_stream_reader;

///! \brief Initialize XR Systems
void InitializeSystems(XrInstance instance);

// Hash class
inline std::hash<std::string> string_hasher;

// Data
// The key is the string hash of an action set path. The same hash is being used for action set handles
inline std::unordered_map<XrPath, std::string> g_xrpath_storage;

// TODO get rid of globals :)
//inline std::unordered_map<XrInstance, GB_Instance> instances;
inline std::unordered_map<XrSession, GB_Session> g_sessions;
inline std::unordered_map<XrSystemId, GB_System> g_systems;
// OpenXR makes a distinction between active and inactive action sets, only active ones have to be updated.
// Since we don't plan on handling VR actions I left this distinction out, but it might be good to have later.
inline std::unordered_map<XrActionSet, GB_ActionSet> g_action_sets;
inline std::unordered_map<XrAction, GB_Action> g_actions;
inline std::unordered_map<XrSpace, GB_ReferenceSpace> g_reference_spaces;
inline std::unordered_map<XrSpace, GB_ActionSpace> g_action_spaces;
inline std::unordered_map<XrSpace, GB_Window> g_displays;
