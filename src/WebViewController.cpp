#include "WebViewController.h"

#include <iostream>

#include "json.hpp"
#include "Utils.h"
#include "WindowManager.h"

namespace v1_taskbar_manager {
    WebViewController::WebViewController(HWND hWnd): hWnd(hWnd) {
    }

    WebViewController::~WebViewController() {
        if (webviewController) {
            webviewController->Close();
        }
    }

    void WebViewController::Initialize() {
        CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
                                                 Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                                                     [this](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
                                                         // Create a CoreWebView2Controller and get the associated CoreWebView2 whose parent is the main window hWnd
                                                         env->CreateCoreWebView2Controller(
                                                             hWnd, Callback<
                                                                 ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                                                                 [this](HRESULT result,
                                                                        ICoreWebView2Controller *controller) ->
                                                             HRESULT {
                                                                     if (controller != nullptr) {
                                                                         webviewController = controller;
                                                                         webviewController->get_CoreWebView2(&webview);
                                                                     }

                                                                     SetupWebViewSettings();
                                                                     RegisterMessageHandler();

                                                                     RECT bounds;
                                                                     GetClientRect(hWnd, &bounds);
                                                                     webviewController->put_Bounds(bounds);

                                                                     LoadApplication();
                                                                     return S_OK;
                                                                 }).Get());
                                                         return S_OK;
                                                     }).Get());
    }

    void WebViewController::Resize(const RECT &bounds) {
        if (webviewController != nullptr) {
            webviewController->put_Bounds(bounds);
        }
    }

    void WebViewController::SetupWebViewSettings() {
        if (!webview) {
            return;
        }
        wil::com_ptr<ICoreWebView2Settings> settings;
        webview->get_Settings(&settings);
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);

        webview->AddScriptToExecuteOnDocumentCreated(L"Object.freeze(Object);", nullptr);
    }

    void WebViewController::RegisterMessageHandler() {
        if (!webview) {
            return;
        }
        EventRegistrationToken token;
        webview->add_WebMessageReceived(
            Callback<
                ICoreWebView2WebMessageReceivedEventHandler>(
                [this](ICoreWebView2 *webview,
                       ICoreWebView2WebMessageReceivedEventArgs *
                       args) -> HRESULT {
                    wil::unique_cotaskmem_string message;
                    args->TryGetWebMessageAsString(&message);
                    ProcessMessage(message.get());
                    return S_OK;
                }).Get(), &token);
    }

    void WebViewController::LoadApplication() {
        if (!webview) {
            return;
        }
        std::wstring exeDir = Utils::GetExeDirectory();
        std::wstring htmlPath = exeDir + L"/index.html";
        std::wstring url = L"file:///" + htmlPath;
        webview->Navigate(url.c_str());
    }

    void WebViewController::ProcessMessage(const std::wstring &msg) {
        if (msg == L"quit") {
            PostQuitMessage(0);
        }
        if (msg == L"getWindows") {
            SendWindowsListToWebView();
        }
        if (msg.rfind(L"activateWindow|", 0) ==
            0) {
            size_t pos = msg.find(L'|');
            if (pos != std::wstring::npos && pos +
                1 < msg.size()) {
                std::wstring content = msg.substr(
                    pos + 1);
                WindowManager::ActivateWindow(content);
            }
        }
    }

    void WebViewController::SendWindowsListToWebView() {
        if (!webview) {
            return;
        }
        std::vector<WindowInfo> windows = WindowManager::GetTaskbarWindows();

        using json = nlohmann::json;
        json result;
        result["windows"] = json::array();

        for (const auto &info: windows) {
            json windowJson;

            // 将宽字符串转换为UTF-8字符串
            const int size_needed = WideCharToMultiByte(CP_UTF8, 0, info.title.c_str(), -1,
                                                        nullptr, 0, nullptr, nullptr);
            if (size_needed > 0) {
                std::string titleUtf8(size_needed - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, info.title.c_str(), -1, &titleUtf8[0],
                                    size_needed, nullptr, nullptr);
                windowJson["title"] = titleUtf8;
            } else {
                windowJson["title"] = "(无标题)";
            }
            windowJson["handle"] = v1_taskbar_manager::Utils::HWndToHexString(info.hWnd);
            result["windows"].push_back(windowJson);
        }
        webview->PostWebMessageAsString(v1_taskbar_manager::Utils::StringToWString(result.dump(2)).c_str());
    }
}
