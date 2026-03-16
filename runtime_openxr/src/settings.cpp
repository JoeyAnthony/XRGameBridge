#include "settings.h"
#include <filesystem>
#include <regex>

#include <windows.h>

RuntimeLogger::RuntimeLogger() {
    const auto log_path = std::filesystem::path(g_runtime_settings->GetRuntimePath()).parent_path() / log_name;
    stderr_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string());
    multi_sink_logger = std::make_shared<spdlog::logger>("multi_sink", (stderr_sink, file_sink));

    multi_sink_logger->set_level(spdlog::level::debug);

    spdlog::set_default_logger(multi_sink_logger);

    //spdlog::error("Log path: {}", log_path.string());
}

std::shared_ptr<spdlog::logger> RuntimeLogger::GetLogger() {
    return multi_sink_logger;
}

RuntimeSettings::RuntimeSettings(void* h_Inst) {
    h_Instance = h_Inst;
    FetchRuntimePath();
    FetchPathEnvSR();
    InitializeClock();
}

void RuntimeSettings::FetchRuntimePath() {
    std::string module_path;
    module_path.resize(MAX_PATH);
    GetModuleFileNameA(static_cast<HINSTANCE>(h_Instance), module_path.data(), MAX_PATH);
    module_path.shrink_to_fit();
    runtime_path = module_path;
}

void RuntimeSettings::FetchPathEnvSR() {
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

void RuntimeSettings::InitializeClock() {
    QueryPerformanceCounter(&win_epoch.qpc);
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    win_epoch.frequency = freq.QuadPart;
    runtime_epoch = std::chrono::high_resolution_clock::now();
}

std::string RuntimeSettings::GetSrInstallPath() {
    return sr_install_path;
}

std::string RuntimeSettings::GetRuntimePath() {
    return runtime_path;
}

const std::chrono::high_resolution_clock::time_point& RuntimeSettings::GetRuntimeEpoch() {
    return runtime_epoch;
}

const WindowsEpoch RuntimeSettings::GetWindowsRuntimeEpoch()
{
    return win_epoch;
}

void* RuntimeSettings::GethInstance() {
    return h_Instance;
}
