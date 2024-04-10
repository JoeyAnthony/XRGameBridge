#pragma once
#include "openxr_includes.h"

namespace XRGameBridge {
    class GB_Display {
        // The main window class name.
        std::string window_class = "Game Bridge Window";

        // The string that appears in the application's title bar.
        std::string title = "XR Game Bridge";

        HWND h_wnd = 0;

        inline static bool window_class_is_registered = false;

    public:
        // Returns the window of the application or false if none exist
        static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
        bool InitWindowClass(HINSTANCE hInstance);
        bool CreateApplicationWindow(HINSTANCE hInstance, uint32_t width, uint32_t height, int nCmdShow, bool fullscreen = true);
        bool DestroyApplicationWindow();
        HWND GetWindowHandle();
        void UpdateWindow();
    };
}
