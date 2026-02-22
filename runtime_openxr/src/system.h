/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once
#include "openxr_includes.h"
#include "featuremodule.h"

#include <set>

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

class XRSystem;
inline std::unordered_map<XrSystemId, std::shared_ptr<XRSystem>> g_systems;

enum class XRSystemType { SRSystem };
constexpr uint32_t max_retries = 5;
constexpr uint32_t wait_time_ms = 500;

class XRSystem {
protected:
    const XrSystemId id;
    const XRSystemType type;
    const std::string name;

public:
    XrSystemId GetId() const { return id; };

    // Virtual functions
    virtual uint32_t RecommendedWidth() const = 0;
    virtual uint32_t RecommendedHeight() const = 0;
    virtual uint32_t PhysicalResolutionWidth() const = 0;
    virtual uint32_t PhysicalResolutionHeight() const = 0;
    virtual float PhysicalSizeWidth() const = 0;
    virtual float PhysicalSizeHeight() const = 0;
    virtual uint32_t GetViewCount() const = 0; // 1,2, or more for multiview


    // Capability queries
    //virtual bool SupportsEyeTracking() const = 0;
    virtual std::set<XrFormFactor> GetSupportedFormFactors() const = 0;
    virtual XrSystemProperties GetSystemProperties() const = 0;
    virtual std::vector<XrViewConfigurationProperties> GetViewConfigurationProperties() const = 0;
    virtual std::vector<XrViewConfigurationView> GetViewConfigurationViews(XrViewConfigurationType type) const = 0;
    virtual std::vector<XrEnvironmentBlendMode> GetEnvironmentBlendModes() const = 0;
    virtual const FaceTrackingModule* GetFaceTracking() = 0;
    virtual bool IsAvailable() const = 0;


    // Hook for system-specific setup when a session or pipeline is created
    //virtual void OnAttach(DisplayBackend* backend) { (void)backend; }
    //virtual void OnDetach() {}

    XRSystem(XrSystemId sys_id, XRSystemType type, const std::string& name) : id(sys_id), type(type), name(name) {}
    virtual ~XRSystem() = default;
};
