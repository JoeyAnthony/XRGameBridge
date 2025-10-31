#pragma once
#include <windows.h>

class WindowHooks {
    inline static bool is_hooked = false;

public:
    WindowHooks();

    void ActivateWindowMessageHook(HWND h_wnd = nullptr);
    void RestoreWindowMessageHook();

    void OpenConsole();

    void CloseConsole();
};
