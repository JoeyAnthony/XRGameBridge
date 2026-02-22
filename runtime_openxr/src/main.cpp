/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include <string>
#include <format>
#include <filesystem>

#include <spdlog/spdlog.h>
#include <openxr/openxr.h>

#include "settings.h"
#include "debug.h"

namespace fs = std::filesystem;

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD fdwReason, LPVOID) {

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
    {
        if (g_runtime_settings == nullptr) {
            g_runtime_settings = std::make_unique<RuntimeSettings>(hInst);
            g_runtime_logger = std::make_unique<RuntimeLogger>();
        }

        spdlog::info("DLL_PROCESS_ATTACH");

        spdlog::info("XR Game Bridge Loaded");

        spdlog::info("Runtime location: {}", g_runtime_settings->GetRuntimePath());

        spdlog::info("Game Bridge: VERSION");

        spdlog::info("OpenXR API: {}.{}.{}", XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), XR_VERSION_MINOR(XR_CURRENT_API_VERSION), XR_VERSION_PATCH(XR_CURRENT_API_VERSION));

        spdlog::info("Process: ");
        spdlog::info("Executable: ");

        spdlog::info("Support D3D11 {}", RuntimeSettings::support_d3d11 ? "TRUE" : "FALSE");
        spdlog::info("Support D3D12 {}", RuntimeSettings::support_d3d12 ? "TRUE" : "FALSE");
        spdlog::info("Support GL {}", RuntimeSettings::support_gl ? "TRUE" : "FALSE");
        spdlog::info("Support VK {}", RuntimeSettings::support_vk ? "TRUE" : "FALSE");

        //if (FClientSettings::ClientSettings.AllowVK)
        //{
        //    int Status = gladLoaderLoadVulkan(nullptr, nullptr, nullptr);
        //    Log(FLogOpenXRInterface, Trace, "GLAD VK status: %i", Status);
        //}

        // Allocate console for when none exists for debugging
        //AllocConsole();

        break;
    }

    case DLL_PROCESS_DETACH:
    {
        spdlog::info("DLL_PROCESS_DETACH");

        spdlog::info("XR Game Bridge Unloaded");

        //FreeConsole();
        break;
    }

    default: { break; }
    }
    return TRUE;
}

// Targets are delayed in CMake
#include <delayimp.h>
#pragma comment(lib, "ntdll.lib")
FARPROC WINAPI delayHook(unsigned dliNotify, PDelayLoadInfo pdli) {
#ifdef _DEBUG
    std::string gb_dll_name = "3DGameBridged.dll";
#else
    std::string gb_dll_name = "3DGameBridge.dll";
#endif

    std::string dll_name(pdli->szDll);
    static HMODULE loaded_module = 0;

    //static std::string eos_path;
    //HMODULE eos_module = 0;
    //if (eos_path.empty()) {
    //    eos_module = GetModuleHandleA("EOSOVH-Win64-Shipping.dll");
    //    char buffer[MAX_PATH];
    //    GetModuleFileNameA(eos_module, buffer, MAX_PATH);
    //    eos_path = std::string(buffer);
    //    //FreeLibrary(eos_module);
    //}

    switch (dliNotify) {
    case dliStartProcessing:
    {
        // If you want to return control to the helper, return 0.
        // Otherwise, return a pointer to a FARPROC helper function
        // that will be used instead, thereby bypassing the rest
        // of the helper.

        //spdlog::info("dliStartProcessing " << "DLL Name: " << pdli->szDll;if (!eos_path.empty()) {
        //eos_module = GetModuleHandleA("EOSOVH-Win64-Shipping.dll");
        //FreeLibrary(eos_module);
        break;
    }
    case dliNotePreLoadLibrary:
    {
        // If you want to return control to the helper, return 0.
        // Otherwise, return your own HMODULE to be used by the
        // helper instead of having it call LoadLibrary itself.
        //spdlog::info("dliNotePreLoadLibrary " << "DLL Name: " << pdli->szDll;
        spdlog::info("Loading dll ", dll_name);
        fs::path dll_path = fs::path(dll_name);
        if (dll_name.find(gb_dll_name) != std::string::npos) {
            dll_path = fs::path(g_runtime_settings->GetRuntimePath()).parent_path() /= dll_name;
            if (fs::exists(dll_path) == false) {
                spdlog::info("Debug this!");
            }

            spdlog::info("Try loading {} from: {}", dll_name, dll_path.string());
            loaded_module = LoadLibraryA(dll_path.string().data());
        }
        else {
            std::wstring wpath = dll_path.wstring();
            loaded_module = LoadLibraryExW(wpath.data(), NULL, NULL);
        }

        if (loaded_module == NULL) {
            spdlog::error("Failed to load {} error: {}", dll_path.string(), GetLastError());
            return 0;
        }

        spdlog::info("Loading success");
        return reinterpret_cast<FARPROC>(loaded_module);

    }
    case dliNotePreGetProcAddress:
        // If you want to return control to the helper, return 0.
        // If you choose you may supply your own FARPROC function
        // address and bypass the helper's call to GetProcAddress.
        //spdlog::info("dliNotePreGetProcAddress " << "DLL Name: " << pdli->szDll;


        //return reinterpret_cast<FARPROC>(loaded_module);
        break;

    case dliFailLoadLib:
        // LoadLibrary failed.
        // If you don't want to handle this failure yourself, return 0.
        // In this case the helper will raise an exception
        // (ERROR_MOD_NOT_FOUND) and exit.
        // If you want to handle the failure by loading an alternate
        // DLL (for example), then return the HMODULE for
        // the alternate DLL. The helper will continue execution with
        // this alternate DLL and attempt to find the
        // requested entrypoint via GetProcAddress.

        throw XrException(XR_ERROR_RUNTIME_FAILURE, std::format("LoadLibrary failed: {}", pdli->szDll));
        break;

    case dliFailGetProc:
        // GetProcAddress failed.
        // If you don't want to handle this failure yourself, return 0.
        // In this case the helper will raise an exception
        // (ERROR_PROC_NOT_FOUND) and exit.
        // If you choose, you may handle the failure by returning
        // an alternate FARPROC function address.
        //spdlog::info("dliFailGetProc " << "DLL Name: " << pdli->szDll;


        break;

    case dliNoteEndProcessing:
        // This notification is called after all processing is done.
        // There is no opportunity for modifying the helper's behavior
        // at this point except by longjmp()/throw()/RaiseException.
        // No return value is processed.

        //spdlog::info("dliNoteEndProcessing " << "DLL Name: " << pdli->szDll;


        //eos_module = LoadLibraryA(eos_path.data());

        break;

    default:
        //spdlog::info("default" << "DLL Name: " << pdli->szDll;
        return NULL;
    }

    return NULL;
}

// and then at global scope somewhere:

ExternC const PfnDliHook __pfnDliNotifyHook2 = delayHook;
ExternC const PfnDliHook __pfnDliFailureHook2 = delayHook;
