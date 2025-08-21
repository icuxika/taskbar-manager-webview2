#pragma once
#include <windows.h>
#include <string>
#include <wrl.h>
#include <wil/com.h>

#include "GlobalHotKeyManager.h"
#include "json.hpp"
#include "TrayManager.h"
#include "WebView2.h"

using namespace Microsoft::WRL;

namespace v1_taskbar_manager {
    class WebViewController {
    public:
        explicit WebViewController(HWND hWnd, std::weak_ptr<GlobalHotKeyManager> globalHotKeyManager);

        ~WebViewController();

        void Initialize();

        void Resize(const RECT &bounds);

    private:
        HWND hWnd;
        wil::com_ptr<ICoreWebView2Controller> webviewController;
        wil::com_ptr<ICoreWebView2> webview;
        std::weak_ptr<GlobalHotKeyManager> globalHotKeyManager;

        void SetupWebViewSettings();

        void RegisterMessageHandler();

        void LoadApplication();

        void sendResult(const std::string &id, const nlohmann::json &result);

        void emitEvent(const std::string &name, const nlohmann::json &data);
    };
}
