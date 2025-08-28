#pragma once
#include <winsock2.h>
#include <windows.h>
#include <memory>
#include <thread>

#include "GlobalHotKeyManager.h"
#include "HttpServer.h"
#include "TrayManager.h"
#include "WebViewController.h"

namespace v1_taskbar_manager {
    class Application {
    public:
        static Application &GetInstance();

        Application(const Application &) = delete;

        Application &operator=(const Application &) = delete;

        int Run(HINSTANCE hInstance, int nCmdShow);

        LRESULT WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    private:
        Application() = default;

        ~Application() = default;

        bool RegisterWindowClass(HINSTANCE hInstance);

        HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow);

        void SetupSpdlog();

        void Cleanup();

        HWND hWnd = nullptr;
        int hotKeyId = 0;
        std::unique_ptr<HttpServer> httpServer;
        std::shared_ptr<GlobalHotKeyManager> globalHotKeyManager;
        std::unique_ptr<TrayManager> trayManager;
        std::unique_ptr<WebViewController> webViewController;
        HANDLE mutex = nullptr;
    };
}

// 全局窗口过程函数
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);