/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once
#include <stdexcept>
#include <windows.h>

#include <openxr/openxr.h>
#include "spdlog/spdlog.h"

#define LOG_RUNTIME_ERROR spdlog::error("RUNTIME FAILURE func: {} ln: {}", __func__, __LINE__);

void ThrowIfFailed(HRESULT hr);
void TraceLogFunctionCall(std::string function_name, size_t line_number, XrSession session = XR_NULL_HANDLE, XrInstance* instance = XR_NULL_HANDLE);

class XrException : public std::runtime_error {
    const XrResult xr_result;
public:
    XrException(XrResult result, std::string message) : xr_result(result), std::runtime_error(message) {
        spdlog::error("XrException was thrown: ", message);
    }

    XrResult GetResult() {
        return xr_result;
    }
};
