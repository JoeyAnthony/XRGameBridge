/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

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


namespace fs = std::filesystem;

#include<windows.h>
struct WindowsEpoch{
    LARGE_INTEGER qpc;
    int64_t frequency;
};

class RuntimeLogger {
    static constexpr std::string_view log_name = "xrgb_log.txt";
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> stderr_sink;
    std::shared_ptr<spdlog::sinks::basic_file_sink_mt> file_sink;
    std::shared_ptr<spdlog::logger> multi_sink_logger;

public:
    RuntimeLogger();
    std::shared_ptr<spdlog::logger> GetLogger();
};

class RuntimeSettings {
    std::string sr_install_path;
    std::string sr_install_path_win32;
    std::string runtime_path;
    std::chrono::high_resolution_clock::time_point runtime_epoch;
    WindowsEpoch win_epoch;

    void* h_Instance;

    void FetchRuntimePath();
    void FetchPathEnvSR();
    void InitializeClock();

public:
    static constexpr bool support_d3d12 = true;
    static constexpr bool support_d3d11 = false;
    static constexpr bool support_vk = false;
    static constexpr bool support_gl = false;

    RuntimeSettings() = delete;
    explicit RuntimeSettings(void* h_Inst);

    std::string GetSrInstallPath();
    std::string GetRuntimePath();
    const std::chrono::high_resolution_clock::time_point& GetRuntimeEpoch();
    const WindowsEpoch GetWindowsRuntimeEpoch();
    void* GethInstance();
};

inline std::unique_ptr<RuntimeSettings> g_runtime_settings;
inline std::unique_ptr<RuntimeLogger> g_runtime_logger;
