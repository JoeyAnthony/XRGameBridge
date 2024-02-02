#include "instance.h"

#include <stdexcept>
#include <vector>
#include <easylogging++.h>
#include <set>

#include "openxr_functions.h"

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

using namespace GameBridge;

// Not really a global...
// TODO a list of instances in the future?
GB_Instance* g_gbinstance = nullptr;

XrResult xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) {
    try {
        *function = GameBridge::openxr_functions.at(name);
    }
    catch (std::out_of_range& e) {
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
    else if (propertyCapacityInput < array_size) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }
    // Return whether the extension exists
    else {
        // Fill array
        memcpy_s(properties, propertyCapacityInput * sizeof(XrExtensionProperties), supported_extensions.data(), array_size * sizeof(XrExtensionProperties));
        return XR_SUCCESS;
    }
}

XrResult xrCreateInstance(const XrInstanceCreateInfo* createInfo, XrInstance* instance) {
    if (createInfo == nullptr) {
        LOG(INFO) << "Invalid XrInstanceCreateInfo";
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    if (createInfo->type != XR_TYPE_INSTANCE_CREATE_INFO) {
        LOG(INFO) << "createInfo struct type not XR_TYPE_INSTANCE_CREATE_INFO";
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Only support a single instance for now
    if (g_gbinstance != nullptr) {
        return XR_ERROR_LIMIT_REACHED;
    }

    // Check application info
    XrApplicationInfo app_info = createInfo->applicationInfo;
    std::string app_name = app_info.applicationName;

    LOG(INFO) << "Application name: "       << app_name;
    LOG(INFO) << "Api version: "            << app_info.apiVersion;
    LOG(INFO) << "Application version: "    << app_info.applicationVersion;
    LOG(INFO) << "Engine: "                 << app_info.engineName;
    LOG(INFO) << "Engine version: "         << app_info.engineVersion;

    if (app_name.empty()) {
        return XR_ERROR_NAME_INVALID;
    }

    if (app_info.apiVersion != XR_CURRENT_API_VERSION) {
        return XR_ERROR_API_VERSION_UNSUPPORTED;
    }

    // Check api layers
    const char* const* api_layers = createInfo->enabledApiLayerNames;

    // Check extensions
    const char* const* api_extensions = createInfo->enabledExtensionNames;
    // Create set of application extensions
    std::set <std::string> application_extensions;
    for(uint32_t i  = 0; i < createInfo->enabledExtensionCount; i++)
    {
        application_extensions.insert(application_extensions.end(), api_extensions[i]);
    }
    // Remove our extensions from application extensions
    for (auto extension : supported_extensions)
    {
        application_extensions.erase(extension.extensionName);
    }
    if(!application_extensions.empty())
    {
        LOG(ERROR) << "Unsupported extensions: ";
        for(auto extension : application_extensions)
        {
            LOG(ERROR) << "\t" << extension;
        }

        return XR_ERROR_EXTENSION_NOT_PRESENT;
    }

    // Create new instance
    g_gbinstance = new GB_Instance();
    *instance = reinterpret_cast<XrInstance>(g_gbinstance);

    LOG(INFO) << "New GameBridge Instance created";
    return XR_SUCCESS;
}

XrResult xrGetInstanceProperties(XrInstance instance, XrInstanceProperties* instanceProperties) {
    // TODO Make a list of instances to check whether passed instances are valid or not
    GB_Instance* gb_instance = reinterpret_cast<GameBridge::GB_Instance*>(instance);

    strcpy_s(instanceProperties->runtimeName, XR_MAX_RUNTIME_NAME_SIZE, gb_instance->runtime_name.data());
    instanceProperties->runtimeVersion = gb_instance->runtime_version;

    return XR_SUCCESS;
}

XrResult xrDestroyInstance(XrInstance instance) {
    return test_return;
}

std::vector<IDXGIAdapter*> EnumerateAdapters(void) {
    IDXGIAdapter* adapter;
    std::vector<IDXGIAdapter*> adapters;
    IDXGIFactory1* factory = NULL;

    // Create a DXGIFactory object.
    HRESULT err = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
    if (FAILED(err)) {
        LOG(ERROR) << "Could not create DXGIFactory with error: " << err;
        return adapters;
    }

    for (UINT i = 0; factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        adapters.push_back(adapter);
    }

    if (factory) {
        factory->Release();
    }

    return adapters;
}

std::map<uint32_t, DXGI_ADAPTER_DESC> DetermineDeviceScores(std::vector<IDXGIAdapter*> adapters) {
    // VRAM
    uint32_t dedicated_memory_modifier = 2;
    // RAM used only by the GPU (integrated graphics)
    uint32_t dedicated_system_memory_modifier = 1;
    // CPU ram shared with the GPU
    uint32_t shared_memory_modifier = 0.1;

    std::map<uint32_t, DXGI_ADAPTER_DESC> adapter_scores;
    for (auto adapter : adapters) {
        DXGI_ADAPTER_DESC adapter_desc;
        adapter->GetDesc(&adapter_desc);

        uint32_t adapter_score = 0;

        adapter_score += dedicated_memory_modifier * adapter_desc.DedicatedVideoMemory;
        adapter_score += dedicated_system_memory_modifier * adapter_desc.DedicatedSystemMemory;
        adapter_score += shared_memory_modifier * adapter_desc.SharedSystemMemory;

        // TODO If a pc has two identical GPU'S the first one may be overwritten here
        adapter_scores[adapter_score] = adapter_desc;
    }

    return adapter_scores;
}

// DX11 and DX12 requirements functions have the same logic, they do have different out types
XrResult xrGetD3D11GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D11KHR* graphicsRequirements) {

    auto adapters = EnumerateAdapters();
    auto adapter_scores = DetermineDeviceScores(adapters);

    if (adapter_scores.size() > 0) {

        GameBridge::GB_Instance* gb_instance = reinterpret_cast<GameBridge::GB_Instance*>(instance);
        // Check if the system is the same as the one in the instance
        if (systemId == gb_instance->system.id) {
            gb_instance->system.feature_level = D3D_FEATURE_LEVEL_11_0;
            gb_instance->system.features_enumerated = true;

            // Give graphics requirements to the connected application
            graphicsRequirements->adapterLuid = adapter_scores.begin()->second.AdapterLuid;
            graphicsRequirements->minFeatureLevel = D3D_FEATURE_LEVEL_11_0;

            return XR_SUCCESS;
        }

        return XR_ERROR_SYSTEM_INVALID;
    }

    LOG(ERROR) << "No devices found";
    return XR_ERROR_RUNTIME_FAILURE;
}

XrResult xrGetD3D12GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D12KHR* graphicsRequirements) {
    auto adapters = EnumerateAdapters();
    auto adapter_scores = DetermineDeviceScores(adapters);

    if (adapter_scores.size() > 0) {

        GameBridge::GB_Instance* gb_instance = reinterpret_cast<GameBridge::GB_Instance*>(instance);
        // Check if the system is the same as the one in the instance
        if (systemId == gb_instance->system.id) {
            gb_instance->system.feature_level = D3D_FEATURE_LEVEL_11_0;
            gb_instance->system.features_enumerated = true;

            // Give graphics requirements to the connected application
            graphicsRequirements->adapterLuid = adapter_scores.begin()->second.AdapterLuid;
            graphicsRequirements->minFeatureLevel = D3D_FEATURE_LEVEL_11_0;

            return XR_SUCCESS;
        }

        return XR_ERROR_SYSTEM_INVALID;
    }

    LOG(ERROR) << "No devices found";
    return XR_ERROR_RUNTIME_FAILURE;
}

XrResult xrStringToPath(XrInstance instance, const char* pathString, XrPath* path)
{
    GB_Instance* gb_instance = reinterpret_cast<GB_Instance*>(instance);
    XrPath xr_path = string_hasher(pathString);
    *path = xr_path;

    // Don't overwrite the path if it already exists
    if (!gb_instance->xrpath_storage[xr_path].empty()) {
        return XR_SUCCESS;
    }

    gb_instance->xrpath_storage[xr_path] = std::string(pathString);
    *path = xr_path;

    return XR_SUCCESS;
}

XrResult xrPathToString(XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer)
{
    GB_Instance* gb_instance = reinterpret_cast<GB_Instance*>(instance);
    std::string string_path = gb_instance->xrpath_storage[path];

    if(string_path.empty()) 
    {
        return XR_ERROR_PATH_INVALID;
    }

    uint32_t path_size = static_cast<uint32_t>(string_path.size());
    *bufferCountOutput = path_size;

    if(bufferCapacityInput == 0)
    {
        return XR_SUCCESS;
    }
    if(bufferCapacityInput < path_size)
    {
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
        // Doing it just in case but we might not care about it since we may not want to process input actions for SR.
        return XR_ERROR_LOCALIZED_NAME_INVALID;
    }

    XrActionSet handle = reinterpret_cast<XrActionSet>(string_hasher(action_set_name));

    auto pair = action_sets.insert({ handle, GB_ActionSet{ instance, createInfo->priority, localized_action_set_name } });
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
    GB_ActionSet to_delete;
    try {
        to_delete = action_sets.at(actionSet);
    }
    catch (std::out_of_range& e) {
        LOG(ERROR) << "Failed adding action set: " << to_delete.localized_name << "does not exist";
        return XR_ERROR_HANDLE_INVALID;
    }
    catch (std::exception& e) {
        LOG(ERROR) << "Exception occurred: " << e.what();
        return XR_ERROR_RUNTIME_FAILURE;
    }

    std::erase_if(actions, [&](const auto& item)-> bool {
        auto const& [key, value] = item;
            if (value.action_set == actionSet) {
                return true;
            }
            return false;
        }
    );

    LOG(INFO) << "Unregistered action: " << to_delete.localized_name;
    action_sets.erase(actionSet);

    return XR_SUCCESS;
}

XrResult xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action) {
    GB_ActionSet gb_action_set;
    try {
        gb_action_set = action_sets.at(actionSet);
    }
    catch (std::out_of_range& e) {
        LOG(ERROR) << "Failed adding action: " << createInfo->actionName << ". Action set: " << gb_action_set.localized_name << "does not exist";
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

    // TODO Can only register unique action names. I cannot register the same action handles to different actions sets. Is this a problem?
    auto pair = actions.insert({ handle, new_action });
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
    GB_Action to_delete;
    try {
        to_delete = actions.at(action);
    }
    catch (std::out_of_range& e) {
        LOG(ERROR) << "Failed adding action: " << to_delete.localized_name << "does not exist";
        return XR_ERROR_HANDLE_INVALID;
    }
    catch (std::exception& e) {
        LOG(ERROR) << "Exception occurred: " << e.what();
        return XR_ERROR_RUNTIME_FAILURE;
    }

    LOG(INFO) << "Unregistered action: " << to_delete.localized_name;
    actions.erase(action);

    return XR_SUCCESS;
}

XrResult xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings) {
    // TODO not implemented since we may not need this for now https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#semantic-path-interaction-profiles
    // Vendor specific input mappings

    GameBridge::GB_Instance* gb_instance = reinterpret_cast<GameBridge::GB_Instance*>(instance);
    suggestedBindings = &gb_instance->suggested_bindings;
    return XR_SUCCESS;
};