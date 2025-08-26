#include "Application.h"

#include <dwmapi.h>
#include <future>
#include <iostream>
#include <sstream>

#include "Constants.h"
#include "Utils.h"
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LOGGER_TRACE
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

namespace v1_taskbar_manager {
    Application &Application::GetInstance() {
        static Application instance;
        return instance;
    }

    /**
     * @brief 运行应用程序
     * @param hInstance 实例句柄
     * @param nCmdShow 命令显示参数
     * @return int 应用程序退出码
     * @note 应用程序的入口点，初始化应用程序并运行消息循环
     */
    int Application::Run(HINSTANCE hInstance, int nCmdShow) {
        int nArgs;
        LPWSTR *szArgList = CommandLineToArgvW(GetCommandLineW(), &nArgs);
        if (szArgList == nullptr) {
            return -1;
        }
        std::wstring parameters;
        if (nArgs > 1) {
            for (int i = 1; i < nArgs; i++) {
                parameters += szArgList[i];
            }
        }
        LocalFree(szArgList);
        std::wcout << parameters << std::endl;
        if (parameters.find(L"--console") != std::wstring::npos) {
            Utils::CreateConsole();
        }

        SetupSpdlog();

        SetupDPI();

        if (!Utils::IsRunningAsAdmin()) {
            Utils::RelaunchAsAdmin();
            return 0;
        }

        if (Utils::IsAlreadyRunning(L"TaskbarManagerWebview2", mutex)) {
            MessageBox(nullptr, L"程序已经在运行", L"错误", MB_OK | MB_ICONERROR);
            if (HWND hWnd = FindWindow(szWindowClass, szTitle)) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            }
            return 0;
        }

        Initialize(hInstance);
        if (!RegisterWindowClass(hInstance)) {
            MessageBox(nullptr, L"Failed to register window class!", L"Error", MB_ICONERROR);
            return 1;
        }

        int port = StartHttpServerAsync();
        SPDLOG_INFO("本地 Socket 服务端口: {}", port);

        this->hWnd = CreateMainWindow(hInstance, nCmdShow);
        if (!hWnd) {
            MessageBox(nullptr, L"Failed to create window!", L"Error", MB_ICONERROR);
            return 1;
        }

        this->globalHotKeyManager = std::make_shared<GlobalHotKeyManager>(hWnd);
        this->trayManager = std::make_unique<TrayManager>(hWnd);
        this->webViewController = std::make_unique<WebViewController>(hWnd, this->globalHotKeyManager, port);

        // 设置窗口圆角
        constexpr DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
        DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

        trayManager->AddTrayIcon();
        webViewController->Initialize();

        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);

        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        Cleanup();
        return static_cast<int>(msg.wParam);
    }

    /**
     * @brief 窗口过程
     * @param hWnd 窗口句柄
     * @param message 消息类型
     * @param wParam 消息参数1
     * @param lParam 消息参数2
     * @return LRESULT 消息处理结果
     * @note 处理窗口消息，包括热键消息
     */
    LRESULT Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_SIZE:
            if (this->webViewController != nullptr) {
                RECT bounds;
                GetClientRect(hWnd, &bounds);
                this->webViewController->Resize(bounds);
            };
            break;
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                HWND hNewActive = (HWND)lParam;
                WCHAR className[256] = {};
                if (hNewActive)
                    GetClassName(hNewActive, className, 255);

                if (hNewActive == nullptr ||
                    (GetParent(hNewActive) != hWnd && wcscmp(className, L"Chrome_WidgetWin_1") != 0)) {
                    ShowWindow(hWnd, SW_HIDE); // 隐藏到托盘
                }
            }
            break;
        case WM_TRAY_ICON: {
            if (this->trayManager) {
                return this->trayManager->HandleTrayMessage(wParam, lParam);
            }
        }
        break;
        case WM_CLOSE: {
            const int result =
                MessageBox(hWnd, L"是否退出程序？\n点击“否”将最小化到托盘。", L"退出确认", MB_ICONQUESTION | MB_YESNO);

            if (result == IDYES) {
                PostQuitMessage(0); // 真正退出
            } else {
                ShowWindow(hWnd, SW_HIDE); // 否则只是隐藏窗口
            }
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
            case ID_TRAY_ABOUT:
                MessageBox(hWnd, L"Windows 任务栏窗口管理\nBy 浮木", L"关于", MB_ICONINFORMATION);
                break;
            case ID_TRAY_EXIT:
                PostQuitMessage(0);
                break;
            case ID_TRAY_ENABLE_HOTKEY: {
                HotKeyRegistrationResult result =
                    globalHotKeyManager->RegisterGlobalHotKey('T', MOD_CONTROL | MOD_ALT, [this]() {
                        ShowWindow(this->hWnd, SW_RESTORE);
                        SetForegroundWindow(this->hWnd);
                    });
                if (result.Success()) {
                    hotKeyId = result.id;
                    MessageBox(hWnd, L"已成功注册全局快捷键 Ctrl+Alt+T", L"全局快捷键", MB_ICONINFORMATION);
                } else {
                    MessageBox(hWnd, Utils::StringToWString(result.errorMessage).c_str(), L"注册热键失败",
                               MB_ICONERROR);
                }
                break;
            }
            case ID_TRAY_DISABLE_HOTKEY:
                if (hotKeyId != 0) {
                    if (globalHotKeyManager->UnregisterHotKey(hotKeyId)) {
                        MessageBox(hWnd, L"已成功取消全局快捷键 Ctrl+Alt+T", L"全局快捷键", MB_ICONINFORMATION);
                    }
                }
                break;
            case ID_TRAY_LOCAL_APP_DATA: {
                const std::wstring localAppData = Utils::GetLocalAppDataFolder();
                ShellExecute(nullptr, L"open", localAppData.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                break;
            }
            default: ;
            }
            break;
        case WM_HOTKEY:
            if (globalHotKeyManager) {
                globalHotKeyManager->HandleHotKeyMessage(wParam);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        return 0;
    }

    bool Application::Initialize(HINSTANCE hInstance) {
        this->hInstance = hInstance;
        return true;
    }

    /**
     * @brief 注册窗口类
     * @param hInstance 实例句柄
     * @return bool 是否注册成功
     * @note 注册窗口类，用于创建窗口
     */
    bool Application::RegisterWindowClass(HINSTANCE hInstance) {
        WNDCLASSEX wcex;

        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hInstance;
        wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = szWindowClass;
        wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

        return RegisterClassEx(&wcex);
    }

    /**
     * @brief 创建主窗口
     * @param hInstance 实例句柄
     * @param nCmdShow 命令显示参数
     * @return HWND 窗口句柄
     * @note 创建主窗口，设置窗口属性和位置
     */
    HWND Application::CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
        const int physicalWidth = GetSystemMetrics(SM_CXSCREEN);;
        const int physicalHeight = GetSystemMetrics(SM_CYSCREEN);
        const UINT dpi = GetDpiForSystem();
        const float scale = dpi / 96.0f;
        const int logicalWidth = static_cast<int>(physicalWidth / scale);
        const int logicalHeight = static_cast<int>(physicalHeight / scale);

        const int windowWidth = 480 * scale;
        const int windowHeight = (logicalHeight - 80) * scale;
        const int x = (logicalWidth - 480 - 16) * scale;
        const int y = 16 * scale;

        SPDLOG_INFO("Windows 物理像素: {}x{}", physicalWidth, physicalHeight);
        SPDLOG_INFO("Windows 逻辑像素: {}x{}", logicalWidth, logicalHeight);
        SPDLOG_INFO("程序窗口位置和大小: {}x{} @ {}x{}", windowWidth, windowHeight, x, y);

        return CreateWindowEx(WS_EX_TOOLWINDOW, szWindowClass, szTitle, WS_POPUP | WS_VISIBLE, x, y, windowWidth,
                              windowHeight, nullptr, nullptr, hInstance, nullptr);
    }

    /**
     * @brief 设置日志记录
     * @note 设置日志记录，将日志输出到控制台和文件
     */
    void Application::SetupSpdlog() {
        const std::wstring localAppData = Utils::GetLocalAppDataFolder();
        const std::wstring logsDir = localAppData + L"\\logs";
        CreateDirectory(logsDir.c_str(), nullptr);
        const std::wstring logFile = logsDir + L"\\log.txt";

        // trace,debug 输出到控制台，其他级别会被文本记录
        // 设置日志格式. 参数含义: [日志标识符] [日期] [日志级别] [线程号] [文件名:行号] [数据]
        const auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        console_sink->set_pattern("[%n] [%Y-%m-%d %H:%M:%S.%e] [%^---%L---%$] [%t] [%s:%#] %v");
        const auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            Utils::WStringToString(logFile), true);
        file_sink->set_level(spdlog::level::info);
        file_sink->set_pattern("[%n] [%Y-%m-%d %H:%M:%S.%e] [%^---%L---%$] [%t] [%s:%#] %v");

        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        const auto logger = std::make_shared<spdlog::logger>("spdlog", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::trace);
        spdlog::set_level(spdlog::level::trace);
        spdlog::set_default_logger(logger);
        spdlog::flush_every(std::chrono::seconds(3));
        SPDLOG_INFO("应用程序启动");
        SPDLOG_INFO("日志存储位置: {}", Utils::WStringToString(logFile));
    }

    void Application::SetupDPI() { SetProcessDPIAware(); }

    void Application::Cleanup() {
        webViewController.reset();
        trayManager.reset();
        globalHotKeyManager.reset();
        if (mutex) {
            CloseHandle(mutex);
        }
        StopHttpServer();
        SPDLOG_INFO("应用程序已正常退出");
        spdlog::shutdown();
    }

    /**
     * @brief 启动HTTP服务器异步
     * @return int 服务器端口号
     * @note 启动HTTP服务器，监听本地端口，返回服务器端口号
     */
    int Application::StartHttpServerAsync() {
        const std::wstring wStrHTML = Utils::LoadWStringFromResource(302, 303);
        const std::string html = Utils::WStringToString(wStrHTML);

        std::promise<int> portPromise;
        auto portFuture = portPromise.get_future();

        serverThread = std::thread([html, p = std::move(portPromise), this]() mutable {
            WSADATA wsaData;
            const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                p.set_value(-1);
                return;
            }

            serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (serverSocket == INVALID_SOCKET) {
                WSACleanup();
                p.set_value(-1);
                return;
            }

            constexpr int reuseAddr = 1;
            setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuseAddr),
                       sizeof(reuseAddr));

            int selectedPort = GetPreferredPort();

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(selectedPort);

            if (bind(serverSocket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == SOCKET_ERROR) {
                closesocket(serverSocket);
                WSACleanup();
                p.set_value(-1);
                return;
            }

            sockaddr_in actualAddr{};
            int len = sizeof(actualAddr);
            if (getsockname(serverSocket, reinterpret_cast<sockaddr *>(&actualAddr), &len) == SOCKET_ERROR) {
                closesocket(serverSocket);
                WSACleanup();
                p.set_value(-1);
                return;
            }
            int actualPort = ntohs(actualAddr.sin_port);

            if (listen(serverSocket, 5) == SOCKET_ERROR) {
                // 增加backlog到5
                closesocket(serverSocket);
                WSACleanup();
                p.set_value(-1);
                return;
            }

            Utils::SavePortToWindowsRegistry(actualPort);
            p.set_value(actualPort);

            while (!shouldStop.load()) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(serverSocket, &fds);

                timeval timeout = {0, 100000}; // 100ms超时

                if (int selectResult = select(0, &fds, nullptr, nullptr, &timeout);
                    selectResult > 0 && FD_ISSET(serverSocket, &fds)) {
                    if (SOCKET clientSocket = accept(serverSocket, nullptr, nullptr); clientSocket != INVALID_SOCKET) {
                        // 读取客户端请求
                        char buffer[4096];
                        if (const int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
                            bytesReceived > 0) {
                            buffer[bytesReceived] = '\0';
                        }

                        std::ostringstream response;
                        response << "HTTP/1.1 200 OK\r\n";
                        response << "Content-Type: text/html; charset=utf-8\r\n";
                        response << "Content-Length: " << html.size() << "\r\n";
                        response << "Connection: close\r\n";
                        response << "\r\n";
                        response << html;

                        std::string responseStr = response.str();

                        // 发送响应
                        int totalSent = 0;
                        int responseSize = static_cast<int>(responseStr.size());
                        while (totalSent < responseSize && !shouldStop.load()) {
                            int sent = send(clientSocket, responseStr.c_str() + totalSent, responseSize - totalSent, 0);
                            if (sent == SOCKET_ERROR) {
                                break;
                            }
                            totalSent += sent;
                        }
                        shutdown(clientSocket, SD_SEND);
                        closesocket(clientSocket);
                    }
                }
            }
            closesocket(serverSocket);
            WSACleanup();
        });
        return portFuture.get();
    }

    void Application::StopHttpServer() {
        shouldStop.store(true);
        if (serverSocket != INVALID_SOCKET) {
            closesocket(serverSocket);
        }
        if (serverThread.joinable()) {
            serverThread.join();
        }
    }

    /**
     * @brief 获取首选端口
     * @return int 端口号
     * @note 从Windows注册表中读取端口号，如果端口号不可用，则返回0
     */
    int Application::GetPreferredPort() {
        const int savedPort = Utils::ReadPortFromWindowsRegistry();
        if (savedPort > 0) {
            if (Utils::IsPortAvailable(savedPort)) {
                return savedPort;
            }
            SPDLOG_INFO("程序上次使用的端口不再可用，将使用系统分配的新端口");
        }
        return 0;
    }
} // namespace v1_taskbar_manager

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return v1_taskbar_manager::Application::GetInstance().WindowProc(hWnd, message, wParam, lParam);
}