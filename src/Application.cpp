#include "Application.h"

#include <dwmapi.h>
#include <iostream>
#include "Constants.h"
#include "Utils.h"

namespace v1_taskbar_manager {
    Application &Application::GetInstance() {
        static Application instance;
        return instance;
    }

    int Application::Run(HINSTANCE hInstance, int nCmdShow) {
        SetupDPI();

        if (!Utils::IsRunningAsAdmin()) {
            Utils::RelaunchAsAdmin();
            return 0;
        }

        if (Utils::IsAlreadyRunning(L"TaskbarManagerWebview2", mutex)) {
            MessageBoxW(nullptr, L"程序已经在运行", L"错误", MB_OK | MB_ICONERROR);
            return 0;
        }

        Initialize(hInstance);
        if (!RegisterWindowClass(hInstance)) {
            MessageBox(nullptr, TEXT("Failed to register window class!"),
                       TEXT("Error"), MB_ICONERROR);
            return 1;
        }

        // Utils::CreateConsole();

        this->hWnd = CreateMainWindow(hInstance, nCmdShow);
        if (!hWnd) {
            MessageBox(nullptr, TEXT("Failed to create window!"),
                       TEXT("Error"), MB_ICONERROR);
            return 1;
        }

        this->globalHotKeyManager = std::make_shared<GlobalHotKeyManager>(hWnd);
        this->trayManager = std::make_unique<TrayManager>(hWnd);
        this->webViewController = std::make_unique<WebViewController>(hWnd, this->globalHotKeyManager);


        // 设置窗口圆角
        constexpr DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
        DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

        trayManager->AddTrayIcon();
        webViewController->Initialize();

        ShowWindow(hWnd,
                   nCmdShow);
        UpdateWindow(hWnd);

        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        Cleanup();
        return static_cast<int>(msg.wParam);
    }

    LRESULT Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
            case WM_SIZE:
                if (this->webViewController != nullptr) {
                    RECT bounds;
                    GetClientRect(hWnd, &bounds);
                    this->webViewController->Resize(bounds);
                };
                break;
            case WM_ACTIVATE:
                if (LOWORD(wParam) == WA_INACTIVE) {
                    HWND hNewActive = (HWND) lParam;
                    WCHAR className[256] = {};
                    if (hNewActive)
                        GetClassNameW(hNewActive, className, 255);

                    std::wcout << L"WM_ACTIVATE lost focus to: " << className << std::endl;

                    if (hNewActive == nullptr ||
                        (GetParent(hNewActive) != hWnd &&
                         wcscmp(className, L"Chrome_WidgetWin_1") != 0)) {
                        ShowWindow(hWnd, SW_HIDE); // 隐藏到托盘
                    }
                }
                break;
            case WM_TRAY_ICON: {
                if (this->trayManager) {
                    this->trayManager->HandleTrayMessage(wParam, lParam);
                }
            }
            break;
            case WM_CLOSE: {
                const int result = MessageBoxW(hWnd,
                                               L"是否退出程序？\n点击“否”将最小化到托盘。",
                                               L"退出确认",
                                               MB_ICONQUESTION | MB_YESNO);

                if (result == IDYES) {
                    PostQuitMessage(0); // 真正退出
                } else {
                    ShowWindow(hWnd, SW_HIDE); // 否则只是隐藏窗口
                }
                return 0;
            }
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case ID_TRAY_ABOUT:
                        MessageBoxW(hWnd, L"Windows 任务栏窗口管理\nBy 浮木", L"关于", MB_ICONINFORMATION);
                        break;
                    case ID_TRAY_EXIT:
                        PostQuitMessage(0);
                        break;
                    case ID_TRAY_ENABLE_HOTKEY:
                        hotKeyId = globalHotKeyManager->RegisterGlobalHotKey('T', MOD_CONTROL | MOD_ALT, [this]() {
                            ShowWindow(this->hWnd, SW_RESTORE);
                            SetForegroundWindow(this->hWnd);
                        });
                        if (hotKeyId != 0) {
                            MessageBoxW(hWnd, L"已成功注册全局快捷键 Ctrl+Alt+T", L"全局快捷键", MB_ICONINFORMATION);
                        }
                        break;
                    case ID_TRAY_DISABLE_HOTKEY:
                        if (hotKeyId != 0) {
                            if (globalHotKeyManager->UnregisterHotKey(hotKeyId)) {
                                MessageBoxW(hWnd, L"已成功取消全局快捷键 Ctrl+Alt+T", L"全局快捷键", MB_ICONINFORMATION);
                            }
                        }
                        break;
                    default: ;
                }
                break;
            case WM_HOTKEY:
                if (globalHotKeyManager) {
                    globalHotKeyManager->HandleHotKeyMessage(wParam);
                }
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        return 0;
    }

    bool Application::Initialize(HINSTANCE hInstance) {
        this->hInstance = hInstance;
        return true;
    }

    bool Application::RegisterWindowClass(HINSTANCE hInstance) {
        WNDCLASSEX wcex;

        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hInstance;
        wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = szWindowClass;
        wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

        return RegisterClassEx(&wcex);
    }

    HWND Application::CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
        const int physicalWidth = GetSystemMetrics(SM_CXSCREEN);;
        const int physicalHeight = GetSystemMetrics(SM_CYSCREEN);
        const UINT dpi = GetDpiForSystem();
        const float scale = dpi / 96.0f;
        const int logicalWidth = static_cast<int>(physicalWidth / scale);
        const int logicalHeight = static_cast<int>(physicalHeight / scale);

        const int windowWidth = 480 * scale;
        const int windowHeight = (logicalHeight - 80) * scale;
        const int x = (logicalWidth - 480 - 16) * scale;
        const int y = 16 * scale;

        std::cout << "physicalWidth: " << physicalWidth << std::endl;
        std::cout << "physicalHeight: " << physicalHeight << std::endl;
        std::cout << "logicalWidth: " << logicalWidth << std::endl;
        std::cout << "logicalHeight: " << logicalHeight << std::endl;

        return CreateWindowEx(
            WS_EX_TOOLWINDOW,
            szWindowClass,
            szTitle,
            WS_POPUP | WS_VISIBLE,
            x, y,
            windowWidth, windowHeight,
            nullptr,
            nullptr,
            hInstance,
            nullptr
        );
    }

    void Application::SetupDPI() {
        SetProcessDPIAware();
    }

    void Application::Cleanup() {
        webViewController.reset();
        trayManager.reset();
        globalHotKeyManager.reset();
        if (mutex) {
            CloseHandle(mutex);
        }
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return v1_taskbar_manager::Application::GetInstance().WindowProc(hWnd, message, wParam, lParam);
}
