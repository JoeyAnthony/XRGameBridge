#pragma once

#include <string>
#include <unordered_map>

#include "openxr_includes.h"
#include "openxr/openxr_reflection.h"

inline XrResult xrResultToString(XrInstance instance, XrResult value, char buffer[XR_MAX_RESULT_STRING_SIZE]) {
    TraceLogFunctionCall(__func__, __LINE__);

#define EMIT_RESULT_STRING(name, value)                                                                                \
    case name:                                                                                                         \
        sprintf_s(buffer, XR_MAX_RESULT_STRING_SIZE, "%s", #name);                                                     \
        break;
    switch (value) {
        XR_LIST_ENUM_XrResult(EMIT_RESULT_STRING);

    default:
        if (XR_FAILED(value)) {
            sprintf_s(buffer, XR_MAX_RESULT_STRING_SIZE, "XR_UNKNOWN_FAILURE_%d", (int)value);
        }
        else {
            sprintf_s(buffer, XR_MAX_RESULT_STRING_SIZE, "XR_UNKNOWN_SUCCESS_%d", (int)value);
        }
    }

#undef EMIT_RESULT_STRING

    return XR_SUCCESS;
}

inline XrResult xrStructureTypeToString(XrInstance instance, XrStructureType value, char buffer[XR_MAX_STRUCTURE_NAME_SIZE]) {
    TraceLogFunctionCall(__func__, __LINE__);

#define EMIT_STRUCTURE_TYPE_STRING(name, value)                                                                        \
    case name:                                                                                                         \
        sprintf_s(buffer, XR_MAX_STRUCTURE_NAME_SIZE, "%s", #name);                                                    \
        break;

    switch ((int)value) {
        XR_LIST_ENUM_XrStructureType(EMIT_STRUCTURE_TYPE_STRING);

    default:
        sprintf_s(buffer, XR_MAX_STRUCTURE_NAME_SIZE, "XR_UNKNOWN_STRUCTURE_TYPE_%d", (int)value);
    }

#undef EMIT_STRUCTURE_TYPE_STRING

    return XR_SUCCESS;
}

inline XrResult GetSessionStateString(XrSessionState state, char buffer[XR_MAX_RESULT_STRING_SIZE]) {
    TraceLogFunctionCall(__func__, __LINE__);

#define EMIT_SESSION_STATE_STRING(name, state)                                                                                \
    case name:                                                                                                         \
        sprintf_s(buffer, XR_MAX_RESULT_STRING_SIZE, "%s", #name);                                                     \
        break;
    switch (state) {
        XR_LIST_ENUM_XrSessionState(EMIT_SESSION_STATE_STRING);

    default:
        if (XR_FAILED(state)) {
            sprintf_s(buffer, XR_MAX_RESULT_STRING_SIZE, "XR_UNKNOWN_FAILURE_%d", (int)state);
        }
        else {
            sprintf_s(buffer, XR_MAX_RESULT_STRING_SIZE, "XR_UNKNOWN_SUCCESS_%d", (int)state);
        }
    }

#undef EMIT_SESSION_STATE_STRING

    return XR_SUCCESS;
}

inline XrResult xrGetInputSourceLocalizedName(XrSession session, const XrInputSourceLocalizedNameGetInfo* getInfo, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer) {
    TraceLogFunctionCall(__func__, __LINE__);
    LOG(INFO) << "Called " << __func__; return XR_ERROR_RUNTIME_FAILURE;
}
inline XrResult xrApplyHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo, const XrHapticBaseHeader* hapticFeedback) {
    TraceLogFunctionCall(__func__, __LINE__);
    LOG(INFO) << "Called " << __func__; return XR_ERROR_RUNTIME_FAILURE;
}
inline XrResult xrStopHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo) {
    TraceLogFunctionCall(__func__, __LINE__);
    LOG(INFO) << "Called " << __func__; return XR_ERROR_RUNTIME_FAILURE;
}


// Forward declarations not defined in openxr header (implemented)
XrResult xrGetD3D11GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D11KHR* graphicsRequirements);
XrResult xrGetD3D12GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D12KHR* graphicsRequirements);

//XrResult xrGetOpenGLGraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsOpenGLKHR* graphicsRequirements);
//XrResult xrGetVulkanInstanceExtensionsKHR(XrInstance instance, XrSystemId systemId, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer);
//XrResult xrGetVulkanDeviceExtensionsKHR(XrInstance instance, XrSystemId systemId, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer);
//XrResult xrGetVulkanGraphicsDeviceKHR(XrInstance instance, XrSystemId systemId, VkInstance vkInstance, VkPhysicalDevice* vkPhysicalDevice);
//XrResult xrGetVulkanGraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsVulkanKHR* graphicsRequirements);
//XrResult xrGetVisibilityMaskKHR(XrSession session, XrViewConfigurationType viewConfigurationType, uint32_t viewIndex, XrVisibilityMaskTypeKHR visibilityMaskType, XrVisibilityMaskKHR* visibilityMask);
//XrResult xrCreateVulkanInstanceKHR(XrInstance instance, const XrVulkanInstanceCreateInfoKHR* createInfo, VkInstance* vulkanInstance, VkResult* vulkanResult);
//XrResult xrCreateVulkanDeviceKHR(XrInstance instance, const XrVulkanDeviceCreateInfoKHR* createInfo, VkDevice* vulkanDevice, VkResult* vulkanResult);
//XrResult xrGetVulkanGraphicsDevice2KHR(XrInstance instance, const XrVulkanGraphicsDeviceGetInfoKHR* getInfo, VkPhysicalDevice* vulkanPhysicalDevice);
//XrResult xrGetVulkanGraphicsRequirements2KHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsVulkanKHR* graphicsRequirements);
//XrResult xrCreateHandTrackerEXT(XrSession session, const XrHandTrackerCreateInfoEXT* createInfo, XrHandTrackerEXT* handTracker);
//XrResult xrDestroyHandTrackerEXT(XrHandTrackerEXT handTracker);
//XrResult xrLocateHandJointsEXT(XrHandTrackerEXT handTracker, const XrHandJointsLocateInfoEXT* locateInfo, XrHandJointLocationsEXT* locations);
//XrResult xrEnumerateDisplayRefreshRatesFB(XrSession session, uint32_t displayRefreshRateCapacityInput, uint32_t* displayRefreshRateCountOutput, float* displayRefreshRates);
//XrResult xrGetDisplayRefreshRateFB(XrSession session, float* displayRefreshRate);
//XrResult xrRequestDisplayRefreshRateFB(XrSession session, float displayRefreshRate);


const std::unordered_map<std::string, PFN_xrVoidFunction> openxr_functions {
    { "rGetInstanceProcAddr", reinterpret_cast<PFN_xrVoidFunction>(xrGetInstanceProcAddr)},
    { "xrEnumerateInstanceExtensionProperties",     reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateInstanceExtensionProperties) },
    { "xrCreateInstance",                           reinterpret_cast<PFN_xrVoidFunction>(xrCreateInstance) },
    { "xrDestroyInstance",                          reinterpret_cast<PFN_xrVoidFunction>(xrDestroyInstance) },
    { "xrGetInstanceProperties",                    reinterpret_cast<PFN_xrVoidFunction>(xrGetInstanceProperties) },
    { "xrPollEvent",                                reinterpret_cast<PFN_xrVoidFunction>(xrPollEvent) },
    { "xrResultToString",                           reinterpret_cast<PFN_xrVoidFunction>(xrResultToString) },
    { "xrStructureTypeToString",                    reinterpret_cast<PFN_xrVoidFunction>(xrStructureTypeToString) },
    { "xrGetSystem",                                reinterpret_cast<PFN_xrVoidFunction>(xrGetSystem) },
    { "xrGetSystemProperties",                      reinterpret_cast<PFN_xrVoidFunction>(xrGetSystemProperties) },
    { "xrEnumerateEnvironmentBlendModes",           reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateEnvironmentBlendModes) },
    { "xrCreateSession",                            reinterpret_cast<PFN_xrVoidFunction>(xrCreateSession) },
    { "xrDestroySession",                           reinterpret_cast<PFN_xrVoidFunction>(xrDestroySession) },
    { "xrEnumerateReferenceSpaces",                 reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateReferenceSpaces) },
    { "xrCreateReferenceSpace",                     reinterpret_cast<PFN_xrVoidFunction>(xrCreateReferenceSpace) },
    { "xrGetReferenceSpaceBoundsRect",              reinterpret_cast<PFN_xrVoidFunction>(xrGetReferenceSpaceBoundsRect) },
    { "xrCreateActionSpace",                        reinterpret_cast<PFN_xrVoidFunction>(xrCreateActionSpace) },
    { "xrLocateSpace",                              reinterpret_cast<PFN_xrVoidFunction>(xrLocateSpace) },
    { "xrDestroySpace",                             reinterpret_cast<PFN_xrVoidFunction>(xrDestroySpace) },
    { "xrEnumerateViewConfigurations",              reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateViewConfigurations) },
    { "xrGetViewConfigurationProperties",           reinterpret_cast<PFN_xrVoidFunction>(xrGetViewConfigurationProperties) },
    { "xrEnumerateViewConfigurationViews",          reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateViewConfigurationViews) },
    { "xrEnumerateSwapchainFormats",                reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateSwapchainFormats) },
    { "xrCreateSwapchain",                          reinterpret_cast<PFN_xrVoidFunction>(xrCreateSwapchain) },
    { "xrDestroySwapchain",                         reinterpret_cast<PFN_xrVoidFunction>(xrDestroySwapchain) },
    { "xrEnumerateSwapchainImages",                 reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateSwapchainImages) },
    { "xrAcquireSwapchainImage",                    reinterpret_cast<PFN_xrVoidFunction>(xrAcquireSwapchainImage) },
    { "xrWaitSwapchainImage",                       reinterpret_cast<PFN_xrVoidFunction>(xrWaitSwapchainImage) },
    { "xrReleaseSwapchainImage",                    reinterpret_cast<PFN_xrVoidFunction>(xrReleaseSwapchainImage) },
    { "xrBeginSession",                             reinterpret_cast<PFN_xrVoidFunction>(xrBeginSession) },
    { "xrEndSession",                               reinterpret_cast<PFN_xrVoidFunction>(xrEndSession) },
    { "xrRequestExitSession",                       reinterpret_cast<PFN_xrVoidFunction>(xrRequestExitSession) },
    { "xrWaitFrame",                                reinterpret_cast<PFN_xrVoidFunction>(xrWaitFrame) },
    { "xrBeginFrame",                               reinterpret_cast<PFN_xrVoidFunction>(xrBeginFrame) },
    { "xrEndFrame",                                 reinterpret_cast<PFN_xrVoidFunction>(xrEndFrame) },
    { "xrLocateViews",                              reinterpret_cast<PFN_xrVoidFunction>(xrLocateViews) },
    { "xrStringToPath",                             reinterpret_cast<PFN_xrVoidFunction>(xrStringToPath) },
    { "xrPathToString",                             reinterpret_cast<PFN_xrVoidFunction>(xrPathToString) },
    { "xrCreateActionSet",                          reinterpret_cast<PFN_xrVoidFunction>(xrCreateActionSet) },
    { "xrDestroyActionSet",                         reinterpret_cast<PFN_xrVoidFunction>(xrDestroyActionSet) },
    { "xrCreateAction",                             reinterpret_cast<PFN_xrVoidFunction>(xrCreateAction) },
    { "xrDestroyAction",                            reinterpret_cast<PFN_xrVoidFunction>(xrDestroyAction) },
    { "xrSuggestInteractionProfileBindings",        reinterpret_cast<PFN_xrVoidFunction>(xrSuggestInteractionProfileBindings) },
    { "xrAttachSessionActionSets",                  reinterpret_cast<PFN_xrVoidFunction>(xrAttachSessionActionSets) },
    { "xrGetCurrentInteractionProfile",             reinterpret_cast<PFN_xrVoidFunction>(xrGetCurrentInteractionProfile) },
    { "xrGetActionStateBoolean",                    reinterpret_cast<PFN_xrVoidFunction>(xrGetActionStateBoolean) },
    { "xrGetActionStateFloat",                      reinterpret_cast<PFN_xrVoidFunction>(xrGetActionStateFloat) },
    { "xrGetActionStateVector2f",                   reinterpret_cast<PFN_xrVoidFunction>(xrGetActionStateVector2f) },
    { "xrGetActionStatePose",                       reinterpret_cast<PFN_xrVoidFunction>(xrGetActionStatePose) },
    { "xrSyncActions",                              reinterpret_cast<PFN_xrVoidFunction>(xrSyncActions) },
    { "xrEnumerateBoundSourcesForAction",           reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateBoundSourcesForAction) },
    { "xrGetInputSourceLocalizedName",              reinterpret_cast<PFN_xrVoidFunction>(xrGetInputSourceLocalizedName) },
    { "xrApplyHapticFeedback",                      reinterpret_cast<PFN_xrVoidFunction>(xrApplyHapticFeedback) },
    { "xrStopHapticFeedback",                       reinterpret_cast<PFN_xrVoidFunction>(xrStopHapticFeedback) },
    { "xrConvertWin32PerformanceCounterToTimeKHR",  reinterpret_cast<PFN_xrVoidFunction>(xrConvertWin32PerformanceCounterToTimeKHR) },
    { "xrConvertTimeToWin32PerformanceCounterKHR",  reinterpret_cast<PFN_xrVoidFunction>(xrConvertTimeToWin32PerformanceCounterKHR) },

        // Graphics extensions
    { "xrGetD3D11GraphicsRequirementsKHR",          reinterpret_cast<PFN_xrVoidFunction>(xrGetD3D11GraphicsRequirementsKHR) },
    { "xrGetD3D12GraphicsRequirementsKHR",          reinterpret_cast<PFN_xrVoidFunction>(xrGetD3D12GraphicsRequirementsKHR) }
};

const std::vector<XrExtensionProperties> supported_extensions{
    // Microsoft Windows extensions
    { XR_TYPE_EXTENSION_PROPERTIES, nullptr, XR_EXT_WIN32_APPCONTAINER_COMPATIBLE_EXTENSION_NAME, XR_EXT_win32_appcontainer_compatible_SPEC_VERSION },
    { XR_TYPE_EXTENSION_PROPERTIES, nullptr, XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME, XR_KHR_win32_convert_performance_counter_time_SPEC_VERSION },
    { XR_TYPE_EXTENSION_PROPERTIES, nullptr, XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME, XR_KHR_composition_layer_depth_SPEC_VERSION },

        // Graphics Extensions
    { XR_TYPE_EXTENSION_PROPERTIES, nullptr, XR_KHR_D3D11_ENABLE_EXTENSION_NAME, XR_KHR_D3D11_enable_SPEC_VERSION },
    { XR_TYPE_EXTENSION_PROPERTIES, nullptr, XR_KHR_D3D12_ENABLE_EXTENSION_NAME, XR_KHR_D3D12_enable_SPEC_VERSION },
        //{ XR_TYPE_EXTENSION_PROPERTIES, nullptr, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME, XR_KHR_vulkan_enable2_SPEC_VERSION }
};
