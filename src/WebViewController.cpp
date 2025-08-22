#include "WebViewController.h"

#include <iostream>
#include <sstream>

#include "json.hpp"
#include "Utils.h"
#include "WindowManager.h"
#include "Shlwapi.h"

namespace v1_taskbar_manager {
    WebViewController::WebViewController(HWND hWnd, std::weak_ptr<GlobalHotKeyManager> globalHotKeyManager,
                                         int port): hWnd(hWnd),
                                                    globalHotKeyManager(globalHotKeyManager), port(port) {
    }

    WebViewController::~WebViewController() {
        if (webviewController) {
            webviewController->Close();
        }
    }

    /**
     * @brief 初始化WebViewController
     * @note 调用CreateCoreWebView2EnvironmentWithOptions创建CoreWebView2环境，
     * 并在环境创建完成后创建CoreWebView2Controller
     */
    void WebViewController::Initialize() {
        std::wstring runtimePath = GetWebView2RuntimePath();
        CreateCoreWebView2EnvironmentWithOptions(
            runtimePath.empty() ? nullptr : runtimePath.c_str(),
            nullptr,
            nullptr,
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
                }).Get()
        );
    }

    void WebViewController::Resize(const RECT &bounds) {
        if (webviewController != nullptr) {
            webviewController->put_Bounds(bounds);
        }
    }

    /**
     * @brief 设置WebView2的相关设置
     * @note 启用脚本、默认脚本对话框、Web消息、开发者工具等
     */
    void WebViewController::SetupWebViewSettings() {
        if (!webview) {
            return;
        }
        wil::com_ptr<ICoreWebView2Settings> settings;
        webview->get_Settings(&settings);
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreDevToolsEnabled(TRUE);

        webview->AddScriptToExecuteOnDocumentCreated(
            wil::make_bstr(Utils::LoadWStringFromResource(304, 305).c_str()).get(), nullptr);
        webview->AddScriptToExecuteOnDocumentCreated(L"Object.freeze(Object);", nullptr);
    }

    /**
     * @brief 注册WebView2的消息处理函数
     * @note 当WebView2接收到消息时，会调用此函数处理消息
     */
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
                        nlohmann::json result;
                        result["windows"] = nlohmann::json::array();

                        for (const auto &info: windows) {
                            nlohmann::json windowJson;
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
                    if (cmd == "registerHotkey") {
                        const nlohmann::json hotkey =
                                args.contains("hotkey") ? args["hotkey"] : nlohmann::json(nullptr);
                        const bool ctrl = hotkey.value("ctrl", false);
                        const bool shift = hotkey.value("shift", false);
                        const bool alt = hotkey.value("alt", false);
                        const std::string key = hotkey.value("key", "");
                        std::cout << "ctrl: " << ctrl << std::endl;
                        std::cout << "shift: " << shift << std::endl;
                        std::cout << "alt: " << alt << std::endl;
                        std::cout << "key: " << key << std::endl;

                        if (auto ghm = globalHotKeyManager.lock()) {
                            int ret = ghm->RegisterGlobalHotKey(ctrl, shift, alt, key, [this] {
                                ShowWindow(this->hWnd, SW_RESTORE);
                                SetForegroundWindow(this->hWnd);
                            });

                            nlohmann::json result;
                            if (ret == -1) {
                                result["message"] = "操作失败";
                            } else {
                                result["message"] = "操作成功";
                            }
                            sendResult(id, result);
                        }
                    }
                    if (cmd == "clearHotkey") {
                        if (auto ghm = globalHotKeyManager.lock()) {
                            ghm->UnregisterAll();

                            nlohmann::json result;
                            result["message"] = "操作成功";
                            sendResult(id, result);
                        }
                    }
                    return S_OK;
                }).Get(), &token);
    }

    /**
     * @brief 获取WebView2运行时路径
     * @return std::wstring WebView2运行时路径
     * @note 从可执行文件路径中获取WebView2运行时路径
     */
    std::wstring WebViewController::GetWebView2RuntimePath() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);

        std::wstring exeDir = exePath;
        exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\/"));

        std::wstring runtimePath = exeDir + L"\\webview2_runtime";
        if (PathFileExistsW(runtimePath.c_str())) {
            return runtimePath;
        }
        return L"";
    }

    /**
     * @brief 加载应用程序
     * @note 导航到指定的URL，URL格式为"http://localhost:端口号"
     */
    void WebViewController::LoadApplication() {
        if (!webview) {
            return;
        }
        std::wstringstream url;
        url << L"http://localhost:" << port;
        webview->Navigate(url.str().c_str());
    }

    /**
     * @brief 发送结果消息
     * @param id 消息ID
     * @param result 结果数据
     * @note 用于向WebView2发送结果消息，消息格式为JSON字符串
     */
    void WebViewController::sendResult(const std::string &id, const nlohmann::json &result) {
        const nlohmann::json payload = {
            {"id", id},
            {"result", result}
        };
        webview->PostWebMessageAsJson(Utils::StringToWString(payload.dump()).c_str());
    }

    /**
     * @brief 发送事件消息
     * @param name 事件名称
     * @param data 事件数据
     * @note 用于向WebView2发送事件消息，消息格式为JSON字符串
     */
    void WebViewController::emitEvent(const std::string &name, const nlohmann::json &data) {
        const nlohmann::json payload = {
            {"event", name},
            {"data", data}
        };
        webview->PostWebMessageAsJson(Utils::StringToWString(payload.dump()).c_str());
    }
}
