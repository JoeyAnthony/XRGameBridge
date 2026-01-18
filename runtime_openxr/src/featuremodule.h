#pragma once
#include <openxr/openxr.h>

enum class FeatureType {
    None,
    EyeTracking,
    FeatureCount
};

class FeatureModule {
protected:
    FeatureType type = FeatureType::None;
    explicit FeatureModule(FeatureType t) : type(t) {}
public:
    virtual ~FeatureModule() = default;
    [[nodiscard]] FeatureType GetFeatureType() const { return type; }
};

class FaceTrackingModule : public FeatureModule {
protected:
    FaceTrackingModule() : FeatureModule(FeatureType::EyeTracking) {}
public:
    virtual std::tuple<XrVector3f, XrVector3f> GetEyePositions(double x_offset) const = 0;
};
