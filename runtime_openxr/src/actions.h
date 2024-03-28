#pragma once
#include "openxr_includes.h"

XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo);
XrResult xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state);
XrResult xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state);
XrResult xrGetActionStateVector2f(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateVector2f* state);
XrResult xrGetActionStatePose(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* state);

namespace  XRGameBridge {
    inline std::vector<std::string> g_supported_paths{
            "/user/hand/left",
            "/user/hand/right",
            "/user/head",
            "/user/gamepad"
    };

    // Only here for reference. Can be found here https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#semantic-path-user
    inline const std::vector g_unsupported_paths{
        "/user/treadmill"
    };
}
