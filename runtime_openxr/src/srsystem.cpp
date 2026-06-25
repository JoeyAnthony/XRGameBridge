/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "srsystem.h"

#include <sr/management/srcontext.h>
#include <sr/world/display/display.h>
#include <sr/sense/display/switchablehint.h>
#include <sr/utility/exception.h>

#include "instance.h"

SrEyeTrackingSystemFeature::SrEyeTrackingSystemFeature(SR::SRContext& sr_context) :
    eye_pair_listener(SrEyePairListener(SR::PredictingEyeTracker::create(sr_context))) {
    spdlog::info("Creating SR Predicting Eye Tracker");
    sr_context.initialize();
}

std::tuple<XrVector3f, XrVector3f> SrEyeTrackingSystemFeature::GetEyePositions(double x_offset) const {
    auto [right, left] = eye_pair_listener.GetEyePositions();
    std::tuple vec = {
        XrVector3f {
        .x = static_cast<float>(right.x / 1000),
        .y = static_cast<float>(right.y / 1000),
        .z = static_cast<float>(right.z / 1000)
        },
        XrVector3f {
        .x = static_cast<float>(left.x / 1000),
        .y = static_cast<float>(left.y / 1000),
        .z = static_cast<float>(left.z / 1000)
        },
    };
    return vec;
}

D3D12WeaverPipeline::D3D12WeaverPipeline(const std::shared_ptr<SR::SRContext>& context) : WeaverPipeline(GraphicsBackend::D3D12) {
    //weaver = new SR::PredictingDX12Weaver(context.get(), d3d12_device.Get(), command_allocators[0].Get(), d3d12_command_queue.Get(), intermediate_resource->GetBuffers()[0].Get(), window_swapchain.GetImages()[0].Get(), window.GetWindowHandle());
}

D3D12WeaverPipeline::~D3D12WeaverPipeline() {
}

void D3D12WeaverPipeline::execute_pipeline_step(void* command_list) {

}

SrPipelineFactory::SrPipelineFactory(std::shared_ptr<SR::SRContext> context) {
    context = std::move(context);
}

PipelineStep* SrPipelineFactory::CreateD3D12DistortionPipeline() {
    return new D3D12WeaverPipeline(context);
}

void SRSystem::InitializeSrContext() {
    spdlog::info("Initializing SR Context");
    for (uint32_t retries = 0; retries < max_retries; retries++) {
        if (retries >= max_retries) {
            throw XrException(XR_ERROR_RUNTIME_FAILURE, "Could not connect to sr service, max retries reached");
        }

        if (context == nullptr) {
            try {
                context = std::make_unique<SR::SRContext>(SR::SRContext::create());

                // Set systemEvent listener to the newly constructed systemsense
                SR::SystemSense* systemSense = SR::SystemSense::create(*context);
                system_event_listener.stream.set(systemSense->openSystemEventStream(&system_event_listener));

                break;
            }
            catch (SR::ServerNotAvailableException& ex) {
                // Unable to construct SR Context.
                spdlog::error("SR Service not available, retrying...");
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));
            }
        }
    }

    context->initialize();
}

void SRSystem::InitializeSrDisplay() {
    spdlog::info("Initializing SR Display");

    for (uint32_t retries = 0; retries < max_retries; retries++) {
        if (retries >= max_retries) {
            throw XrException(XR_ERROR_RUNTIME_FAILURE, "Could not find any connected SR display max retries reached");
        }

        display = SR::Display::create(*context);
        if(display) {
            context->initialize();
            break;
        }

        spdlog::error("SR display not found, retrying...");
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));
    }
}

SRSystem::SRSystem(XrSystemId sys_id, GraphicsBackend graphics) : XRSystem(sys_id, XRSystemType::SRSystem, "Simulated Reality Display") {
    spdlog::info("Initializing SR System");
    instance = instance;
    form_factor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    active_graphics_backend = graphics;

    InitializeSrContext();
    InitializeSrDisplay();

    // Check if an sr display is connected.
    // This is done by checking if the virtual display coordinates of the screen are all 0 or not.
    auto display_coordinates = display->getLocation();
    if (display_coordinates.left == 0 &&
        display_coordinates.bottom == 0 &&
        display_coordinates.right == 0 &&
        display_coordinates.top == 0) {
        // For when no SR display is connected, and if it's an 8K SR display it should work as well
        device_is_connected = false;
        auto res = GetResolutionMainDisplay();

        physical_resolution_width = res.x;
        physical_resolution_height = res.y;

        // Render at half width
        swapchain_image_width = res.x / 2;
        swapchain_image_height = res.y;

        // Some values of a 32 inch display
        physical_screen_width_m = 0.69f;
        physical_screen_height_m = 0.3880f;
    }

    device_is_connected = true;

    lens_hint = SR::SwitchableLensHint::create(*context);

    physical_resolution_width = static_cast<uint32_t>(display->getPhysicalResolutionWidth());
    physical_resolution_height = static_cast<uint32_t>(display->getPhysicalResolutionHeight());

    recommended_resolution_width = static_cast<uint32_t>(display->getRecommendedViewsTextureWidth());
    recommended_resolution_height = static_cast<uint32_t>(display->getRecommendedViewsTextureHeight());

    // Render at half width
    swapchain_image_width = physical_resolution_width / 2;
    swapchain_image_height = physical_resolution_height;

    physical_screen_width_m = display->getPhysicalSizeWidth() / 100.f;
    physical_screen_height_m = display->getPhysicalSizeHeight() / 100.f;

    spdlog::info("Physical resolution:    {}x{}", physical_resolution_width, physical_resolution_height);
    spdlog::info("Recommended resolution: {}x{}", recommended_resolution_width, recommended_resolution_height);
}

XrFovf SRSystem::GetConvergingFov(const glm::vec3& eye_position) {
    static glm::vec3 old_position = { 0.0f, 0.0f, 0.30f };

    float half_width = physical_screen_width_m / 2;
    float half_height = physical_screen_height_m / 2;

    float z = eye_position.z; //glm::clamp(eye_position.z, 0.001f, 5.0f); // where to check this and restore valid values?
    float half_pi = glm::pi<float>() / 2;

    float z_scale = half_width / half_height;

    auto fov = XrFovf{
        glm::clamp(glm::atan(-(half_width + eye_position.x) / z), -half_pi, half_pi),    //Left
        glm::clamp(glm::atan((half_width - eye_position.x) / z), -half_pi, half_pi),    //Right
        glm::clamp(glm::atan((half_height - eye_position.y) / z), -half_pi, half_pi),    //Up
        glm::clamp(glm::atan(-(half_height + eye_position.y) / z), -half_pi, half_pi)    //Down
    };

    // Make sure the view can't be vertically or horizontally flipped. Also the depth is larger than 0.
    //if (fov.angleLeft > fov.angleRight || fov.angleDown > fov.angleUp /*|| eye_position.z < 0.001f*/) {
    //    // Set to last accepted angles
    //    //eye_position = old_position;
    //    return GetConvergingFov(old_position);
    //}

    old_position = eye_position;

    return fov;
}

bool SRSystem::IsConnected() const {
    return device_is_connected;
}

XrRect2Di SRSystem::GetDisplayRect() const {
    const auto rect = display->getLocation();
    const auto ext = XrExtent2Di{ static_cast<int32_t>(rect.right - rect.left), static_cast<int32_t>(rect.bottom - rect.top) };
    const auto off = XrOffset2Di{ static_cast<int32_t>(rect.topLeft.x), static_cast<int32_t>(rect.topLeft.y) };
    return XrRect2Di{
        .offset = off,
        .extent = ext,
    };
}

glm::u32vec2 SRSystem::GetResolutionMainDisplay() {
    return glm::u32vec2{
        static_cast<uint32_t>(GetSystemMetrics(SM_CXSCREEN)),
        static_cast<uint32_t>(GetSystemMetrics(SM_CYSCREEN))
    };
}

std::shared_ptr<SRSystem> SRSystem::Create(XrInstance instance) {
    GB_Instance* gb_instance = reinterpret_cast<GB_Instance*>(instance);

    // Create system
    XrSystemId id = g_systems.size() + 1; // 0 is NULL_SYSTEM_HANDLE
    std::shared_ptr<SRSystem> system = std::make_shared<SRSystem>(id, gb_instance->GetActiveGraphicsAPI());
    g_systems.insert({ id, system });
    spdlog::info("Created system: {}", id);
    return system;
}

uint32_t SRSystem::RecommendedWidth() const {
    return physical_resolution_width;
}

uint32_t SRSystem::RecommendedHeight() const {
    return physical_resolution_height;
}

uint32_t SRSystem::PhysicalResolutionWidth() const {
    return physical_resolution_width;
}

uint32_t SRSystem::PhysicalResolutionHeight() const {
    return physical_resolution_height;
}

uint32_t SRSystem::GetViewCount() const {
    if(form_factor == XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY) {
        return 2;
    }
    else {
        return 1;
    }
}

XrSystemProperties SRSystem::GetSystemProperties() const {
    XrSystemGraphicsProperties g_props{};
    g_props.maxLayerCount = XR_MIN_COMPOSITION_LAYERS_SUPPORTED;
    g_props.maxSwapchainImageWidth = physical_resolution_width * 2;
    g_props.maxSwapchainImageHeight = physical_resolution_height * 2;

    XrSystemTrackingProperties t_props{};
    t_props.positionTracking = false;
    t_props.orientationTracking = false;

    XrSystemProperties sys_props{
        XR_TYPE_SYSTEM_PROPERTIES,
        nullptr,
        id,
        0x354B, // USB Vendor ID
        "SR Monitor",
        g_props,
        t_props
    };
    return sys_props;
}

std::vector<XrViewConfigurationProperties> SRSystem::GetViewConfigurationProperties() const {
    std::vector <XrViewConfigurationProperties> props {
        //{
        //    .type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES,
        //    .viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO,
        //    .fovMutable = true,
        //},
        {
            .type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES,
            .viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            .fovMutable = true,
        }
    };
    return props;
}

std::vector<XrViewConfigurationView> SRSystem::GetViewConfigurationViews(XrViewConfigurationType type) const {
    std::vector<XrViewConfigurationView> views;
    if (type == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
        const XrViewConfigurationView view{
            .type = XR_TYPE_VIEW_CONFIGURATION_VIEW,
            .recommendedImageRectWidth = RecommendedWidth(),
            .maxImageRectWidth = PhysicalResolutionWidth(),
            .recommendedImageRectHeight = RecommendedHeight(),
            .maxImageRectHeight = PhysicalResolutionHeight(),
            .recommendedSwapchainSampleCount = 1,
            .maxSwapchainSampleCount = 1,
        };

        views.push_back(view);
        views.push_back(view);
    }
    return views;
}

bool SRSystem::IsAvailable() const {
    if(context == nullptr) {
        spdlog::error("No connection to SR Service");
        return false;
    }
    return true;
}

const FaceTrackingModule* SRSystem::GetFaceTracking() {
    if(feature_modules[static_cast<int>(FeatureType::EyeTracking)] == nullptr) {
        feature_modules[static_cast<int>(FeatureType::EyeTracking)] = std::make_unique<SrEyeTrackingSystemFeature>(*context);
    }

    return static_cast<FaceTrackingModule*>(feature_modules[static_cast<int>(FeatureType::EyeTracking)].get());
}

float SRSystem::PhysicalSizeWidth() const {
    return physical_screen_width_m;
}

float SRSystem::PhysicalSizeHeight() const {
    return physical_screen_width_m;
}
