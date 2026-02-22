/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "system.h"

#include <array>

#include "debug.h"
#include "instance.h"

XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) {
    TraceLogFunctionCall(__func__, __LINE__);

    // Check if the requested form factor is supported
    bool found = false;
    bool available = false;
    for (auto it = g_systems.begin(); it != g_systems.end(); it++) {
        if (it->second->GetSupportedFormFactors().contains(getInfo->formFactor)) {
            found = true;
            *systemId = it->second->GetId();

            if (it->second != nullptr && it->second->IsAvailable()) {
                available = true;
            }
            break;
        }
    }

    if (!found) {
        return XR_ERROR_FORM_FACTOR_UNSUPPORTED;
    }

    if (!available) {
        return XR_ERROR_HANDLE_INVALID;
    }

    return XR_SUCCESS;
}

XrResult xrGetSystemProperties(XrInstance instance, XrSystemId systemId, XrSystemProperties* properties) {
    TraceLogFunctionCall(__func__, __LINE__);

    const auto gb_system = g_systems[systemId];
    if(gb_system == nullptr) {
        return XR_ERROR_HANDLE_INVALID;
    }
    *properties = gb_system->GetSystemProperties();

    return XR_SUCCESS;
}

XrResult xrEnumerateEnvironmentBlendModes(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t environmentBlendModeCapacityInput, uint32_t* environmentBlendModeCountOutput, XrEnvironmentBlendMode* environmentBlendModes) {
    TraceLogFunctionCall(__func__, __LINE__);

    const auto gb_system = g_systems[systemId];
    if (gb_system == nullptr) {
        return XR_ERROR_HANDLE_INVALID;
    }

    spdlog::info("Requested view configuration type: {}", static_cast<uint32_t>(viewConfigurationType));
    const auto supported_blend_modes = gb_system->GetEnvironmentBlendModes();
    *environmentBlendModeCountOutput = supported_blend_modes.size();

    // Request for the extension array or the extension array itself
    if (environmentBlendModeCapacityInput == 0) {
        return XR_SUCCESS;
    }
    // Passed array not large enough
    if (environmentBlendModeCapacityInput < 1) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }
    // Return whether the extension exists

    // Fill array
    memcpy_s(environmentBlendModes, environmentBlendModeCapacityInput * sizeof(XrEnvironmentBlendMode), supported_blend_modes.data(), supported_blend_modes.size() * sizeof(XrEnvironmentBlendMode));
    return XR_SUCCESS;
}

XrResult xrEnumerateViewConfigurations(XrInstance instance, XrSystemId systemId, uint32_t viewConfigurationTypeCapacityInput, uint32_t* viewConfigurationTypeCountOutput, XrViewConfigurationType* viewConfigurationTypes) {
    TraceLogFunctionCall(__func__, __LINE__);

    const auto gb_system = g_systems[systemId];
    if (gb_system == nullptr) {
        return XR_ERROR_HANDLE_INVALID;
    }
    const auto properties = gb_system->GetViewConfigurationProperties();
    *viewConfigurationTypeCountOutput = properties.size();

    // Request for the extension array or the extension array itself
    if (viewConfigurationTypeCapacityInput == 0) {
        return XR_SUCCESS;
    }
    // Passed array not large enough
    else if (viewConfigurationTypeCapacityInput < 1) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }
    // Return whether the extension exists
    else {
        // Get view config types from properties
        std::ranges::transform(properties, viewConfigurationTypes, [](const auto& prop) {
            return prop.viewConfigurationType;
        });

        // Fill array
        return XR_SUCCESS;
    }
}

XrResult xrGetViewConfigurationProperties(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, XrViewConfigurationProperties* configurationProperties) {
    TraceLogFunctionCall(__func__, __LINE__);

    const auto gb_system = g_systems[systemId];
    if(gb_system == nullptr) {
        return XR_ERROR_HANDLE_INVALID;
    }

    XrResult res = XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
    for (const auto& prop : gb_system->GetViewConfigurationProperties()) {
        if (prop.viewConfigurationType == viewConfigurationType) {
            *configurationProperties = prop;
            res = XR_SUCCESS;
            break;
        }
    }
    return res;
}

XrResult xrEnumerateViewConfigurationViews(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrViewConfigurationView* views) {
    TraceLogFunctionCall(__func__, __LINE__);

    const auto gb_system = g_systems[systemId];
    if (gb_system == nullptr) {
        return XR_ERROR_HANDLE_INVALID;
    }

    const auto supported_views = gb_system->GetViewConfigurationViews(viewConfigurationType);
    if (supported_views.size() == 0) {
        LOG_RUNTIME_ERROR;
        return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
    }

    // Set output count
    *viewCountOutput = supported_views.size();

    // Request for the extension array or the extension array itself
    XrResult res;
    if (viewCapacityInput == 0) {
        res = XR_SUCCESS;
    }
    // Passed array not large enough
    else if (viewCapacityInput < supported_views.size()) {
        res = XR_ERROR_SIZE_INSUFFICIENT;
    }
    else {
        memcpy_s(views, viewCapacityInput * sizeof(XrViewConfigurationView), supported_views.data(), supported_views.size() * sizeof(XrViewConfigurationView));
        res = XR_SUCCESS;
    }

    return res;
}

XrResult xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views) {
    TraceLogFunctionCall(__func__, __LINE__);

    XRSession& gb_session = g_sessions[session];
    const auto view_count = gb_session.GetSystem()->GetViewCount();

    if (viewLocateInfo->viewConfigurationType != gb_session.view_configuration) {
        return XR_ERROR_VALIDATION_FAILURE;
    }

    if (gb_session.view_configuration == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO) {
        *viewCountOutput = view_count;
    }
    else if (gb_session.view_configuration == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
        *viewCountOutput = view_count;
    }

    // Request for the extension array or the extension array itself
    if (viewCapacityInput == 0) {
        return XR_SUCCESS;
    }
    // Passed array not large enough
    if (viewCapacityInput < view_count) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    const auto eye_positions = gb_session.GetViewPositions();
    const glm::mat4 base_transform = g_space_transforms[viewLocateInfo->space];
    std::vector<XrView> sr_views;
    for (uint32_t i = 0; i < view_count; i++) {
        XrPosef pose = eye_positions[i].pose;
        glm::mat4 view_transform = glm::translate(glm::mat4(1.0f), { pose.position.x, pose.position.y , pose.position.z }) * glm::mat4_cast(glm::quat{ pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z });

        // Transform
        const glm::mat4 transform = glm::inverse(base_transform) * view_transform;
        const glm::vec3 position = glm::vec3(transform[3]);
        const glm::quat orientation = glm::quat_cast(transform);

        XrView view;
        view.pose = { { orientation.x, orientation.y, orientation.z, orientation.w }, { position.x, position.y, position.z } };
        view.fov = eye_positions[i].fov;
        sr_views.push_back(view);
    }

    memcpy_s(views, viewCapacityInput * sizeof(XrView), sr_views.data(), sr_views.size() * sizeof(XrView));

    viewState->viewStateFlags = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;

    return XR_SUCCESS;
}

XrResult xrEnumerateReferenceSpaces(XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces) {
    TraceLogFunctionCall(__func__, __LINE__);

    XRSession& gb_session = g_sessions[session];

    std::array reference_space_types{
        XR_REFERENCE_SPACE_TYPE_VIEW,
        XR_REFERENCE_SPACE_TYPE_LOCAL
    };

    *spaceCountOutput = reference_space_types.size();

    // Request for the extension array or the extension array itself
    if (spaceCapacityInput == 0) {
        return XR_SUCCESS;
    }
    // Passed array not large enough
    if (spaceCapacityInput < reference_space_types.size()) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    memcpy_s(spaces, spaceCapacityInput * sizeof(XrReferenceSpaceType), reference_space_types.data(), reference_space_types.size() * sizeof(XrReferenceSpaceType));
    return XR_SUCCESS;
}

XrResult xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space) {
    static uint64_t reference_space_count = 1;
    XrSpace handle = reinterpret_cast<XrSpace>(reference_space_count);
    GB_ReferenceSpace new_space {
        createInfo->referenceSpaceType
    };

    if (createInfo->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_VIEW &&
        createInfo->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_LOCAL &&
        createInfo->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_STAGE) {

        spdlog::error("ERROR Reference space unsupported: {}", static_cast<uint32_t>(createInfo->referenceSpaceType));
        return XR_ERROR_REFERENCE_SPACE_UNSUPPORTED;
    }

    // Take space from the app
    XrPosef pose = createInfo->poseInReferenceSpace;

    if (createInfo->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_VIEW) {
        // Update position in session
    }
    else if (createInfo->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL) {
        // Set a hardcoded floor
        pose.position.y -= 1.72f;
    }
    else if (createInfo->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE) {
        // Set a hardcoded floor
        //pose.position.y -= 1.72f;
    }

    // Create transform
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), { pose.position.x, pose.position.y , pose.position.z }) * glm::mat4_cast(glm::quat{ pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z });
    const auto inserted = g_reference_spaces.insert({ handle, new_space });
    g_space_transforms.insert({ handle, transform });

    if (!inserted.second) {
        LOG_RUNTIME_ERROR
        return XR_ERROR_RUNTIME_FAILURE;
    }

    reference_space_count++;
    *space = handle;

    return XR_SUCCESS;
}

XrResult xrGetReferenceSpaceBoundsRect(XrSession session, XrReferenceSpaceType referenceSpaceType, XrExtent2Df* bounds) {
    TraceLogFunctionCall(__func__, __LINE__);

    if (referenceSpaceType == XR_REFERENCE_SPACE_TYPE_VIEW) {
        bounds->width = 0;
        bounds->height = 0;
        return XR_SPACE_BOUNDS_UNAVAILABLE;
    }
    else if (referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL) {
        // Bounds can be defined by the eye tracker bounding box.
        // Current values are hardcoded defaults because the box is different for every screen.
        // TODO get eyetracker box 
        bounds->width = 1.10f;
        bounds->height = 1.10f;
    }
    else if (referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE) {
        bounds->width = 1.10f;
        bounds->height = 1.10f;
    }
    else {
        spdlog::error("ERROR Reference space unsupported: {}", static_cast<uint32_t>(referenceSpaceType));
        return XR_ERROR_REFERENCE_SPACE_UNSUPPORTED;
    }

    return XR_SUCCESS;
}

XrResult xrCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space) {
    TraceLogFunctionCall(__func__, __LINE__);

    GB_ActionSpace new_space{};
    new_space.action = createInfo->action;
    new_space.sub_action_path = createInfo->subactionPath;
    XrPosef pose = createInfo->poseInActionSpace;

    // Create transform
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), { pose.position.x, pose.position.y , pose.position.z }) * glm::mat4_cast(glm::quat{ pose.orientation.w, pose.orientation.x, pose.orientation.y , pose.orientation.z });

    // Add action handle to sub action handle for a space handle hash
    XrSpace handle = reinterpret_cast<XrSpace>(reinterpret_cast<uint64_t>(new_space.action) + createInfo->subactionPath);
    *space = handle;
    g_action_spaces.insert({ handle, new_space });
    g_space_transforms.insert({ handle, transform });
    return XR_SUCCESS;
}

XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location) {
    TraceLogFunctionCall(__func__, __LINE__);

    location->locationFlags;

    // TODO Application may ask for a velocity of the tracked object
    if (location->next != nullptr) {
        XrSpaceVelocity* velocity = static_cast<XrSpaceVelocity*>(location->next);
        velocity->velocityFlags = XR_SPACE_VELOCITY_ANGULAR_VALID_BIT;
    }

    // Get spaces
    glm::mat4 gb_space = g_space_transforms[space];
    glm::mat4 gb_base_space = g_space_transforms[baseSpace];

    // Transform
    glm::mat4 transform = glm::inverse(gb_base_space) * gb_space;
    glm::vec3 position = glm::vec3(transform[3]);
    glm::quat orientation = glm::quat_cast(transform);

    location->pose = { {orientation.x, orientation.y , orientation.z, orientation.w }, {position.x, position.y, position.z} };
    location->locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;

    return XR_SUCCESS;
}

XrResult xrDestroySpace(XrSpace space) {
    TraceLogFunctionCall(__func__, __LINE__);

    if (g_reference_spaces.contains(space)) {
        g_reference_spaces.erase(space);
        return XR_SUCCESS;
    }

    if (g_action_spaces.contains(space)) {
        g_action_spaces.erase(space);
        return XR_SUCCESS;
    }

    return XR_ERROR_HANDLE_INVALID;
}

XrResult xrConvertWin32PerformanceCounterToTimeKHR(XrInstance instance, const LARGE_INTEGER* performanceCounter, XrTime* time) {
    TraceLogFunctionCall(__func__, __LINE__);

    *time = performanceCounter->QuadPart;
    return XR_SUCCESS;
}

XrResult xrConvertTimeToWin32PerformanceCounterKHR(XrInstance instance, XrTime time, LARGE_INTEGER* performanceCounter) {
    TraceLogFunctionCall(__func__, __LINE__);

    performanceCounter->QuadPart = time;
    return XR_SUCCESS;
}