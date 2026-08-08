/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#pragma once
#include "openxr_includes.h"
#include "SRSystem.h"

class GameBridgeWindow {
    // The main window class name.
    std::string window_class = "Game Bridge Window";

    // The string that appears in the application's title bar.
    std::string title = "XR Game Bridge";

    inline static HWND h_wnd = 0;
    inline static HWND h_wnd_external = 0;

    inline static bool window_class_is_registered = false;

    // Locked width-over-height ratio enforced while the user drags the window border.
    // 0 means no constraint has been set yet, and resizing is left unconstrained.
    inline static float aspect_ratio = 0.0f;

    // Set from WM_SIZE, consumed once per frame by the renderer.
    inline static bool has_pending_resize = false;
    inline static uint32_t pending_width = 0;
    inline static uint32_t pending_height = 0;

    // Tracks borderless-fullscreen (WS_POPUP) vs windowed (WS_OVERLAPPEDWINDOW), and the last
    // known windowed placement so toggling back from fullscreen restores a sensible rect.
    inline static bool is_fullscreen = false;
    inline static RECT windowed_rect{};

    static void ToggleFullscreen(HWND hWnd);

    bool InitWindowClass(HINSTANCE hInstance);
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
    ~GameBridgeWindow();
    // Returns the window of the application or false if none exist
    bool CreateApplicationWindow(HINSTANCE hInstance, const std::shared_ptr<SRSystem>& system, uint32_t width, uint32_t height, int nCmdShow, bool fullscreen = true, bool showWindow = true);
    bool DestroyApplicationWindow();
    HWND GetWindowHandle();
    void UpdateWindow();

    // Locks interactive resizing to the given width/height ratio. Pass width/height of the
    // desired ratio, e.g. the SR panel's physical resolution, or a single eye's aspect for side-by-side.
    void SetAspectRatio(uint32_t width, uint32_t height);

    // Returns true once if the window's client area changed size since the last call, filling
    // out_width/out_height with the new client size. Meant to be polled once per frame.
    bool ConsumePendingResize(uint32_t& out_width, uint32_t& out_height);

    bool PeekMessageExternal(LPMSG& msg);
};
