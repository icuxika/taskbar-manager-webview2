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

        const auto bridgeScript = LR"(
(() => {
    function randomUUID() {
        const temp = URL.createObjectURL(new Blob());
        const uuid = temp.toString();
        URL.revokeObjectURL(temp);
        return uuid.substring(uuid.lastIndexOf("/") + 1);
    }

    const listeners = new Map();
    const pending = new Map();

    function onMessage(event) {
        const msg = event.data;
        if (!msg) {
            return;
        }
        if (msg.id && msg.result !== undefined) {
            const p = pending.get(msg.id);
            if (p) {
                clearTimeout(p.timer);
                pending.delete(msg.id);
                if (msg.result && msg.result.error) {
                    p.reject(new Error(msg.result.message || "Native error!"));
                } else {
                    p.resolve(msg.result);
                }
            }
            return;
        }
        if (msg.event) {
            const set = listeners.get(msg.event);
            if (set) {
                for (const fn of set) {
                    try {
                        fn(msg.data);
                    } catch (_) {}
                }
            }
        }
    }

    function invoke(cmd, args, opts) {
        const id = randomUUID();
        const payload = { id, cmd, args };
        window.chrome.webview.postMessage(payload);
        return new Promise((resolve, reject) => {
            const timeout = (opts && opts.timeout) || 15000;
            const timer = setTimeout(() => {
                pending.delete(id);
                reject(new Error("invoke timeout: " + cmd));
            }, timeout);
            pending.set(id, { resolve, reject, timer });
        });
    }

    function on(event, fn) {
        if (!listeners.has(event)) {
            listeners.set(event, new Set());
        }
        listeners.get(event).add(fn);
        return () => listeners.get(event)?.delete(fn);
    }

    window.Native = { invoke, on };
    window.chrome.webview.addEventListener("message", onMessage);
})();
        )";
        webview->AddScriptToExecuteOnDocumentCreated(bridgeScript, nullptr);
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
                       receivedEventArgs) -> HRESULT {
                    wil::unique_cotaskmem_string message;
                    receivedEventArgs->get_WebMessageAsJson(&message);
                    nlohmann::json msg = nlohmann::json::parse(Utils::WStringToString(message.get()));
                    const std::string id = msg.value("id", "");
                    const std::string cmd = msg.value("cmd", "");
                    std::cout << "id: " << id << std::endl;
                    std::cout << "cmd: " << cmd << std::endl;
                    const nlohmann::json args = msg.contains("args") ? msg["args"] : nlohmann::json(nullptr);
                    if (cmd == "quit") {
                        PostQuitMessage(0);
                    }
                    if (cmd == "getWindows") {
                        const std::vector<WindowInfo> windows = WindowManager::GetTaskbarWindows();
                        using json = nlohmann::json;
                        json result;
                        result["windows"] = json::array();

                        for (const auto &info: windows) {
                            json windowJson;
                            if (std::string title = Utils::WStringToString(info.title); !title.empty()) {
                                windowJson["title"] = title;
                            } else {
                                windowJson["title"] = "(无标题)";
                            }
                            windowJson["handle"] = Utils::HWndToHexString(info.hWnd);
                            result["windows"].push_back(windowJson);
                        }
                        sendResult(id, result);
                    }
                    if (cmd == "activateWindow") {
                        const std::string handle = args.value("handle", "");
                        std::cout << "handle: " << handle << std::endl;
                        WindowManager::ActivateWindow(handle);
                    }
                    return S_OK;
                }).Get(), &token);
    }

    void WebViewController::LoadApplication() {
        if (!webview) {
            return;
        }
        const std::wstring exeDir = Utils::GetExeDirectory();
        const std::wstring htmlPath = exeDir + L"/index.html";
        const std::wstring url = L"file:///" + htmlPath;
        webview->Navigate(url.c_str());
    }

    void WebViewController::sendResult(const std::string &id, const nlohmann::json &result) {
        const nlohmann::json payload = {
            {"id", id},
            {"result", result}
        };
        webview->PostWebMessageAsJson(Utils::StringToWString(payload.dump()).c_str());
    }

    void WebViewController::emitEvent(const std::string &name, const nlohmann::json &data) {
        const nlohmann::json payload = {
            {"event", name},
            {"data", data}
        };
        webview->PostWebMessageAsJson(Utils::StringToWString(payload.dump()).c_str());
    }
}
