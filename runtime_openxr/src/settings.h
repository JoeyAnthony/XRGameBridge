/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once
#include <regex>
#include <windows.h>
#include <string>
#include <filesystem>
#include  <array>

#include "debug.h"

namespace fs = std::filesystem;

struct GB_RuntimeSettings {
    bool support_d3d12 = true;
    bool support_d3d11 = false;
    bool support_vk = false;
    bool support_gl = false;
    HINSTANCE hInst;
} inline g_runtime_settings;

constexpr std::array sr_dlls = {
L"SimulatedRealityDirectX.dll",
L"SimulatedReality.dll",
L"SimulatedRealityCore.dll",
L"SimulatedRealityCameras.dll",
L"SimulatedRealityHandTrackers.dll",
L"SimulatedRealityFaceTrackers.dll",
L"SimulatedRealityUserModelers.dll",
L"SimulatedRealityDisplays.dll",
L"DimencoWeaving.dll",
L"glog.dll",
L"LeapC.dll",
L"opencv_world343.dll"
};

inline std::string sr_install_path;
inline std::string sr_install_path_win32;
inline std::string runtime_path;
static void FindPathEnv() {
    std::string path_environment_variable = std::getenv("PATH");
    std::regex path_search_regex("[a-zA-Z0-9+_\\-\\.:%()\\s\\\\]+");

    auto words_begin = std::sregex_iterator(path_environment_variable.begin(), path_environment_variable.end(), path_search_regex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        std::string match_str = match.str();
        spdlog::info(match_str);
        if (match_str.find("Simulated Reality") != std::string::npos) {
            if (match_str.find("x86") != std::string::npos) {
                sr_install_path_win32 = match_str;
            }
            else {
                sr_install_path = match_str;
            }
        }
    }
}
