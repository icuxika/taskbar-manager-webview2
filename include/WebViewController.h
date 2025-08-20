#pragma once
#include <windows.h>
#include <string>
#include <wrl.h>
#include <wil/com.h>

#include "json.hpp"
#include "WebView2.h"

using namespace Microsoft::WRL;

namespace v1_taskbar_manager {
    class WebViewController {
    public:
        explicit WebViewController(HWND hWnd);

        ~WebViewController();

        void Initialize();

        void Resize(const RECT &bounds);

    private:
        HWND hWnd;
        wil::com_ptr<ICoreWebView2Controller> webviewController;
        wil::com_ptr<ICoreWebView2> webview;

        void SetupWebViewSettings();

        void RegisterMessageHandler();

        void LoadApplication();

        void sendResult(const std::string &id, const nlohmann::json &result);

        void emitEvent(const std::string &name, const nlohmann::json &data);
    };
}
