#pragma once
#include "openxr_includes.h"
#include "system.h"

class GameBridgeWindow {
    // The main window class name.
    std::string window_class = "Game Bridge Window";

    // The string that appears in the application's title bar.
    std::string title = "XR Game Bridge";

    inline static HWND h_wnd = 0;
    inline static HWND h_wnd_external = 0;

    inline static bool window_class_is_registered = false;

    bool InitWindowClass(HINSTANCE hInstance);
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
    ~GameBridgeWindow();
    // Returns the window of the application or false if none exist
    bool CreateApplicationWindow(HINSTANCE hInstance, GB_System& system, uint32_t width, uint32_t height, int nCmdShow, bool fullscreen = true, bool showWindow = true);
    bool DestroyApplicationWindow();
    HWND GetWindowHandle();
    void UpdateWindow();

    /*
     * Returns the game window
     */
    static HWND TryGetExternalDisplay();

    bool PeekMessageExternal(LPMSG& msg);
};
