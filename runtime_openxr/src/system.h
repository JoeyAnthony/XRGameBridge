#pragma once
#include <string>
#include <glm/glm.hpp>
#include <glm/ext/scalar_constants.hpp>

#include "openxr_includes.h"
#include "platform_manager.h"
#include "types.h"

// System
XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId);
XrResult xrGetSystemProperties(XrInstance instance, XrSystemId systemId, XrSystemProperties* properties);
XrResult xrEnumerateEnvironmentBlendModes(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t environmentBlendModeCapacityInput, uint32_t* environmentBlendModeCountOutput, XrEnvironmentBlendMode* environmentBlendModes);

// View configurations
XrResult xrEnumerateViewConfigurations(XrInstance instance, XrSystemId systemId, uint32_t viewConfigurationTypeCapacityInput, uint32_t* viewConfigurationTypeCountOutput, XrViewConfigurationType* viewConfigurationTypes);
XrResult xrGetViewConfigurationProperties(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, XrViewConfigurationProperties* configurationProperties);
XrResult xrEnumerateViewConfigurationViews(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrViewConfigurationView* views);

XrResult xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views);

// Spaces
XrResult xrEnumerateReferenceSpaces(XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces);

// Returns a newly create reference space or the space that already exists
XrResult xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space);
XrResult xrGetReferenceSpaceBoundsRect(XrSession session, XrReferenceSpaceType referenceSpaceType, XrExtent2Df* bounds);
XrResult xrCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space);
XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location);
XrResult xrDestroySpace(XrSpace space);

// Misc
XrResult xrConvertWin32PerformanceCounterToTimeKHR(XrInstance instance, const LARGE_INTEGER* performanceCounter, XrTime* time);
XrResult xrConvertTimeToWin32PerformanceCounterKHR(XrInstance instance, XrTime time, LARGE_INTEGER* performanceCounter);

enum class SRDisplay {
    SR_DISPLAY
};

class GB_System {
    // TODO make members private
public:
    XrInstance instance;
    XrSystemId id;
    std::array<XrFormFactor, 2> supported_formfactors;
    XrFormFactor form_factor;
    SRDisplay sr_device;
    D3D_FEATURE_LEVEL feature_level;
    bool features_enumerated = false;
    GraphicsBackend active_graphics_backend;
    GBVector2i physical_resolution;
    bool device_is_connected = false;

    SR::Display* sr_display;
    SR::SwitchableLensHint* lens_hint;

    // Head params
    glm::vec3 head_position;
    glm::vec3 head_direction;
    float interpupillary_distance_m = 0.062f;

    // Screen params
    glm::vec2 physical_screen_resolution;
    float physical_screen_width_m = 0.69f;
    float physical_screen_height_m = 0.3880f;
    float ppi;

    void GetHeadPosition();

    // Clamps the separation
    // pupil distance in meters
    float GetSeparation(float pupil_distance) {
        // Normalized interaxial
        pupil_distance = glm::clamp(glm::abs(pupil_distance), 0.0f, interpupillary_distance_m);

        float val = pupil_distance / physical_screen_width_m;
        float separation = glm::clamp(glm::abs(val), 0.0f, 1.f);

        if (pupil_distance < 0.0f) {
            return separation * -1.0f;
        }
        return separation;
    }

    // Eye positions relative to the center of the screen in meters
    XrFovf GetConvergingFov(const glm::vec3& eye_position) {
        static glm::vec3 old_position = { 0.0f, 0.0f, 0.30f };

        float half_width = physical_screen_width_m / 2;
        float half_height = physical_screen_height_m / 2;

        float z = glm::clamp(eye_position.z, 0.001f, 5.0f); // where to check this and restore valid values?
        float half_pi = glm::pi<float>() / 2;

        float z_scale = half_width / half_height;

        auto fov = XrFovf{
            glm::clamp(glm::atan(-(half_width + eye_position.x) / z), -half_pi, half_pi),    //Left
            glm::clamp(glm::atan((half_width - eye_position.x) / z), -half_pi, half_pi),    //Right
            glm::clamp(glm::atan((half_height - eye_position.y) / z), -half_pi, half_pi),    //Up
            glm::clamp(glm::atan(-(half_height + eye_position.y) / z), -half_pi, half_pi)    //Down
        };

        // Make sure the view can't be vertically or horizontally flipped. Also the depth is larger than 0.
        if (fov.angleLeft > fov.angleRight || fov.angleDown > fov.angleUp || eye_position.z < 0.001f) {
            // Set to last accepted angles
            //eye_position = old_position;
            return GetConvergingFov(old_position);
        }

        old_position = eye_position;

        return fov;
    }

    /*
     * Returns whether this device is a connected SR display
     */
    bool GetIsConnected();

    /*
     * Extra notes
     * When the screen is closer ro the user, most users cannot handle more than 50% of the real eye separation.
     */
};

// Spaces are basically transformation matrices.
// They transform a point/orientation with respect to an XrSpace of the applications choosing
struct GB_ReferenceSpace {
    XrSession session;
    XrSpace handle;
    XrReferenceSpaceType space_type;
    XrPosef pose_in_reference_space;
};

struct GB_ActionSpace {
    XrSession session;
    XrSpace handle;
    XrAction action;
    XrPath sub_action_path;
    XrPosef pose_in_action_space;
};

//GBVector2i GetDummyScreenResolution();

//XrSystemProperties GetDummySystemProperties();

/*
* Create Systems per instance based on what the SR context returns
*/
XrSystemId CreateXrGameBridgeSystems(XrInstance instance);
GBVector2i GetRenderResolution(const GB_System& gb_system);
GBVector2i GetSystemResolution(const GB_System& gb_system);
GBVector2i GetResolutionMainDisplay();
XrSystemProperties GetSystemProperties(const GB_System& gb_system);
