#pragma once
#include <openxr/openxr.h>

enum class FeatureType {
    None,
    EyeTracking,
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
    virtual std::vector<XrView> GetEyePositions() = 0;
};
