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
