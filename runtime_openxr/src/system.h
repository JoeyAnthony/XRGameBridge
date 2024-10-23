#pragma once
#include <string>
#include <glm/glm.hpp>

#include "openxr_includes.h"
#include "platform_manager.h"

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

namespace  XRGameBridge {
    // System dummy values
    enum class GraphicsBackend {
        undefined = 0,
        D3D11 = 1,
        D3D12 = 2,
        Vulkan = 3,
        OpenGL = 4
    };

    enum class SRDisplay {
        SR_DISPLAY
    };

    class GB_System {
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

        SR::Screen* sr_screen;
        SR::SwitchableLensHint* lens_hint;

        // Head params
        glm::vec3 head_position;
        glm::vec3 head_direction;
        float interpupillary_distance_cm = 6.2f; // Also known as interaxial

        // Screen params
        glm::vec2 physical_screen_resolution;
        float physical_screen_width = 69;
        float physical_screen_height = 39;
        float ppi;

        void GetHeadPosition();

        // Max possible separation on the screen
        float GetSeparation(float pupil_distance) {
            // Normalized interaxial
            float val = interpupillary_distance_cm / physical_screen_width;
            float separation = glm::clamp(glm::abs(pupil_distance), 0.0f, 10.f);

            if(pupil_distance < 0.0f) {
                return separation * -1.0f;
            }
            return separation;
        }

        XrFovf GetConvergingFov(glm::vec3 eye_position) {
            // Convergence is the distance to the physical screen.
            // By Calculating the fov 

            float half_width = physical_screen_width / 2;
            float half_height = physical_screen_height / 2;
            float z = glm::max(eye_position.z, 0.1f);
            return XrFovf {
                -(half_width - eye_position.x)  / z,    //Left
                 (half_width - eye_position.x)  / z,    //Right
                 (half_height - eye_position.y) / z,    //Up
                -(half_height - eye_position.y) / z,    //Down
            };
        }

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
}
