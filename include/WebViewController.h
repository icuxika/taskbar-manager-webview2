#pragma once
#include <windows.h>
#include <string>
#include <wrl.h>
#include <wil/com.h>
#include "WebView2.h"

using namespace Microsoft::WRL;

namespace v1_taskbar_manager {
    class WebViewController {
    public:
        explicit WebViewController(HWND hWnd);

        ~WebViewController();

        void Initialize();

        void Resize(const RECT &bounds);

        void ProcessMessage(const std::wstring &message);

        void SendWindowsListToWebView();

    private:
        HWND hWnd;
        wil::com_ptr<ICoreWebView2Controller> webviewController;
        wil::com_ptr<ICoreWebView2> webview;

        void SetupWebViewSettings();

        void RegisterMessageHandler();

        void LoadApplication();
    };
}
