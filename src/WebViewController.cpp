#include "WebViewController.h"

#include <iostream>
#include <sstream>
#include <wrl.h>

#include <nlohmann/json.hpp>

#include "Constants.h"
#include "ShlObj.h"
#include "Shlwapi.h"
#include "Utils.h"
#include "WindowManager.h"
#include "spdlog/spdlog.h"

namespace v1_taskbar_manager {
    WebViewController::WebViewController(HWND hWnd, const std::weak_ptr<GlobalHotKeyManager> &globalHotKeyManager,
                                         int port)
        : hWnd(hWnd), globalHotKeyManager(globalHotKeyManager), port(port) {
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
        const std::wstring runtimePath = GetWebView2RuntimePath();
        const std::wstring userDataFolder = GetUserDataFolder();
        SPDLOG_INFO("WebView2 运行时路径: {}", Utils::WStringToString(runtimePath));
        SPDLOG_INFO("WebView2 用户数据文件夹: {}", Utils::WStringToString(userDataFolder));

        HRESULT result = CreateCoreWebView2EnvironmentWithOptions(
            runtimePath.c_str(),
            userDataFolder.c_str(),
            nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([this](HRESULT result,
                ICoreWebView2Environment *env)
                -> HRESULT {
                    env->CreateCoreWebView2Controller(
                        hWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [this](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {
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
                            })
                        .Get());
                    return S_OK;
                }).Get());
        if (FAILED(result)) {
            LPWSTR buffer = nullptr;
            FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                result,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPWSTR>(&buffer),
                0,
                nullptr
                );
            const std::wstring msg(buffer ? buffer : L"");
            if (buffer) {
                LocalFree(buffer);
            }
            SPDLOG_ERROR("无法创建 WebView2 环境，HRESULT=0x{:08X}, {}", static_cast<unsigned long>(result),
                         Utils::WStringToString(msg));
            if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
                SPDLOG_ERROR("当前电脑上未安装 WebView2 Runtime 且当前应用程序未自带 WebView2 Runtime 相关文件");
            }
            MessageBox(hWnd, L"请检查电脑上是否安装了 WebView2 Runtime 或者重新安装自带 WebView2 Runtime 的应用程序版本。", L"错误",
                       MB_OK | MB_ICONERROR);
            PostQuitMessage(0);
        }
    }

    void WebViewController::Resize(const RECT &bounds) const {
        if (webviewController != nullptr) {
            webviewController->put_Bounds(bounds);
        }
    }

    /**
     * @brief 设置WebView2的相关设置
     * @note 启用脚本、默认脚本对话框、Web消息、开发者工具等
     */
    void WebViewController::SetupWebViewSettings() const {
        if (!webview) {
            return;
        }
        wil::com_ptr<ICoreWebView2Settings> settings;
        webview->get_Settings(&settings);
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreDevToolsEnabled(TRUE);
        SPDLOG_INFO("WebView2 DevTools 已启用");

        webview->AddScriptToExecuteOnDocumentCreated(
            wil::make_bstr(Utils::LoadWStringFromResource(304, 305).c_str()).get(), nullptr);
        webview->AddScriptToExecuteOnDocumentCreated(L"Object.freeze(Object);", nullptr);
    }

    /**
     * @brief 注册WebView2的消息处理函数
     * @note 当WebView2接收到消息时，会调用此函数处理消息
     */
    void WebViewController::RegisterMessageHandler() const {
        if (!webview) {
            return;
        }
        EventRegistrationToken token;
        webview->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [this](ICoreWebView2 *webview, ICoreWebView2WebMessageReceivedEventArgs *receivedEventArgs) -> HRESULT {
                    wil::unique_cotaskmem_string message;
                    receivedEventArgs->get_WebMessageAsJson(&message);
                    nlohmann::json msg = nlohmann::json::parse(Utils::WStringToString(message.get()));
                    const std::string id = msg.value("id", "");
                    const std::string cmd = msg.value("cmd", "");
                    const nlohmann::json args = msg.contains("args") ? msg["args"] : nlohmann::json(nullptr);

                    const auto logger = spdlog::get("spdlog");
                    logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::trace,
                                "收到invoke: {}", msg.dump(2));

                    ProcessMessage(id, cmd, args, [webview](const nlohmann::json &response) {
                        webview->PostWebMessageAsJson(Utils::StringToWString(response.dump()).c_str());
                    });

                    return S_OK;
                })
            .Get(),
            &token);
    }

    /**
     * @brief 获取用户数据文件夹路径
     * @return std::wstring 用户数据文件夹路径
     * @note 从环境变量中获取用户数据文件夹路径，若不存在则创建
     */
    std::wstring WebViewController::GetUserDataFolder() {
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
            std::wstring folder(path);
            CoTaskMemFree(path);
            folder += L"\\";
            folder += APP_IDENTIFIER;
            CreateDirectoryW(folder.c_str(), nullptr);
            return folder;
        }
        return L"";
    }

    /**
     * @brief 获取WebView2运行时路径
     * @return std::wstring WebView2运行时路径
     * @note 从可执行文件路径中获取WebView2运行时路径
     */
    std::wstring WebViewController::GetWebView2RuntimePath() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);

        std::filesystem::path exeDir = exePath;
        exeDir = exeDir.parent_path();

        if (const std::filesystem::path runtimePath = exeDir / WEBVIEW2_RUNTIME_PATH;
            std::filesystem::exists(runtimePath)) {
            return runtimePath.wstring();
        }
        return L"";
    }

    /**
     * @brief 加载应用程序
     * @note 导航到指定的URL，URL格式为"http://localhost:端口号"
     */
    void WebViewController::LoadApplication() const {
        if (!webview) {
            return;
        }
        std::wstringstream url;
        url << L"http://localhost:" << port;
        webview->Navigate(url.str().c_str());
    }

    /**
     * @brief 处理消息
     * @param id 消息ID
     * @param cmd 消息命令
     * @param args 消息参数
     * @param callback 消息回调函数
     * @note 用于处理WebView2发送的消息，消息格式为JSON字符串
     */
    void WebViewController::ProcessMessage(const std::string &id, const std::string &cmd, const nlohmann::json &args,
                                           const std::function<void(const nlohmann::json &)> &callback) const {
        if (cmd == "quit") {
            callback(ResultResponse(id, 10000, "操作成功", nullptr));
            PostQuitMessage(0);
        }
        if (cmd == "getWindows") {
            const std::vector<WindowInfo> windows = WindowManager::GetTaskbarWindows();

            nlohmann::json data;
            data["windows"] = nlohmann::json::array();
            for (const auto &info : windows) {
                nlohmann::json windowJson;
                if (std::string title = Utils::WStringToString(info.title); !title.empty()) {
                    windowJson["title"] = title;
                } else {
                    windowJson["title"] = "(无标题)";
                }
                windowJson["handle"] = Utils::HWndToHexString(info.hWnd);
                data["windows"].push_back(windowJson);
            }
            callback(ResultResponse(id, 10000, "查询成功", data));
        }
        if (cmd == "activateWindow") {
            const std::string handle = args.value("handle", "");
            WindowManager::ActivateWindow(handle);
            callback(ResultResponse(id, 10000, "操作成功", nullptr));
        }
        if (cmd == "registerHotkey") {
            const nlohmann::json hotkey =
                args.contains("hotkey") ? args["hotkey"] : nlohmann::json(nullptr);
            const bool ctrl = hotkey.value("ctrl", false);
            const bool shift = hotkey.value("shift", false);
            const bool alt = hotkey.value("alt", false);
            const std::string key = hotkey.value("key", "");

            if (auto ghm = globalHotKeyManager.lock()) {
                HotKeyRegistrationResult result = ghm->RegisterGlobalHotKey(ctrl, shift, alt, key, [this] {
                    ShowWindow(this->hWnd, SW_RESTORE);
                    SetForegroundWindow(this->hWnd);
                });

                if (result.Success()) {
                    callback(ResultResponse(id, 10000, "操作成功", nullptr));
                } else {
                    // 发送详细的错误信息给webview2
                    nlohmann::json errorData = {{"errorCode", result.errorCode},
                                                {"errorMessage", result.errorMessage}};
                    callback(ResultResponse(id, 20000, result.errorMessage, errorData));
                }
            } else {
                callback(ResultResponse(id, 20000, "全局热键管理器不可用", nullptr));
            }
        }
        if (cmd == "clearHotkey") {
            if (auto ghm = globalHotKeyManager.lock()) {
                ghm->UnregisterAll();
                callback(ResultResponse(id, 10000, "操作成功", nullptr));
            }
        }
    }

    /**
     * @brief 构建结果响应
     * @param id 消息ID
     * @param code 响应码
     * @param msg 响应消息
     * @param data 响应数据
     * @return nlohmann::json 结果响应JSON对象
     * @note 用于构建WebView2发送的结果响应消息，消息格式为JSON字符串
     */
    nlohmann::json WebViewController::ResultResponse(const std::string &id, int code, const std::string &msg,
                                                     const nlohmann::json &data) const {
        const nlohmann::json result = {{"code", code}, {"msg", msg}, {"data", data}};
        const nlohmann::json payload = {{"id", id}, {"result", result}};
        return payload;
    }

    /**
     * @brief 构建事件响应
     * @param name 事件名称
     * @param data 事件数据
     * @return nlohmann::json 事件响应JSON对象
     * @note 用于构建WebView2发送的事件响应消息，消息格式为JSON字符串
     */
    nlohmann::json WebViewController::EmitResponse(const std::string &name, const nlohmann::json &data) const {
        const nlohmann::json payload = {{"event", name}, {"data", data}};
        return payload;
    }
} // namespace v1_taskbar_manager