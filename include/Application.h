#pragma once
#include <windows.h>
#include <memory>

#include "GlobalHotKeyManager.h"
#include "TrayManager.h"
#include "WebViewController.h"

namespace v1_taskbar_manager {
    class Application {
    public:
        static Application &GetInstance();

        int Run(HINSTANCE hInstance, int nCmdShow);

        LRESULT WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    private:
        Application() = default;

        ~Application() = default;

        bool Initialize(HINSTANCE hInstance);

        bool RegisterWindowClass(HINSTANCE hInstance);

        HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow);

        void SetupDPI();

        void Cleanup();

        HINSTANCE hInstance = nullptr;
        HWND hWnd = nullptr;
        int hotKeyId = 0;
        std::shared_ptr<GlobalHotKeyManager> globalHotKeyManager;
        std::unique_ptr<TrayManager> trayManager;
        std::unique_ptr<WebViewController> webViewController;
        HANDLE mutex = nullptr;
    };
}

// 全局窗口过程函数
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
