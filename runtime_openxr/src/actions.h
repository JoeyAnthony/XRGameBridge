/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once
#include "openxr_includes.h"

XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo);
XrResult xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state);
XrResult xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state);
XrResult xrGetActionStateVector2f(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateVector2f* state);
XrResult xrGetActionStatePose(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* state);

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
