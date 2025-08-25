#pragma once
#include <windows.h>
#include <string>
#include <wil/com.h>

#include "GlobalHotKeyManager.h"
#include <nlohmann/json.hpp>
#include "WebView2.h"

using namespace Microsoft::WRL;

namespace v1_taskbar_manager {
    class WebViewController {
    public:
        explicit WebViewController(HWND hWnd, const std::weak_ptr<GlobalHotKeyManager> &globalHotKeyManager, int port);

        ~WebViewController();

        void Initialize();

        void Resize(const RECT &bounds) const;

    private:
        HWND hWnd;
        wil::com_ptr<ICoreWebView2Controller> webviewController;
        wil::com_ptr<ICoreWebView2> webview;
        std::weak_ptr<GlobalHotKeyManager> globalHotKeyManager;
        int port;

        void SetupWebViewSettings() const;

        void RegisterMessageHandler() const;

        static std::wstring GetUserDataFolder();

        static std::wstring GetWebView2RuntimePath();

        void LoadApplication() const;

        void ProcessMessage(const std::string &id, const std::string &cmd, const nlohmann::json &args,
                            const std::function<void(const nlohmann::json &)> &callback) const;

        nlohmann::json ResultResponse(const std::string &id, int code, const std::string &msg,
                                      const nlohmann::json &data) const;

        nlohmann::json EmitResponse(const std::string &name, const nlohmann::json &data) const;
    };
}