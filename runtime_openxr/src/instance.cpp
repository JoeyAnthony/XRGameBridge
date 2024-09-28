#include "instance.h"

#include <stdexcept>
#include <vector>
#include <set>

#include <easylogging++.h>
#include <hotkey_windows_impl.h>

#include <game_bridge_structs.h>

#include "actions.h"
#include "openxr_functions.h"
#include "swapchain.h"
#include "system.h"

//class OpenXRContainers {
//public:
//    std::vector<XrInstance> instance_handles;
//    std::vector<GBInstance*> instances;
//
////public:
////    void Add(XrInstance instance, GBInstance* gb_instance);
////    const GBInstance* GetGameBridgeInstance(XrInstance);
//
//} static g_openxr_stuff;

using namespace XRGameBridge;

XrResult xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) {
    try {
        *function = openxr_functions.at(name);
    }
    catch (std::out_of_range& e) {
        //LOG(WARNING) << "FUNCTION UNSUPPORTED: " << name << " Error: " << e.what();
        return XR_ERROR_FUNCTION_UNSUPPORTED;
    }
    catch (std::exception& e) {
        return XR_ERROR_RUNTIME_FAILURE; // Generic error
    }

    LOG(INFO) << "Address retrieved: " << name;
    return XR_SUCCESS;
}

XrResult xrNegotiateLoaderRuntimeInterface(const XrNegotiateLoaderInfo* loaderInfo, XrNegotiateRuntimeRequest* runtimeRequest) {
    LOG(INFO) << "xrNegotiateLoaderRuntimeInterface";
    LOG(INFO) << "\tminApiVersion " << loaderInfo->minApiVersion;
    LOG(INFO) << "\tmaxApiVersion " << loaderInfo->maxApiVersion;

    runtimeRequest->runtimeApiVersion = XR_CURRENT_API_VERSION;
    runtimeRequest->runtimeInterfaceVersion = XR_CURRENT_LOADER_RUNTIME_VERSION;
    runtimeRequest->getInstanceProcAddr = &xrGetInstanceProcAddr;

    LOG(INFO) << "\truntimeApiVersion %i", runtimeRequest->runtimeApiVersion;
    LOG(INFO) << "\truntimeInterfaceVersion %i", runtimeRequest->runtimeInterfaceVersion;

    return XR_SUCCESS;
}

XrResult xrEnumerateInstanceExtensionProperties(const char* layerName, uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrExtensionProperties* properties) {
    LOG(INFO) << "Called: xrEnumerateInstanceExtensionProperties";
    const uint32_t array_size = static_cast<uint32_t>(supported_extensions.size());

    *propertyCountOutput = array_size;

    // Request for the extension array or the extension array itself
    if (layerName == nullptr && propertyCapacityInput == 0) {
        return XR_SUCCESS;
    }
    // Passed array not large enough
    if (propertyCapacityInput < array_size) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }
    // Return whether the extension exists

    // Fill array
    memcpy_s(properties, propertyCapacityInput * sizeof(XrExtensionProperties), supported_extensions.data(), array_size * sizeof(XrExtensionProperties));
    return XR_SUCCESS;
}

XrResult xrCreateInstance(const XrInstanceCreateInfo* createInfo, XrInstance* instance) {
    LOG(INFO) << "Creating instance created session: ";

    if (createInfo == nullptr) {
        LOG(INFO) << "Invalid XrInstanceCreateInfo";
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    if (createInfo->type != XR_TYPE_INSTANCE_CREATE_INFO) {
        LOG(INFO) << "createInfo struct type not XR_TYPE_INSTANCE_CREATE_INFO";
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Only support a single instance for now
    if (g_xr_instance != nullptr) {
        return XR_ERROR_LIMIT_REACHED;
    }

    // Check application info
    XrApplicationInfo app_info = createInfo->applicationInfo;
    std::string app_name = app_info.applicationName;

    LOG(INFO) << "Application name: " << app_name;
    LOG(INFO) << "Api version: " << app_info.apiVersion;
    LOG(INFO) << "Application version: " << app_info.applicationVersion;
    LOG(INFO) << "Engine: " << app_info.engineName;
    LOG(INFO) << "Engine version: " << app_info.engineVersion;

    if (app_name.empty()) {
        return XR_ERROR_NAME_INVALID;
    }

    // Todo doing this wrong
    //if (app_info.apiVersion != XR_CURRENT_API_VERSION) {
    //    return XR_ERROR_API_VERSION_UNSUPPORTED;
    //}

    // Check api layers
    const char* const* api_layers = createInfo->enabledApiLayerNames;

    // Check extensions
    const char* const* api_extensions = createInfo->enabledExtensionNames;
    // Create set of application extensions
    std::set <std::string> application_extensions;
    for (uint32_t i = 0; i < createInfo->enabledExtensionCount; i++) {
        application_extensions.insert(application_extensions.end(), api_extensions[i]);
    }
    // Remove our extensions from application extensions
    for (auto extension : supported_extensions) {
        application_extensions.erase(extension.extensionName);
    }
    if (!application_extensions.empty()) {
        LOG(ERROR) << "Unsupported extensions: ";
        for (auto extension : application_extensions) {
            LOG(ERROR) << "\t" << extension;
        }

        return XR_ERROR_EXTENSION_NOT_PRESENT;
    }

    /* TODO Make a Game Bridge class
    * This class will initialize game bridge and SR
    * After those are initialized, a connected screen can be retreived.
    * With this the Systems can be initialized.
    *
    * There should always be one system, that is the main monitor. This is never an SR screen.
    * Then later when SR is fulliy initialized, the main screen can be replaced by the SR screen, and the 3D pipeline will be activated.
    *
    * Flow:
    * Initialize main screen ->
    * Initialize SR on a separate thread ->
    * Initialize the rutime until resolutions need to be returned ->
    * Check if SR is initialized and an SR System is created ->
    * If so Use the SR resolutions, otherwise use the main screen (Or a user set resolution)
    * When Game Bridge gives an event that a SR screen has been created, swap the systems.
    *
    */

    // Create new instance
    g_xr_instance = new GB_Instance();
    *instance = reinterpret_cast<XrInstance>(g_xr_instance);

    InitializeSystems(*instance);

    // Check the context
    if (g_xr_instance->GetPlatformManager()->GetContext() == nullptr) {
        LOG(ERROR) << "Failed to connect to the SR service";
        return XR_ERROR_RUNTIME_FAILURE;
    }

    LOG(INFO) << "XR Instance created";
    return XR_SUCCESS;
}

XrResult xrGetInstanceProperties(XrInstance instance, XrInstanceProperties* instanceProperties) {
    // TODO Make a list of instances to check whether passed instances are valid or not
    GB_Instance* gb_instance = reinterpret_cast<GB_Instance*>(instance);

    strcpy_s(instanceProperties->runtimeName, XR_MAX_RUNTIME_NAME_SIZE, gb_instance->GetRuntimeName().data());
    instanceProperties->runtimeVersion = gb_instance->GetRuntimeVersion();

    return XR_SUCCESS;
}

XrResult xrDestroyInstance(XrInstance instance) {

    // Delete sessions

    // Delete actions
    // TODO Make the instance destroy all owned objects here as well

    delete XRGameBridge::g_xr_instance;

    LOG(INFO) << "Called " << __func__; return XR_ERROR_RUNTIME_FAILURE;
}

// DX11 and DX12 requirements functions have the same logic, they do have different out types
XrResult xrGetD3D11GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D11KHR* graphicsRequirements) {
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;
    GB_GraphicsDevice::CreateDXGIFactory(&factory);
    GB_GraphicsDevice::GetGraphicsAdapter(factory.Get(), &hardwareAdapter, true);

    if (factory == nullptr) {
        LOG(ERROR) << "No suitable device found";
        return XR_ERROR_SYSTEM_INVALID;
    }

    try {
        GB_System& system = g_systems.at(systemId);

        if (system.instance != instance) {
            LOG(ERROR) << "Instance not bound to this system";
            return XR_ERROR_HANDLE_INVALID;
        }

        system.feature_level = D3D_FEATURE_LEVEL_11_0;
        system.features_enumerated = true;

        //GB_Instance gb_instance = instances.at(instance);
        g_xr_instance->ActivateGraphicsAPI(GraphicsBackend::D3D11);
        system.active_graphics_backend = GraphicsBackend::D3D11;
    }
    catch (std::out_of_range& e) {
        return XR_ERROR_SYSTEM_INVALID;
    }
    catch (std::exception& e) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    // Give graphics requirements to the connected application
    DXGI_ADAPTER_DESC1 desc;
    hardwareAdapter->GetDesc1(&desc);
    graphicsRequirements->adapterLuid = desc.AdapterLuid;
    graphicsRequirements->minFeatureLevel = D3D_FEATURE_LEVEL_11_0;

    return XR_SUCCESS;
}

XrResult xrGetD3D12GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D12KHR* graphicsRequirements) {
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;
    GB_GraphicsDevice::CreateDXGIFactory(&factory);
    GB_GraphicsDevice::GetGraphicsAdapter(factory.Get(), &hardwareAdapter, true);

    if (factory == nullptr) {
        LOG(ERROR) << "No suitable device found";
        return XR_ERROR_SYSTEM_INVALID;
    }

    try {
        GB_System& system = g_systems.at(systemId);

        if (system.instance != instance) {
            LOG(ERROR) << "Instance not bound to this system";
            return XR_ERROR_HANDLE_INVALID;
        }

        system.feature_level = D3D_FEATURE_LEVEL_11_0;
        system.features_enumerated = true;

        //GB_Instance gb_instance = instances.at(instance);
        //TODO Do I need this in both? Maybe only in system sincen that the device that renders in the end
        g_xr_instance->ActivateGraphicsAPI(GraphicsBackend::D3D12);
        system.active_graphics_backend = GraphicsBackend::D3D12;
        LOG(INFO) << "";
    }
    catch (std::out_of_range& e) {
        return XR_ERROR_SYSTEM_INVALID;
    }
    catch (std::exception& e) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    // Give graphics requirements to the connected application
    DXGI_ADAPTER_DESC1 desc;
    hardwareAdapter->GetDesc1(&desc);
    graphicsRequirements->adapterLuid = desc.AdapterLuid;
    graphicsRequirements->minFeatureLevel = D3D_FEATURE_LEVEL_11_0;

//#ifdef _DEBUG
//    // Enable the D3D12 debug layer.
//    {
//        ComPtr<ID3D12Debug> debugController;
//        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())))) {
//            debugController->EnableDebugLayer();
//        }
//    }
//#endif
    return XR_SUCCESS;
}

XrResult xrStringToPath(XrInstance instance, const char* pathString, XrPath* path) {
    XrPath xr_path = string_hasher(pathString);
    *path = xr_path;

    // Don't overwrite the path if it already exists
    if (!g_xrpath_storage[xr_path].empty()) {
        return XR_SUCCESS;
    }

    g_xrpath_storage[xr_path] = std::string(pathString);
    *path = xr_path;

    return XR_SUCCESS;
}

XrResult xrPathToString(XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer) {
    std::string string_path = g_xrpath_storage[path];

    if (string_path.empty()) {
        return XR_ERROR_PATH_INVALID;
    }

    uint32_t path_size = static_cast<uint32_t>(string_path.size());
    *bufferCountOutput = path_size;

    if (bufferCapacityInput == 0) {
        return XR_SUCCESS;
    }
    if (bufferCapacityInput < path_size) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    strcpy_s(buffer, path_size, string_path.data());

    return XR_SUCCESS;
}

XrResult xrCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet) {
    const std::string action_set_name(createInfo->actionSetName);
    const std::string localized_action_set_name(createInfo->localizedActionSetName);

    if (action_set_name.empty() || localized_action_set_name.empty()) {
        // Specification says to return XR_ERROR_LOCALIZED_NAME_INVALID when either of the names ar empty.
        // Doing it just in case but we might not care about it since we may not want to process input g_actions for SR.
        return XR_ERROR_LOCALIZED_NAME_INVALID;
    }

    XrActionSet handle = reinterpret_cast<XrActionSet>(string_hasher(action_set_name));

    GB_ActionSet new_action_set;
    new_action_set.instance = instance;
    new_action_set.priority = createInfo->priority;
    new_action_set.localized_name = localized_action_set_name;

    auto pair = g_action_sets.insert({ handle,  new_action_set });
    if (pair.second) {
        *actionSet = handle;
    }
    else {
        // Iterator to the pair element
        LOG(WARNING) << "Action set already exists: " << localized_action_set_name;
        return XR_ERROR_NAME_DUPLICATED;
    }

    return XR_SUCCESS;
}

XrResult xrDestroyActionSet(XrActionSet actionSet) {
    try {
        GB_ActionSet& to_delete = g_action_sets.at(actionSet);

        LOG(INFO) << "Unregistered action: " << to_delete.localized_name;
        g_action_sets.erase(actionSet);
    }
    catch (std::out_of_range& e) {
        LOG(ERROR) << "Action set not found";
        return XR_ERROR_HANDLE_INVALID;
    }
    catch (std::exception& e) {
        LOG(ERROR) << "Exception occurred: " << e.what();
        return XR_ERROR_RUNTIME_FAILURE;
    }

    // Remove g_actions linked to the action set
    std::erase_if(g_actions, [&](const auto& item)-> bool {
        auto const& [key, value] = item;
        if (value.action_set == actionSet) {
            return true;
        }
        return false;
        }
    );

    return XR_SUCCESS;
}

XrResult xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action) {
    try {
        GB_ActionSet& gb_action_set = g_action_sets.at(actionSet);
    }
    catch (std::out_of_range& e) {
        LOG(ERROR) << "Action set does not exist";
        return XR_ERROR_HANDLE_INVALID;
    }
    catch (std::exception& e) {
        LOG(ERROR) << "Exception occurred: " << e.what();
        return XR_ERROR_RUNTIME_FAILURE;
    }

    GB_Action new_action{};
    new_action.action_set = actionSet;
    new_action.type = createInfo->actionType;
    new_action.sub_action_paths.insert(new_action.sub_action_paths.begin(), createInfo->subactionPaths, createInfo->subactionPaths + createInfo->countSubactionPaths);
    new_action.localized_name = createInfo->localizedActionName;

    XrAction handle = reinterpret_cast<XrAction>(string_hasher(createInfo->actionName));

    // TODO Can only register unique action names. I cannot register the same action handles to different g_actions sets. Is this a problem?
    auto pair = g_actions.insert({ handle, new_action });
    if (pair.second) {
        *action = handle;
    }
    else {
        LOG(WARNING) << "Action already exists: " << new_action.localized_name << "";
        return XR_ERROR_NAME_DUPLICATED;
    }

    return XR_SUCCESS;
}

XrResult xrDestroyAction(XrAction action) {
    try {
        GB_Action& to_delete = g_actions.at(action);

        LOG(INFO) << "Unregistered action: " << to_delete.localized_name;
        g_actions.erase(action);
    }
    catch (std::out_of_range& e) {
        LOG(ERROR) << "Action does not exist";
        return XR_ERROR_HANDLE_INVALID;
    }
    catch (std::exception& e) {
        LOG(ERROR) << "Exception occurred: " << e.what();
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return XR_SUCCESS;
}

XrResult xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo) {
    try {
        const std::vector<XrActionSet> attach_info_sets(attachInfo->actionSets, attachInfo->actionSets + attachInfo->countActionSets);

        for (uint32_t i = 0; i < attach_info_sets.size(); i++) {
            GB_ActionSet& gb_action_set = g_action_sets.at(attach_info_sets[i]);
            gb_action_set.session = session;
        }
    }
    catch (std::out_of_range& e) {
        LOG(ERROR) << "Action does not exist";
        return XR_ERROR_HANDLE_INVALID;
    }
    catch (std::exception& e) {
        LOG(ERROR) << "Exception occurred: " << e.what();
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return XR_SUCCESS;
}

XrResult xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings) {
    // TODO not implemented since we may not need this for now https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#semantic-path-interaction-profiles
    // Vendor specific input mappings

    GB_Instance* gb_instance = reinterpret_cast<GB_Instance*>(instance);
    //suggestedBindings = &gb_instance->suggested_bindings;
    return XR_SUCCESS;
}

XrResult xrGetCurrentInteractionProfile(XrSession session, XrPath topLevelUserPath, XrInteractionProfileState* interactionProfile) {
    std::string string_path = g_xrpath_storage[topLevelUserPath];
    if(std::find(XRGameBridge::g_supported_paths.begin(), XRGameBridge::g_supported_paths.end(), string_path) == XRGameBridge::g_supported_paths.end())
    {
        return XR_ERROR_PATH_UNSUPPORTED;
    }

    return XR_SUCCESS;
}

XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData) {
    // TODO need event stream reader for poll events
    uint32_t event_type;
    void* data = g_openxr_event_stream_reader->GetNextEvent(event_type);
    if (event_type == GB_EVENT_NULL) {
        return XR_EVENT_UNAVAILABLE;
    }

    XrEventDataSessionStateChanged* state_change = reinterpret_cast<XrEventDataSessionStateChanged*>(data);
    if (state_change->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
        uint32_t objsize = sizeof(XrEventDataSessionStateChanged);
        eventData->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
        memcpy_s(eventData, XR_MAX_EVENT_DATA_SIZE, data, objsize);
        return XR_SUCCESS;
    }

    // Runtime tries to send an event that is not supported.
    return XR_ERROR_RUNTIME_FAILURE;
}

void XRGameBridge::InitializeSystems(XrInstance instance) {
    CreateXrGameBridgeSystems(instance);
};

XRGameBridge::GB_Instance::GB_Instance() {
    // Set dpi awareness for the application
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
    InitializeSR();

    // TODO move to input class
    // Initialize hotkey manager
    HotkeyManagerInitialize hotkey_params{};
    hotkey_params.game_bridge = gamebridge_instance;
    hotkey_params.implementation = std::make_shared<WindowsHotkeyImplementation>();
    g_hotkey_manager = new HotkeyManager(hotkey_params);

    // TODO move to event xr handler class
    // Set-up event streams for the runtime
    auto& event_manager = gamebridge_instance->GetEventManager();
    g_openxr_event_stream_writer = event_manager.CreateEventStream(GB_EVENT_STREAM_TYPE_XR_GAME_BRIDGE, 300, XR_MAX_EVENT_DATA_SIZE);
    g_openxr_event_stream_reader = event_manager.GetEventStreamReader(GB_EVENT_STREAM_TYPE_XR_GAME_BRIDGE);

    //g_openxr_event_stream_writer->SubmitEvent(XR_TYPE_EVENT_DATA_EVENTS_LOST, 200, nullptr);

#ifdef _DEBUG
    window_hook = new WindowHooks();
    window_hook->OpenConsole();
    window_hook->ActivateWindowMessageHook();
#endif

}

XRGameBridge::GB_Instance::~GB_Instance() {
    delete gamebridge_instance;
    delete platform_manager;
}

void XRGameBridge::GB_Instance::InitializeSR() {
    gamebridge_instance = new GameBridge(EventManager());

    SRPlatformManagerInitialize params{};
    platform_manager = new PlatformManager(params);

    while (!platform_manager->InitializeSRContext()) {
        LOG(INFO) << "Failed creating SR context, retrying..";
    }
}

XrResult XRGameBridge::GB_Instance::ActivateGraphicsAPI(GraphicsBackend api) {
    if (active_graphics_backend == GraphicsBackend::undefined) {
        active_graphics_backend == api;
    }
    else {
        LOG(ERROR) << "Active graphics api can only be set once";
        return XR_ERROR_RUNTIME_FAILURE;
    }
}

GameBridge* XRGameBridge::GB_Instance::GetGameBridgeInstane() {
    return gamebridge_instance;
}

PlatformManager* XRGameBridge::GB_Instance::GetPlatformManager() {
    return platform_manager;
}

std::string XRGameBridge::GB_Instance::GetRuntimeName() {
    return runtime_name;
}

uint64_t XRGameBridge::GB_Instance::GetRuntimeVersion() {
    return runtime_version;
}

GraphicsBackend XRGameBridge::GB_Instance::GetActiveGraphicsAPI() {
    return active_graphics_backend;
}
