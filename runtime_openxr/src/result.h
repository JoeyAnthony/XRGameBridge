/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once

#include <unordered_map>
#include <string>

#include "debug.h"

namespace XRGameBridge {
    enum class GB_Result {
        GB_SUCCESS,
    };

    const std::unordered_map <GB_Result, std::string> results {
        {}
    };

    const std::unordered_map <GB_Result, std::string> errors {

    };

    std::string GetSuccessMessage() {
        return std::string("Success");
    }

    std::string GetErrorMessage(GB_Result result) {
        std::string finalResult;
        if (result == GB_Result::GB_SUCCESS) {
            finalResult = GetSuccessMessage();
        }
        else if (result > GB_Result::GB_SUCCESS) {
            finalResult = results[result];
        }
        else {
            finalResult = errors[result];
        }
    }

    void LogErrorMessage(GB_Result result) {
        LOG(GetErrorMessage(result));
    }
}
