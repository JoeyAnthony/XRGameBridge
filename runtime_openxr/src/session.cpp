#include "session.h"

#include <stdexcept>

#include "openxr_functions.h"
#include "instance.h"
#include "system.h"

XrResult xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session) {
    static uint64_t session_creation_count = 1;
    try {
        GameBridge::GB_System& system = GameBridge::systems.at(createInfo->systemId);
        if (!system.features_enumerated) {
            return XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING;
        }

        if (system.instance != instance) {
            return XR_ERROR_SYSTEM_INVALID;
        }
    }
    catch (std::out_of_range& e) {
        return XR_ERROR_SYSTEM_INVALID;
    }
    catch (std::exception& e) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    *session = reinterpret_cast<XrSession>(session_creation_count);
    GameBridge::GB_Session new_session;
    new_session.id = *session;
    new_session.instance = instance;
    new_session.system = createInfo->systemId;
    GameBridge::sessions.insert({ *session, new_session });

    session_creation_count++;
    return XR_SUCCESS;
}

XrResult xrDestroySession(XrSession session) {
    return test_return;
}
