/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "window.h"
#include <Windows.h>
#include "srsystem.h"

void MessageLoop() {
    // Main message loop:
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK GameBridgeWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PAINTSTRUCT ps;
    HDC hdc;
    std::string greeting("Hello, Windows desktop!");

    switch (message) {
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);

        // Here your application is laid out.
        // For this introduction, we just print out "Hello, Windows desktop!"
        // in the top left corner.
        TextOut(hdc, 5, 5, greeting.data(), (greeting.size()));
        // End application-specific layout section.

        EndPaint(hWnd, &ps);
        break;
    case(WM_CLOSE):
        spdlog::info("Window is closing");
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_QUIT:
        ShowWindow(hWnd, false);
        break;
    case WM_SIZE:
        // Ignore minimize (0x0 client area) so the renderer never tries to size resources to zero.
        if (wParam != SIZE_MINIMIZED) {
            pending_width = LOWORD(lParam);
            pending_height = HIWORD(lParam);
            has_pending_resize = true;
        }
        break;
    case WM_SIZING: {
        if (aspect_ratio > 0.0f) {
            RECT* drag_rect = reinterpret_cast<RECT*>(lParam);

            // The rect WM_SIZING hands us is the whole window (title bar + borders included),
            // but the ratio we care about is the client area that actually gets rendered into.
            RECT border{};
            AdjustWindowRectEx(&border, static_cast<DWORD>(GetWindowLongPtr(hWnd, GWL_STYLE)), FALSE,
                                static_cast<DWORD>(GetWindowLongPtr(hWnd, GWL_EXSTYLE)));
            const LONG border_width = border.right - border.left;
            const LONG border_height = border.bottom - border.top;

            LONG client_width = (drag_rect->right - drag_rect->left) - border_width;
            LONG client_height = (drag_rect->bottom - drag_rect->top) - border_height;

            switch (wParam) {
            case WMSZ_LEFT:
            case WMSZ_RIGHT:
                client_height = static_cast<LONG>(client_width / aspect_ratio);
                drag_rect->bottom = drag_rect->top + client_height + border_height;
                break;
            case WMSZ_TOP:
            case WMSZ_BOTTOM:
                client_width = static_cast<LONG>(client_height * aspect_ratio);
                drag_rect->right = drag_rect->left + client_width + border_width;
                break;
            case WMSZ_TOPLEFT:
                client_height = static_cast<LONG>(client_width / aspect_ratio);
                drag_rect->top = drag_rect->bottom - client_height - border_height;
                break;
            case WMSZ_BOTTOMLEFT:
                client_height = static_cast<LONG>(client_width / aspect_ratio);
                drag_rect->bottom = drag_rect->top + client_height + border_height;
                break;
            case WMSZ_TOPRIGHT:
                client_height = static_cast<LONG>(client_width / aspect_ratio);
                drag_rect->top = drag_rect->bottom - client_height - border_height;
                break;
            case WMSZ_BOTTOMRIGHT:
            default:
                client_height = static_cast<LONG>(client_width / aspect_ratio);
                drag_rect->bottom = drag_rect->top + client_height + border_height;
                break;
            }
        }
        return TRUE;
    }
    case WM_SYSKEYDOWN:
        // bit 29 of lParam is set when Alt is held down for a WM_SYSKEYDOWN.
        if (wParam == VK_RETURN && (lParam & (1 << 29))) {
            ToggleFullscreen(hWnd);
            return 0;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return 0;
}

bool GameBridgeWindow::InitWindowClass(HINSTANCE hInstance) {
    WNDCLASSEX window_ex;

    window_ex.cbSize = sizeof(WNDCLASSEX);
    window_ex.style = CS_HREDRAW | CS_VREDRAW;// | window_style;
    window_ex.lpfnWndProc = WndProc;
    window_ex.cbClsExtra = 0;
    window_ex.cbWndExtra = 0;
    window_ex.hInstance = hInstance;
    window_ex.hIcon = LoadIcon(window_ex.hInstance, IDI_APPLICATION);
    window_ex.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_ex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_ex.lpszMenuName = NULL;
    window_ex.lpszClassName = window_class.c_str();
    window_ex.hIconSm = LoadIcon(window_ex.hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&window_ex)) {
        uint32_t err = GetLastError();
        spdlog::error("Call to RegisterClassEx failed {}",err);
        MessageBox(NULL, "Call to RegisterClassEx failed!", "XR Game Bridge", NULL);

        return false;
    }

    return true;
}

void GameBridgeWindow::ToggleFullscreen(HWND hWnd) {
    // SetWindowLongPtr replaces the whole style bitmask, so WS_VISIBLE has to be carried over
    // explicitly or the window would vanish the moment the style changes.
    const LONG_PTR current_style = GetWindowLongPtr(hWnd, GWL_STYLE);
    const LONG_PTR visible_bit = current_style & WS_VISIBLE;

    if (!is_fullscreen) {
        // Remember the windowed placement so it can be restored later, then switch to a
        // borderless popup sized to whichever monitor the window is currently on.
        GetWindowRect(hWnd, &windowed_rect);

        SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | visible_bit);

        HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitor_info{};
        monitor_info.cbSize = sizeof(MONITORINFO);
        GetMonitorInfoA(monitor, &monitor_info);

        SetWindowPos(hWnd, HWND_TOP,
                     monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                     monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                     monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_NOACTIVATE);

        is_fullscreen = true;
    } else {
        // WS_OVERLAPPEDWINDOW brings back the title bar, system menu, and min/maximize/close
        // buttons on its own - nothing extra is needed for those to reappear.
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | visible_bit);

        SetWindowPos(hWnd, HWND_NOTOPMOST,
                     windowed_rect.left, windowed_rect.top,
                     windowed_rect.right - windowed_rect.left,
                     windowed_rect.bottom - windowed_rect.top,
                     SWP_FRAMECHANGED | SWP_NOACTIVATE);

        is_fullscreen = false;
    }
}

GameBridgeWindow::~GameBridgeWindow() {
    DestroyApplicationWindow();
}

bool GameBridgeWindow::CreateApplicationWindow(HINSTANCE hInstance, const std::shared_ptr<SRSystem>& system, uint32_t width, uint32_t height, int nCmdShow, bool fullscreen, bool showWindow) {
    spdlog::info("Creating application window");
    // TODO better window creation checking code
    static bool window_created = false;
    if (h_wnd != nullptr) {
        return false;
    }

    // Ensure the application receives unscaled display metrics
    auto dpi_context = GetThreadDpiAwarenessContext();
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (!window_class_is_registered) {
        InitWindowClass(hInstance);
        window_class_is_registered = true;
    }

    // Create window
    uint32_t window_style = 0;
    uint32_t borderless_fullscreen = WS_POPUP;
    uint32_t windowed = WS_OVERLAPPEDWINDOW;

    if (fullscreen) {
        window_style = borderless_fullscreen;
    }
    else {
        window_style = windowed;
    }
    is_fullscreen = fullscreen;

    // Get position of the SR display
    int window_x = CW_USEDEFAULT, window_y = CW_USEDEFAULT;
    if (system->IsConnected()) {
        const auto [offset, extent] = system->GetDisplayRect();
        const RECT rect{
            .left = offset.x,
            .top = offset.y,
            .right = offset.x + extent.width,
            .bottom = offset.y + extent.height
        };
        windowed_rect = rect;

        const HMONITOR h_monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);

        MONITORINFO monitor_info;
        monitor_info.cbSize = sizeof(MONITORINFO);
        GetMonitorInfoA(h_monitor, &monitor_info);
        window_x = monitor_info.rcMonitor.left;
        window_y = monitor_info.rcMonitor.top;
    }

    // Set the new window as a child window of the game's
    const long w = static_cast<long>(width);
    const long h = static_cast<long>(height);
    h_wnd = CreateWindowEx(0, window_class.c_str(), title.c_str(), window_style, window_x, window_y, w, h, h_wnd_external, NULL, hInstance, NULL);
    if (!h_wnd) {
        spdlog::error("Failed to create window, call to CreateWindow failed!");
        MessageBox(NULL, "Call to CreateWindow failed!", "XR Game Bridge", NULL);
        return false;
    }

    //SetWindowLongPtr(h_wnd, GWL_STYLE, window_style); //3d argument=style

    SetAspectRatio(width, height);

    SetWindowPos(
        h_wnd,
        HWND_TOPMOST,
        window_x,
        window_y,
        width,
        height,
        SWP_FRAMECHANGED | SWP_NOACTIVATE);

    // The parameters to ShowWindow explained:
    // h_wnd: the value returned from CreateWindow
    // nCmdShow: the fourth parameter from WinMain
    if (showWindow) {
        ShowWindow(h_wnd, SW_MAXIMIZE);
    }

    spdlog::info("Windows resolution: {}x{}", width, height);
    spdlog::info("Windows position: {}x{}", window_x, window_y);

    SetThreadDpiAwarenessContext(dpi_context);
    return true;
}

bool GameBridgeWindow::DestroyApplicationWindow() {
    // Must be destroyed from the creation thread
    if (h_wnd == nullptr) {
        spdlog::info("No window to destroy: {}", GetLastError());
        return true;
    }

    bool res = DestroyWindow(h_wnd);
    if (!res) {
       spdlog::error("Failed to destroy window: {}",GetLastError());
    }

    h_wnd = nullptr;
    return res;
}

HWND GameBridgeWindow::GetWindowHandle() {
    return h_wnd;
}

void GameBridgeWindow::SetAspectRatio(uint32_t width, uint32_t height) {
    if (height == 0) {
        aspect_ratio = 0.0f;
        return;
    }
    aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
}

bool GameBridgeWindow::ConsumePendingResize(uint32_t& out_width, uint32_t& out_height) {
    if (!has_pending_resize) {
        return false;
    }
    out_width = pending_width;
    out_height = pending_height;
    has_pending_resize = false;
    return true;
}

void GameBridgeWindow::UpdateWindow() {
    // Main message loop:
    MSG msg;
    if (PeekMessageA(&msg, h_wnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

bool GameBridgeWindow::PeekMessageExternal(LPMSG& msg) {
    if (h_wnd_external == nullptr) {
        return false;
    }
    return PeekMessageA(msg, h_wnd_external, 0, 0, PM_NOREMOVE);
}
