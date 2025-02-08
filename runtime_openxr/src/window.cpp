#include "window.h"

#include <Windows.h>
#include "platform_manager.h"

namespace XRGameBridge {
    void MessageLoop() {
        // Main message loop:
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    LRESULT CALLBACK GB_Window::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
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
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_QUIT:
            ShowWindow(hWnd, false);
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
            break;
        }

        return 0;
    }

    bool GB_Window::InitWindowClass(HINSTANCE hInstance)
    {
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
            LOG(ERROR) << "Call to RegisterClassEx failed " << err;
            MessageBox(NULL, "Call to RegisterClassEx failed!", "XR Game Bridge", NULL);

            return false;
        }
    }

    GB_Window::~GB_Window() {
        DestroyApplicationWindow();
    }

    bool GB_Window::CreateApplicationWindow(HINSTANCE hInstance, GB_System& system, uint32_t width, uint32_t height, int nCmdShow, bool fullscreen, bool showWindow) {
        // TODO better window creation checking code
        static bool window_created = false;
        if (h_wnd != nullptr) {
            return false;
        }

        auto dpi_context = GetThreadDpiAwarenessContext();
        if (dpi_context != DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE) {
            // Ensure the application receives unscaled display metrics
            //SetProcessDpiAwareness(PROCESS_DPI_AWARENESS::PROCESS_PER_MONITOR_DPI_AWARE);
            const DPI_AWARENESS_CONTEXT context = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE;
            SetThreadDpiAwarenessContext(context);
        }

        // Always try to get the external display before creating one ourselves
        TryGetExternalDisplay();

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

        // Get position of the SR display
        int window_x = CW_USEDEFAULT, window_y = CW_USEDEFAULT;
        if(system.GetIsConnected()) {
            auto display_rect = system.sr_display->getLocation();
            window_x = display_rect.left;
            window_y = display_rect.top;
            RECT rect(display_rect.left, display_rect.top, display_rect.right, display_rect.bottom);
            HMONITOR h_monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);

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
            MessageBox(NULL, "Call to CreateWindow failed!", "XR Game Bridge", NULL);
            return false;
        }

        //SetWindowLongPtr(h_wnd, GWL_STYLE, window_style); //3d argument=style

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

        return true;
    }

    bool GB_Window::DestroyApplicationWindow()
    {
        // Must be destroyed from the creation thread
        if(h_wnd == nullptr) {
            LOG(INFO) << "No window to destroy: " << GetLastError();
            return true;
        }

        bool res = DestroyWindow(h_wnd);
        if(!res)
        {
            LOG(ERROR) << "Failed to destroy window: " << GetLastError();
        }

        h_wnd = nullptr;
        return res;
    }

    HWND GB_Window::GetWindowHandle() {
        return h_wnd;
    }

    void GB_Window::UpdateWindow() {
        // Main message loop:
        MSG msg;
        if (PeekMessageA(&msg, h_wnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    HWND GB_Window::TryGetExternalDisplay()
    {
        // Make sure we get the root window, assuming all games uses its root window for showing the game and processing input.
        HWND h_wnd_active = GetActiveWindow();
        //HWND h_wnd_ancestor = GetAncestor(h_wnd_active, GA_ROOT);

        //if(h_wnd_active == h_wnd_ancestor)
        //{
        //
        //}

        if(h_wnd_active == nullptr)
        {
            return nullptr;
        }

        if(h_wnd_active == h_wnd)
        {
            return nullptr;
        }

        h_wnd_external = h_wnd_active;
        return h_wnd_active;
    }

    bool GB_Window::PeekMessageExternal(LPMSG& msg) {
        if (h_wnd_external == nullptr) {
            return false;
        }
        return PeekMessageA(msg, h_wnd_external, 0, 0, PM_NOREMOVE);
    }
}
