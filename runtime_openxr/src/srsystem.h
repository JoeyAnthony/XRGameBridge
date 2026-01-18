/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once
#include "system.h"

#include <set>

#include "types.h"
#include "featuremodule.h"

// SR
#include <sr/utility/exception.h>
#include <sr/sense/core/inputstream.h>
#include <sr/sense/system/systemsense.h>
#include <sr/sense/eyetracker/eyetracker.h>
#include <sr/world/display/display.h>
// GLM
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace SR {
    class PredictingDX12Weaver;
    class SRContext;
    class Display;
    class SwitchableLensHint;
}

class SrSystemEventListener final : public SR::SystemEventListener {
public:
    SR::InputStream<SR::SystemEventStream> stream;
    void accept(const SR::SystemEvent& frame) override {
        //spdlog::info("SR Service event: {}", frame.message);
    };
};

class SrEyePairListener final : public SR::EyePairListener {
    SR::InputStream<SR::EyePairStream> stream;
    glm::dvec3 left = {-30.0f, 0.0f, 600.0f};
    glm::dvec3 right = { 30.0f, 0.0f, 600.0f };

    void accept(const SR_eyePair& eyePair) override {
        left = { eyePair.left.x, eyePair.left.y, eyePair.left.z };
        right = { eyePair.right.x, eyePair.right.y, eyePair.right.z };
    }

public:
    SrEyePairListener() = delete;
    ~SrEyePairListener() = default;

    explicit SrEyePairListener(SR::EyeTracker* tracker) {
        stream.set(tracker->openEyePairStream(this));
    }

    std::tuple<glm::dvec3, glm::dvec3> GetEyePositions() const {
        return { left, right };
    }
};

class SrEyeTrackingSystemFeature final : public FaceTrackingModule {
    SrEyePairListener eye_pair_listener;
public:
    SrEyeTrackingSystemFeature() = delete;
    explicit SrEyeTrackingSystemFeature(SR::SRContext& sr_context);
    ~SrEyeTrackingSystemFeature() override {};

    std::tuple<XrVector3f, XrVector3f> SrEyeTrackingSystemFeature::GetEyePositions(double x_offset) const override;
};

class PipelineStep {
public:
    PipelineStep() = delete;
    explicit PipelineStep(GraphicsBackend graphics): graphics(graphics){};

    virtual ~PipelineStep() = default;
protected:
    GraphicsBackend graphics;

public:
    virtual void execute_pipeline_step(void* command_list) = 0;
};

class WeaverPipeline : public PipelineStep {
public:
    WeaverPipeline() = delete;
    explicit WeaverPipeline(GraphicsBackend graphics): PipelineStep(graphics) {};
};

class D3D12WeaverPipeline final : public WeaverPipeline {
    //std::unique_ptr<SR::PredictingDX12Weaver> weaver;
public:
    D3D12WeaverPipeline() = delete;
    D3D12WeaverPipeline(const std::shared_ptr<SR::SRContext>& context);

    ~D3D12WeaverPipeline() override;
    void execute_pipeline_step(void* command_list) override;
};

class SystemPipelineFactory {
public:
    virtual ~SystemPipelineFactory() = default;
    virtual PipelineStep* CreateD3D11DistortionPipeline() = 0;
    virtual PipelineStep* CreateD3D12DistortionPipeline() = 0;
};

class SrPipelineFactory final: public SystemPipelineFactory {
    std::shared_ptr<SR::SRContext> context;
public:
    explicit SrPipelineFactory(std::shared_ptr<SR::SRContext> context);
    PipelineStep* CreateD3D11DistortionPipeline() override { return nullptr; };
    PipelineStep* CreateD3D12DistortionPipeline() override;
};

/*
 * Extra notes
 * When the screen is closer ro the user, most users cannot handle more than 50% of the real eye separation.
 */
class SRSystem: public XRSystem {
    XrInstance instance;
    GraphicsBackend active_graphics_backend;
    
    XrFormFactor form_factor;
    //bool features_enumerated = false;
    bool device_is_connected = false;

    // Screen params
    float physical_screen_width_m;
    float physical_screen_height_m;
    float ppi;
    uint32_t physical_resolution_width;
    uint32_t physical_resolution_height;
    uint32_t recommended_resolution_width;
    uint32_t recommended_resolution_height;
    uint32_t swapchain_image_width;
    uint32_t swapchain_image_height;


    // SR
    std::unique_ptr <SR::SRContext> context;
    SrSystemEventListener system_event_listener;
    SR::Display* display;
    SR::SwitchableLensHint* lens_hint;


    // System features
    std::array<std::unique_ptr<FeatureModule>, static_cast<int>(FeatureType::FeatureCount)> feature_modules;


    // Head params
    glm::vec3 head_position;
    glm::vec3 head_direction;
    float interpupillary_distance_m = 0.062f;

    void InitializeSrContext();
    void InitializeSrDisplay();

    static glm::u32vec2 GetResolutionMainDisplay();


public:
    void GetHeadPosition();

    // Eye positions relative to the center of the screen in meters
    XrFovf GetConvergingFov(const glm::vec3& eye_position);
    
    // Returns whether this device is a connected SR display
    bool IsConnected() const;

    XrRect2Di GetDisplayRect() const;

    static std::shared_ptr<SRSystem> Create(XrInstance instance);


    //Temporary
   SR::SRContext* GetSrContext() { return context.get(); };

public:
    SRSystem() = delete;
    explicit SRSystem(XrSystemId sys_id, GraphicsBackend graphics);
    ~SRSystem() override = default;

    uint32_t RecommendedWidth() const override;
    uint32_t RecommendedHeight() const override;
    uint32_t PhysicalResolutionWidth() const override;
    uint32_t PhysicalResolutionHeight() const override;
    uint32_t GetViewCount() const override;

    std::set<XrFormFactor> GetSupportedFormFactors() const  override { return { XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY }; };
    XrSystemProperties GetSystemProperties() const override;
    std::vector<XrViewConfigurationProperties> GetViewConfigurationProperties() const override;
    std::vector<XrViewConfigurationView> GetViewConfigurationViews(XrViewConfigurationType type) const override;
    std::vector<XrEnvironmentBlendMode> GetEnvironmentBlendModes() const override { return { XR_ENVIRONMENT_BLEND_MODE_OPAQUE }; };
    bool IsAvailable() const override;
    const FaceTrackingModule* GetFaceTracking() override;
    uint32_t PhysicalSizeWidth() const override;
    uint32_t PhysicalSizeHeight() const override;

private:
};

// Spaces are basically transformation matrices.
// They transform a point/orientation with respect to an XrSpace of the applications choosing
struct GB_ReferenceSpace {
    XrReferenceSpaceType space_type;
};

struct GB_ActionSpace {
    XrAction action;
    XrPath sub_action_path;
};
