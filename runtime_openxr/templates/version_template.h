#pragma once
#include <string_view>

namespace Version {
    constexpr int Major = ${PROJECT_VERSION_MAJOR};
    constexpr int Minor = ${PROJECT_VERSION_MINOR};
    constexpr int Patch = ${PROJECT_VERSION_PATCH};
    constexpr std::string_view Full = "@PROJECT_VERSION@";
}
