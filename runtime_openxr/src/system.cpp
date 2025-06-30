#include "system.h"

#include <array>
#include <complex>

#include "easylogging++.h"
#include "openxr_includes.h"
#include "instance.h"
#include "session.h"

XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) {
    TraceLogFunctionCall(__func__, __LINE__);

    // Check if the requested form factor is supported
    bool found = false;
    bool available = false;
    for (auto it = g_systems.begin(); it != g_systems.end(); it++) {
        found = std::find(it->second.supported_formfactors.begin(), it->second.supported_formfactors.end(), getInfo->formFactor) != it->second.supported_formfactors.end();
        if (found) {
            *systemId = it->second.id;
            it->second.form_factor = getInfo->formFactor;

            if (it->second.sr_display != nullptr) {
                available = true;
            }
            break;
        }
    }

    if (!found) {
        return XR_ERROR_FORM_FACTOR_UNSUPPORTED;
    }

    if (!available) {
        return XR_ERROR_FORM_FACTOR_UNAVAILABLE;
    }

    return XR_SUCCESS;
}

XrResult xrGetSystemProperties(XrInstance instance, XrSystemId systemId, XrSystemProperties* properties) {
    TraceLogFunctionCall(__func__, __LINE__);

    GB_System& gb_system = g_systems[systemId];
    *properties = GetSystemProperties(gb_system);

    return XR_SUCCESS;
}

XrResult xrEnumerateEnvironmentBlendModes(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t environmentBlendModeCapacityInput, uint32_t* environmentBlendModeCountOutput, XrEnvironmentBlendMode* environmentBlendModes) {
    TraceLogFunctionCall(__func__, __LINE__);

    LOG(INFO) << "Requested view configuration type: " << viewConfigurationType;
    // SR only supports XR_ENVIRONMENT_BLEND_MODE_OPAQUE 
    const std::array supported_blend_modes = { XR_ENVIRONMENT_BLEND_MODE_OPAQUE };
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

    // TODO check if mono as primary is ok
    auto set = GB_System::GetViewConfigurationTypes();
    const std::vector <XrViewConfigurationType> supported_view_configurations = std::vector(set.begin(), set.end());
    *viewConfigurationTypeCountOutput = supported_view_configurations.size();

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
        // Fill array
        memcpy_s(viewConfigurationTypes, viewConfigurationTypeCapacityInput * sizeof(XrViewConfigurationType), supported_view_configurations.data(), supported_view_configurations.size() * sizeof(XrViewConfigurationType));
        return XR_SUCCESS;
    }
}

XrResult xrGetViewConfigurationProperties(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, XrViewConfigurationProperties* configurationProperties) {
    TraceLogFunctionCall(__func__, __LINE__);

    XrResult res = XR_ERROR_RUNTIME_FAILURE;

    switch (viewConfigurationType) {
    case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
        configurationProperties->viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
        configurationProperties->fovMutable = true;
        res = XR_SUCCESS;
        break;
    case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
        configurationProperties->viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        configurationProperties->fovMutable = true;
        res = XR_SUCCESS;
        break;
    default:
        res = XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
    }

    return res;
}

XrResult xrEnumerateViewConfigurationViews(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrViewConfigurationView* views) {
    TraceLogFunctionCall(__func__, __LINE__);

    XrResult res = XR_ERROR_RUNTIME_FAILURE;

    GB_System gb_system = g_systems[systemId];
    GBVector2i render_resolution = GetRenderResolution(gb_system);
    GBVector2i system_resolution = GetSystemResolution(gb_system);

    std::vector<XrViewConfigurationView> supported_views;
    if (viewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
        XrViewConfigurationView view{};
        view.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
        // recommended is half width, max is full width?
        view.recommendedImageRectWidth = render_resolution.x;
        view.maxImageRectWidth = render_resolution.x;
        view.recommendedImageRectHeight = render_resolution.y;
        view.maxImageRectHeight = render_resolution.y;
        view.recommendedSwapchainSampleCount = 1; //TODO idk what this means
        view.maxSwapchainSampleCount = 1;

        supported_views.push_back(view);
        supported_views.push_back(view);

        res = XR_SUCCESS;
    }
    else if (viewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO) {
        LOG(ERROR) << "Mono view configuration requested. Not suppoerted";
    }
    else {
        res = XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
        LOG_RUNTIME_ERROR
    }

    // Set output count
    *viewCountOutput = supported_views.size();

    // Request for the extension array or the extension array itself
    if (viewCapacityInput == 0) {
        res = XR_SUCCESS;
    }
    // Passed array not large enough
    else if (viewCapacityInput < supported_views.size()) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }
    else {
        memcpy_s(views, viewCapacityInput * sizeof(XrViewConfigurationView), supported_views.data(), supported_views.size() * sizeof(XrViewConfigurationView));
    }

    return res;
}

XrResult xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views) {
    TraceLogFunctionCall(__func__, __LINE__);

    GB_Session& gb_session = g_sessions[session];

    if(viewLocateInfo->viewConfigurationType != gb_session.view_configuration) {
        return XR_ERROR_VALIDATION_FAILURE;
    }

    if (GB_System::GetViewConfigurationTypes().contains(viewLocateInfo->viewConfigurationType) == false) {
        return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
    }

    // TODO mono configuration is not supported
    if (gb_session.view_configuration == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO) {
        *viewCountOutput = gb_session.views.size();
    }
    else if (gb_session.view_configuration == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
        *viewCountOutput = gb_session.views.size();
    }

    // Request for the extension array or the extension array itself
    if (viewCapacityInput == 0) {
        return XR_SUCCESS;
    }
    // Passed array not large enough
    if (viewCapacityInput < gb_session.views.size()) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    glm::mat4 base_transform = g_space_transforms[viewLocateInfo->space];
    std::vector<XrView> sr_views;
    for (uint32_t i = 0; i < gb_session.views.size(); i++) {
        XrPosef pose = gb_session.views[i].pose;
        glm::mat4 view_transform = glm::translate(glm::mat4(1.0f), { pose.position.x, pose.position.y , pose.position.z }) * glm::mat4_cast(glm::quat{pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z });

        // Transform
        glm::mat4 transform = glm::inverse(base_transform) * view_transform;
        glm::vec3 position = glm::vec3(transform[3]);
        glm::quat orientation = glm::quat_cast(transform);

        XrView view;
        view.pose = { { orientation.x, orientation.y, orientation.z, orientation.w }, { position.x, position.y, position.z } };
        view.fov = gb_session.views[i].fov;
        sr_views.push_back(view);
    }

    memcpy_s(views, viewCapacityInput * sizeof(XrView), sr_views.data(), sr_views.size() * sizeof(XrView));

    viewState->viewStateFlags = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;

    return XR_SUCCESS;
}

XrResult xrEnumerateReferenceSpaces(XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces) {
    TraceLogFunctionCall(__func__, __LINE__);

    GB_Session& gb_session = g_sessions[session];

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

        LOG(ERROR) << "ERROR Reference space unsupported: " << createInfo->referenceSpaceType;
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
        pose.position.y -= 1.72f;
    }

    // Create transform
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), { pose.position.x, pose.position.y , pose.position.z }) * glm::mat4_cast(glm::quat{pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z });
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

    if(referenceSpaceType == XR_REFERENCE_SPACE_TYPE_VIEW) {
        bounds->width = 0;
        bounds->height = 0;
        return XR_SPACE_BOUNDS_UNAVAILABLE;
    }
    else if (referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL){
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
        LOG(ERROR) << "ERROR Reference space unsupported: " << referenceSpaceType;
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
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), {pose.position.x, pose.position.y , pose.position.z }) * glm::mat4_cast(glm::quat{pose.orientation.w, pose.orientation.x, pose.orientation.y , pose.orientation.z });

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

    if(g_reference_spaces.contains(space))
    {
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

//GBVector2i GetDummyScreenResolution() {
//    //TODO dependent on the SR screen, hopefully we can set reset this later on runtime. It would be cool to setup everything without having to connect to the sr service since that might take some time.
//    // MS docs: The width/height of the client area for a full-screen window on the primary display monitor, in pixels.
//    const uint32_t primary_display_res_x = static_cast<uint32_t>(GetSystemMetrics(SM_CXSCREEN) / 2); // Divided by 2 since we render in sbs
//    const uint32_t primary_display_res_y = static_cast<uint32_t>(GetSystemMetrics(SM_CYSCREEN));
//    return { primary_display_res_x, primary_display_res_y };
//}
//
//XrSystemProperties GetDummySystemProperties() {
//    auto screen_resolution = GetDummyScreenResolution();
//
//    XrSystemGraphicsProperties g_props{};
//    g_props.maxLayerCount = 1;
//    g_props.maxSwapchainImageWidth = screen_resolution.x;
//    g_props.maxSwapchainImageHeight = screen_resolution.y;
//
//    XrSystemTrackingProperties t_props{};
//    t_props.positionTracking = false;
//    t_props.orientationTracking = false;
//
//    XrSystemProperties sys_props{
//        XR_TYPE_SYSTEM_PROPERTIES,
//        nullptr,
//        1,
//        0x354B, // USB Vendor ID
//        "SR Monitor",
//        g_props,
//        t_props
//    };
//    return sys_props;
//}

bool GB_System::GetIsConnected() {
    return device_is_connected;
}

std::set<XrViewConfigurationType> GB_System::GetViewConfigurationTypes() {
    return { /**XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO,**/ XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO };
}

XrSystemId CreateXrGameBridgeSystems(XrInstance instance)
{
    GB_Instance* gb_instance = reinterpret_cast<GB_Instance*>(instance);

    // Create system
    GB_System system;
    system.id = g_systems.size() + 1; // 0 is NULL_SYSTEM_HANDLE
    system.instance = instance;
    system.supported_formfactors = { XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY, XR_FORM_FACTOR_HANDHELD_DISPLAY };
    system.sr_device = SRDisplay::SR_DISPLAY;
    system.sr_display = gb_instance->GetPlatformManager()->GetDisplay();
    system.lens_hint = gb_instance->GetPlatformManager()->GetLensHint();
    system.physical_resolution = GBVector2i{ static_cast<uint64_t>(system.sr_display->getPhysicalResolutionWidth()), static_cast<uint64_t>(system.sr_display->getPhysicalResolutionHeight()) };

    system.physical_screen_width_m = system.sr_display->getPhysicalSizeWidth() / 100.f;
    system.physical_screen_height_m = system.sr_display->getPhysicalSizeHeight() / 100.f;

    // Check if an sr display is connected.
    // This is done by checking if the virtual display coordinates of the screen are all 0 or not.
    auto display_coordinates = system.sr_display->getLocation();
    if( display_coordinates.left == 0 &&
        display_coordinates.bottom == 0 &&
        display_coordinates.right == 0 &&
        display_coordinates.top == 0)
    {
        // For when no SR display is connected, and if it's an 8K SR display it should work as well
        system.device_is_connected = false;
        system.physical_resolution = GetResolutionMainDisplay();
    }
    else {
        system.device_is_connected = true;
    }

    g_systems.insert({ system.id, system });

    LOG(INFO) << "Created system: " << system.id;
    return system.id;
}

GBVector2i GetRenderResolution(const GB_System& gb_system) {
    GBVector2i physical_res = gb_system.physical_resolution;
    auto form_factor = gb_system.form_factor;
    bool use_halved_width = form_factor == XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY || form_factor == XR_FORM_FACTOR_HANDHELD_DISPLAY;

    if (use_halved_width) {
        physical_res.x /= 2;
    }

    return physical_res;
}

GBVector2i GetSystemResolution(const GB_System& gb_system) {
    return gb_system.physical_resolution;
}

GBVector2i GetResolutionMainDisplay() {
    size_t width = GetSystemMetrics(SM_CXSCREEN);
    size_t height = GetSystemMetrics(SM_CYSCREEN);
    return GBVector2i{ static_cast<uint32_t>(width) ,static_cast<uint32_t>(height) };
}

XrSystemProperties GetSystemProperties(const GB_System& gb_system) {
    GBVector2i native_resolution = GetRenderResolution(gb_system);

    XrSystemGraphicsProperties g_props{};
    g_props.maxLayerCount = XR_MIN_COMPOSITION_LAYERS_SUPPORTED;
    g_props.maxSwapchainImageWidth = native_resolution.x;
    g_props.maxSwapchainImageHeight = native_resolution.y;

    XrSystemTrackingProperties t_props{};
    t_props.positionTracking = false;
    t_props.orientationTracking = false;

    XrSystemProperties sys_props{
        XR_TYPE_SYSTEM_PROPERTIES,
        nullptr,
        gb_system.id,
        0x354B, // USB Vendor ID
        "SR Monitor",
        g_props,
        t_props
    };
    return sys_props;
}
