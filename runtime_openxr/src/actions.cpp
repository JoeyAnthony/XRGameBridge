/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "actions.h"

#include <vector>

#include "instance.h"
#include "openxr_functions.h"

XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo) {
    const std::vector<XrActiveActionSet> active_action_sets(syncInfo->activeActionSets, syncInfo->activeActionSets + syncInfo->countActiveActionSets);

    try {
        for (auto& set : active_action_sets) {
            auto& gb_action_set = g_action_sets.at(set.actionSet);
            // Check if it is attached to the passed session parameter
            if (gb_action_set.session != session) {
                return XR_ERROR_ACTIONSET_NOT_ATTACHED;
            }
        }
    }
    catch (std::out_of_range& e) {
        return XR_ERROR_HANDLE_INVALID;
    }
    catch (std::exception& e) {
        LOG_RUNTIME_ERROR
        return XR_ERROR_RUNTIME_FAILURE;
    }

    //LOG(INFO) << "Called " << __func__;
    return XR_SUCCESS;
}

XrResult xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state) {
    TraceLogFunctionCall(__func__, __LINE__);

    state->isActive = false;
    state->currentState = false;
    state->changedSinceLastSync = false;
    state->lastChangeTime = 0;
    //LOG(INFO) << "Called " << __func__;
    return XR_SUCCESS;
}

XrResult xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state) {
    TraceLogFunctionCall(__func__, __LINE__);

    GB_Session& gb_session = g_sessions[session];

    state->isActive = false;
    state->currentState = 0.f;
    state->changedSinceLastSync = false;
    state->lastChangeTime = 0;

    //LOG(INFO) << "Called " << __func__;
    return XR_SUCCESS;
}

XrResult xrGetActionStateVector2f(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateVector2f* state) {
    TraceLogFunctionCall(__func__, __LINE__);

    state->isActive = false;
    state->currentState = {0.f};
    state->changedSinceLastSync = false;
    state->lastChangeTime = 0;

    //LOG(INFO) << "Called " << __func__;
    return XR_SUCCESS;
}

XrResult xrGetActionStatePose(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* state) {
    TraceLogFunctionCall(__func__, __LINE__);

    state->isActive = false;

    //LOG(INFO) << "Called " << __func__;
    return XR_SUCCESS;
}
