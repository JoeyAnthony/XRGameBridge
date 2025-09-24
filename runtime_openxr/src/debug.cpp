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
