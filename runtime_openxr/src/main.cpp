#include <string>
#include <format>

#include <easylogging++.h>
#include <openxr/openxr.h>

#include "settings.h"

INITIALIZE_EASYLOGGINGPP

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD fdwReason, LPVOID) {

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
    {
        el::Configurations defaultConf;
        el::Loggers::reconfigureLogger("default", defaultConf);
        defaultConf.setToDefault();

        defaultConf.setGlobally(el::ConfigurationType::Format, "%datetime %level %loc %msg");
        defaultConf.setGlobally(el::ConfigurationType::ToStandardOutput, "true");
        defaultConf.setGlobally(el::ConfigurationType::ToFile, "true");

        char module_path[MAX_PATH];
        GetModuleFileNameA(hInst, module_path, MAX_PATH);
        runtime_path = std::string(module_path);

        std::string log_path = (fs::path(runtime_path).parent_path() /= "log.txt").string();
        defaultConf.setGlobally(el::ConfigurationType::Filename, log_path);

        FindPathEnv();

        LOG(INFO) << "DLL_PROCESS_ATTACH";

        LOG(INFO) << "XR Game Bridge Loaded";

        LOG(INFO) << "Runtime location: " << module_path;

        LOG(INFO) << "Game Bridge: VERSION";

        LOG(INFO) << "OpenXR API: " << std::format("{}.{}.{}", XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), XR_VERSION_MINOR(XR_CURRENT_API_VERSION), XR_VERSION_PATCH(XR_CURRENT_API_VERSION));

        LOG(INFO) << "Process: ";
        LOG(INFO) << "Executable: ";

        LOG(INFO) << "Support D3D11 " << (XRGameBridge::g_runtime_settings.support_d3d11 ? "TRUE" : "FALSE");
        LOG(INFO) << "Support D3D12 " << (XRGameBridge::g_runtime_settings.support_d3d12 ? "TRUE" : "FALSE");
        LOG(INFO) << "Support GL " << (XRGameBridge::g_runtime_settings.support_gl ? "TRUE" : "FALSE");
        LOG(INFO) << "Support VK " << (XRGameBridge::g_runtime_settings.support_vk ? "TRUE" : "FALSE");

        //if (FClientSettings::ClientSettings.AllowVK)
        //{
        //    int Status = gladLoaderLoadVulkan(nullptr, nullptr, nullptr);
        //    Log(FLogOpenXRInterface, Trace, "GLAD VK status: %i", Status);
        //}

        XRGameBridge::g_runtime_settings.hInst = hInst;

        // Allocate console for when none exists for debugging
        //AllocConsole();

        break;
    }

    case DLL_PROCESS_DETACH:
    {
        LOG(INFO) << "DLL_PROCESS_DETACH";

        LOG(INFO) << "XR Game Bridge Unloaded";

        //FreeConsole();
        break;
    }

    default: { break; }
    }
    return TRUE;
}

#include <winternl.h>
typedef NTSTATUS(NTAPI* _LdrLoadDll)(
    PWSTR DllPath, 
    ULONG pFlags, 
    PUNICODE_STRING DllName, 
    HMODULE* BaseAddress
    );
_LdrLoadDll LdrLoadDll;

#include <stdio.h>
#include <signal.h>
#include <tchar.h>


HMODULE LoadwithLdrLoadDLl(std::wstring dll_path)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) {
        LOG(ERROR) << "Failed to get ntdll";
        return NULL;
    }

    LdrLoadDll = (_LdrLoadDll)GetProcAddress(ntdll, "LdrLoadDll");
    if (!LdrLoadDll) {
        LOG(ERROR) << "Failed loading function";
        return NULL;
    }

    LOG(INFO) << "Attempting to load dll";
    UNICODE_STRING udll_name;
    RtlInitUnicodeString(&udll_name, dll_path.data());

    HMODULE loaded_module;
    NTSTATUS result = LdrLoadDll(NULL, 0, &udll_name, &loaded_module);
    if (!NT_SUCCESS(result)) {
        LOG(INFO) << "Failed loading dll";
        return NULL;
    }

    return loaded_module;
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
    std::wstring wdll_name = fs::path(dll_name).wstring();

    static bool weaving_loaded = false;
    if (!weaving_loaded) {
        LoadwithLdrLoadDLl(L"DimencoWeaving.dll");
        HMODULE mopd = GetModuleHandleW(L"DimencoWeaving.dll");
        weaving_loaded = true;
    }

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

        //LOG(INFO) << "dliStartProcessing " << "DLL Name: " << pdli->szDll;if (!eos_path.empty()) {
        //eos_module = GetModuleHandleA("EOSOVH-Win64-Shipping.dll");
        //FreeLibrary(eos_module);
        break;
    }
    case dliNotePreLoadLibrary:
    {
        // If you want to return control to the helper, return 0.
        // Otherwise, return your own HMODULE to be used by the
        // helper instead of having it call LoadLibrary itself.
        //LOG(INFO) << "dliNotePreLoadLibrary " << "DLL Name: " << pdli->szDll;

        if (std::find(sr_dlls.begin(), sr_dlls.end(), wdll_name) != sr_dlls.end()) {
            //fs::path gb_path = fs::path(runtime_path).parent_path() /= dll_name;
            //HMODULE gb_module = LoadLibraryExW(gb_path.wstring().data(), NULL, NULL);

            loaded_module = LoadwithLdrLoadDLl(wdll_name.data());
            //loaded_module = LoadLibraryExW(wdll_name.data(), NULL, LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE);

            LOG(INFO) << "Loading success";
            //LOG(INFO) << "Loading dll: " << gb_path.string();

            if (loaded_module == NULL) {
                //LOG(ERROR) << "Failed to load " << gb_path << " error: " << GetLastError();
                return 0;
            }

            //LOG(INFO) << "Successfully loaded " << gb_dll_name;
            return reinterpret_cast<FARPROC>(loaded_module);
        }
        //else if(dll_name.find(gb_dll_name) != std::string::npos) {
        //    LOG(INFO) << "Loading dll " << dll_name;

        //    fs::path gb_path = fs::path(runtime_path).parent_path() /= dll_name;
        //    std::wstring wpath = gb_path.wstring();
        //    HMODULE gb_module = LoadwithLdrLoadDLl(wpath.data());

        //    LOG(INFO) << "Loading success";
        //    //LOG(INFO) << "Loading dll: " << gb_path.string();

        //    if (gb_module == NULL) {
        //        //LOG(ERROR) << "Failed to load " << gb_path << " error: " << GetLastError();
        //        return 0;
        //    }

        //    //LOG(INFO) << "Successfully loaded " << gb_dll_name;
        //    return reinterpret_cast<FARPROC>(gb_module);
        //}
        else
        {
            LOG(INFO) << "Loading dll " << dll_name;

            fs::path dll_path = fs::path(dll_name);
            std::wstring wpath = dll_path.wstring();
            loaded_module = LoadLibraryExW(wpath.data(), NULL, NULL);

            LOG(INFO) << "Loading success";
            //LOG(INFO) << "Loading dll: " << gb_path.string();

            if (loaded_module == NULL) {
                //LOG(ERROR) << "Failed to load " << gb_path << " error: " << GetLastError();
                return 0;
            }

            //LOG(INFO) << "Successfully loaded " << gb_dll_name;
            return reinterpret_cast<FARPROC>(loaded_module);
        }
    }
    case dliNotePreGetProcAddress:
        // If you want to return control to the helper, return 0.
        // If you choose you may supply your own FARPROC function
        // address and bypass the helper's call to GetProcAddress.
        //LOG(INFO) << "dliNotePreGetProcAddress " << "DLL Name: " << pdli->szDll;


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

        //LOG(INFO) << "dliFailLoadLib " << "DLL Name: " << pdli->szDll;


        break;

    case dliFailGetProc:
        // GetProcAddress failed.
        // If you don't want to handle this failure yourself, return 0.
        // In this case the helper will raise an exception
        // (ERROR_PROC_NOT_FOUND) and exit.
        // If you choose, you may handle the failure by returning
        // an alternate FARPROC function address.
        //LOG(INFO) << "dliFailGetProc " << "DLL Name: " << pdli->szDll;


        break;

    case dliNoteEndProcessing:
        // This notification is called after all processing is done.
        // There is no opportunity for modifying the helper's behavior
        // at this point except by longjmp()/throw()/RaiseException.
        // No return value is processed.

        //LOG(INFO) << "dliNoteEndProcessing " << "DLL Name: " << pdli->szDll;


        //eos_module = LoadLibraryA(eos_path.data());

        break;

    default:
        //LOG(INFO) << "default" << "DLL Name: " << pdli->szDll;
        return NULL;
    }

    return NULL;
}

// and then at global scope somewhere:

ExternC const PfnDliHook __pfnDliNotifyHook2 = delayHook;
ExternC const PfnDliHook __pfnDliFailureHook2 = delayHook;
