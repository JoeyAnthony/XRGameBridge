/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "debug.h"

#include <format>
#include <sstream>

#include "session.h"
#include "instance.h"

void ThrowIfFailed(HRESULT hr) {
#ifdef  _DEBUG
    if (FAILED(hr)) {
        // Set a breakpoint on this line to catch DirectX API errors
        throw std::exception();
    }
#else

#endif

}

void TraceLogFunctionCall(std::string function_name, size_t line_number, XrSession session, XrInstance* instance) {
#ifdef DEBUG_FUNCTION_CALL
    //GB_Session& gb_session = g_sessions[session];
    spdlog::info("Trace log call: {}, {}", function_name, line_number);
#endif
}
