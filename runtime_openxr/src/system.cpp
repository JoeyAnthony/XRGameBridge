#include "system.h"

#include <array>
#include <complex>

#include "easylogging++.h"
#include "openxr_includes.h"
#include "instance.h"
#include "session.h"

XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) {
    // Check if the requested form factor is supported
    bool found = false;
    bool available = false;
    for (auto it = XRGameBridge::g_systems.begin(); it != XRGameBridge::g_systems.end(); it++) {
        found = std::find(it->second.supported_formfactors.begin(), it->second.supported_formfactors.end(), getInfo->formFactor) != it->second.supported_formfactors.end();
        if (found) {
            *systemId = it->second.id;
            it->second.form_factor = getInfo->formFactor;

            if (it->second.sr_screen != nullptr) {
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

    XRGameBridge::GB_System& gb_system = XRGameBridge::g_systems[systemId];
    *properties = XRGameBridge::GetSystemProperties(gb_system);

    return XR_SUCCESS;
}

XrResult xrEnumerateEnvironmentBlendModes(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t environmentBlendModeCapacityInput, uint32_t* environmentBlendModeCountOutput, XrEnvironmentBlendMode* environmentBlendModes) {
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
    // TODO check if mono as primary is ok
    const std::array supported_view_configurations = { XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO };
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
    XrResult res = XR_ERROR_RUNTIME_FAILURE;

    switch (viewConfigurationType) {
    case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
        configurationProperties->viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
        configurationProperties->fovMutable = true; // TODO check if it's ok if the application changes the fov
        res = XR_SUCCESS;
        break;
    case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
        configurationProperties->viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        configurationProperties->fovMutable = true; // TODO check if it's ok if the application changes the fov
        res = XR_SUCCESS;
        break;
    default:;
        res = XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
    }

    return res;
}

XrResult xrEnumerateViewConfigurationViews(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrViewConfigurationView* views) {
    XrResult res = XR_ERROR_RUNTIME_FAILURE;

    XRGameBridge::GB_System gb_system = XRGameBridge::g_systems[systemId];
    XRGameBridge::GBVector2i form_factor_resolution = GetSystemResolution(gb_system, gb_system.form_factor);
    XRGameBridge::GBVector2i native_resolution = GetNativeSystemResolution(gb_system);

    std::vector<XrViewConfigurationView> supported_views;
    if (viewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
        XrViewConfigurationView view{};
        view.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
        // recommended is half width, max is full width?
        view.recommendedImageRectWidth = form_factor_resolution.x;
        view.maxImageRectWidth = native_resolution.x;
        view.recommendedImageRectHeight = form_factor_resolution.y;
        view.maxImageRectHeight = native_resolution.y;
        view.recommendedSwapchainSampleCount = 2; //TODO idk what this means
        view.maxSwapchainSampleCount = 2;

        // TODO Create 2 views here to get 2 swap chains and so a view per eye
        supported_views.push_back(view);
        supported_views.push_back(view);

        res = XR_SUCCESS;
    }
    else if (viewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO) {

    }
    else {
        res = XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
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

constexpr auto M_PI = 3.14159265358979323846;
XrResult xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views) {
    // TODO Dummy implementation for locate views, only returning views with a hardcoded offset hoping these are the eye locations

    float fov = M_PI / 3.5f;

    XrView view1, view2;
    view1.type = XR_TYPE_VIEW;
    view1.next = nullptr;
    view1.pose = { {.0f, 0.0f, 0.0f, 1.0f}, {-0.060f, 0, 0} }; // Orientation, Position
    view1.fov = { -fov, fov, fov, -fov }; // FOV angle left, right, up, down

    view2.type = XR_TYPE_VIEW;
    view2.next = nullptr;
    view2.pose = { {0.0f, 0.0f, 0.0f, 1.0f}, {0.060f, 0, 0} }; // Orientation, Position
    view2.fov = { -fov, fov, fov, -fov }; // FOV angle left, right, up, down

    std::vector<XrView> sr_views;

    //TODO Don't understand this, for some reason it wants a single view for stereo output.
    // Should change this later
    if (viewLocateInfo->viewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO) {
        sr_views = { view1 };
    }
    else if (viewLocateInfo->viewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
        sr_views = { view1, view2 };
    }

    *viewCountOutput = sr_views.size();

    // Request for the extension array or the extension array itself
    if (viewCapacityInput == 0) {
        return XR_SUCCESS;
    }
    // Passed array not large enough
    if (viewCapacityInput < sr_views.size()) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    viewLocateInfo->displayTime;

    XRGameBridge::GB_ReferenceSpace& gb_ref_space = XRGameBridge::g_reference_spaces[viewLocateInfo->space];
    if (gb_ref_space.space_type == XR_REFERENCE_SPACE_TYPE_VIEW) // Camera space
    {
        gb_ref_space.pose_in_reference_space.position;
    }
    if (gb_ref_space.space_type == XR_REFERENCE_SPACE_TYPE_LOCAL) { // World space
        //view1.pose.position += gb_ref_space.pose_in_reference_space.position;
        //LOG(INFO) << "World space not implemented: " << __func__;
    }

    viewState->viewStateFlags = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;

    memcpy_s(views, viewCapacityInput * sizeof(XrView), sr_views.data(), sr_views.size() * sizeof(XrView));

    return XR_SUCCESS;
}

XrResult xrEnumerateReferenceSpaces(XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces) {
    XRGameBridge::GB_Session* gb_session = reinterpret_cast<XRGameBridge::GB_Session*>(session);

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

#include "openxr_functions.h"
XrResult xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space) {
    static uint64_t reference_space_count = 1;
    XrSpace handle = reinterpret_cast<XrSpace>(reference_space_count);
    XRGameBridge::GB_ReferenceSpace new_space;
    new_space.session = session;
    new_space.handle = handle;
    new_space.pose_in_reference_space = createInfo->poseInReferenceSpace;
    new_space.space_type = createInfo->referenceSpaceType;

    if (createInfo->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_VIEW &&
        createInfo->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_LOCAL &&
        createInfo->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_STAGE) {
        return XR_ERROR_REFERENCE_SPACE_UNSUPPORTED;
    }

    const auto inserted = XRGameBridge::g_reference_spaces.insert({ handle, new_space });
    if (!inserted.second) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    reference_space_count++;
    *space = handle;

    return XR_SUCCESS;
}

XrResult xrGetReferenceSpaceBoundsRect(XrSession session, XrReferenceSpaceType referenceSpaceType, XrExtent2Df* bounds) {
    LOG(INFO) << "Called " << __func__; return XR_ERROR_RUNTIME_FAILURE;
}

XrResult xrCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space) {
    XRGameBridge::GB_ActionSpace new_space{};
    new_space.session = session;
    new_space.action = createInfo->action;
    new_space.sub_action_path = createInfo->subactionPath;
    new_space.pose_in_action_space = createInfo->poseInActionSpace;

    // Add action handle to sub action handle for a space handle hash
    XrSpace handle = reinterpret_cast<XrSpace>(reinterpret_cast<uint64_t>(new_space.action) + createInfo->subactionPath);
    *space = handle;
    XRGameBridge::g_action_spaces.insert({ handle, new_space });
    return XR_SUCCESS;
}

XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location) {
    // TODO Return to this with a better understanding of spaces. Are there only single View and Local spaces, or do more of them exist?
    // TODO Should we have both action and reference spaces in a single array for quicker lookup?


    // TODO check location flags
    location->locationFlags;

    // TODO Application may ask for a velocity of the tracked object
    if (location->next != nullptr) {
        XrSpaceVelocity* velocity = static_cast<XrSpaceVelocity*>(location->next);
        velocity->velocityFlags;
    }

    // For Reference spaces
    XRGameBridge::GB_ReferenceSpace& gb_space = XRGameBridge::g_reference_spaces[space];
    XRGameBridge::GB_ReferenceSpace& gb_base_space = XRGameBridge::g_reference_spaces[baseSpace];
    if (gb_space.session != nullptr) {
        // TODO, Transform to base space? just returning it for now, in the test the local space is 0 anyways
        // Telling the application the view position is valid but never being tracked
        location->pose = gb_space.pose_in_reference_space;
        location->locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;

        return XR_SUCCESS;
    }

    // For Action spaces

    // Retreiving action spaces is not supported
    XRGameBridge::GB_ActionSpace& gb_a_space = XRGameBridge::g_action_spaces[space];
    XRGameBridge::GB_ActionSpace& gb_a_base_space = XRGameBridge::g_action_spaces[baseSpace];
    if (gb_a_space.session == nullptr) {
        LOG(INFO) << "Space does not exist";
    }

    std::string& path = XRGameBridge::g_xrpath_storage[gb_a_space.sub_action_path];

    // TODO Actions are never located now, might be what we want anyways
    // Application is told the actions are never being tracked this way
    location->pose = XrPosef{ 0.f };
    location->locationFlags = 0;
    return XR_SUCCESS;
}

XrResult xrDestroySpace(XrSpace space) {
    // Todo If the space is not a reference space, unorderedmap::[] creates an entry for it. This may not be desired but won't do any harm.
    // Check for reference space
     XRGameBridge::GB_ReferenceSpace& gb_reference_space = XRGameBridge::g_reference_spaces[space];
    if(gb_reference_space.session != nullptr)
    {
        XRGameBridge::g_reference_spaces.erase(space);
        return XR_SUCCESS;
    }

    // Check for action space
    XRGameBridge::GB_ActionSpace& gb_action_space = XRGameBridge::g_action_spaces[space];
    if (gb_action_space.session != nullptr) {
        XRGameBridge::g_action_spaces.erase(space);
        return XR_SUCCESS;
    }

    return XR_ERROR_HANDLE_INVALID;
}

//XRGameBridge::GBVector2i XRGameBridge::GetDummyScreenResolution() {
//    //TODO dependent on the SR screen, hopefully we can set reset this later on runtime. It would be cool to setup everything without having to connect to the sr service since that might take some time.
//    // MS docs: The width/height of the client area for a full-screen window on the primary display monitor, in pixels.
//    const uint32_t primary_display_res_x = static_cast<uint32_t>(GetSystemMetrics(SM_CXSCREEN) / 2); // Divided by 2 since we render in sbs
//    const uint32_t primary_display_res_y = static_cast<uint32_t>(GetSystemMetrics(SM_CYSCREEN));
//    return { primary_display_res_x, primary_display_res_y };
//}
//
//XrSystemProperties XRGameBridge::GetDummySystemProperties() {
//    auto screen_resolution = XRGameBridge::GetDummyScreenResolution();
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

XrSystemId XRGameBridge::CreateXrGameBridgeSystem(XrInstance instance)
{
    GB_Instance* gb_instance = reinterpret_cast<GB_Instance*>(instance);

    // Create system
    GB_System system;
    system.id = g_systems.size() + 1; // 0 is NULL_SYSTEM_HANDLE
    system.instance = instance;
    system.supported_formfactors = { XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY, XR_FORM_FACTOR_HANDHELD_DISPLAY };
    system.sr_device = XRGameBridge::SRDisplay::SR_DISPLAY;
    system.sr_screen = SR::Screen::create(*gb_instance->sr_context);
    system.lens_hint = SR::SwitchableLensHint::create(*gb_instance->sr_context);
    system.physical_resolution = GBVector2i{ static_cast<uint64_t>(system.sr_screen->getPhysicalResolutionWidth()), static_cast<uint64_t>(system.sr_screen->getPhysicalResolutionHeight()) };

    g_systems.insert({ system.id, system });

    return system.id;
}

XRGameBridge::GBVector2i XRGameBridge::GetSystemResolution(const GB_System& gb_system, XrFormFactor form_factor) {
    bool use_halved_width = form_factor == XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY || form_factor == XR_FORM_FACTOR_HANDHELD_DISPLAY;

    GBVector2i physical_res = gb_system.physical_resolution;
    if (use_halved_width) {
        //physical_res.x /= 2;
    }

    return physical_res;
}

XRGameBridge::GBVector2i XRGameBridge::GetNativeSystemResolution(const GB_System& gb_system) {
    return gb_system.physical_resolution;
}

XRGameBridge::GBVector2i XRGameBridge::GetScaledSystemResolutionMainDisplay() {
    size_t width = GetSystemMetrics(SM_CXSCREEN);
    size_t height = GetSystemMetrics(SM_CYSCREEN);
    return GBVector2i{ static_cast<uint32_t>(width) ,static_cast<uint32_t>(height) };
}

XrSystemProperties XRGameBridge::GetSystemProperties(const GB_System& gb_system) {
    GBVector2i native_resolution = GetNativeSystemResolution(gb_system);

    XrSystemGraphicsProperties g_props{};
    g_props.maxLayerCount = 1;
    g_props.maxSwapchainImageWidth = native_resolution.x;
    g_props.maxSwapchainImageHeight = native_resolution.y;

    XrSystemTrackingProperties t_props{};
    t_props.positionTracking = false;
    t_props.orientationTracking = false;

    XrSystemProperties sys_props{
        XR_TYPE_SYSTEM_PROPERTIES,
        nullptr,
        1,
        0x354B, // USB Vendor ID
        "SR Monitor",
        g_props,
        t_props
    };
    return sys_props;
}
