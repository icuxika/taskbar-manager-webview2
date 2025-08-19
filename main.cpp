#include <codecvt>
#include <iostream>
#include <sstream>
#include <windows.h>
#include <string>
#include <vector>
#include <dwmapi.h>
#include <json.hpp>
#include <tchar.h>
#include <wrl.h>
#include <wil/com.h>
#include "WebView2.h"
#define WM_TRAY_ICON (WM_USER + 1)
NOTIFYICONDATAW nid = {};

using namespace Microsoft::WRL;

#define ID_TRAY_ABOUT 1001
#define ID_TRAY_EXIT  1002

// The main window class name.
static TCHAR szWindowClass[] = _T("TaskbarManagerClass");

// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T("TaskbarManager");

HINSTANCE hInst;

// Forward declarations of functions included in this code module:
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Pointer to WebViewController
static wil::com_ptr<ICoreWebView2Controller> webviewController;

// Pointer to WebView window
static wil::com_ptr<ICoreWebView2> webview;

void AddTrayIcon(HWND hWnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY_ICON;
    // nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    nid.hIcon = static_cast<HICON>(LoadImage(nullptr, "icon.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE));
    lstrcpyW(nid.szTip, L"Windows 任务栏窗口管理");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void CreateConsole() {
    // 分配控制台
    AllocConsole();

    // 重定向标准输出到控制台
    FILE *pCout;
    freopen_s(&pCout, "CONOUT$", "w", stdout);

    // 重定向标准输入到控制台
    FILE *pCin;
    freopen_s(&pCin, "CONIN$", "r", stdin);

    // 重定向标准错误到控制台
    FILE *pCerr;
    freopen_s(&pCerr, "CONOUT$", "w", stderr);

    // 设置控制台窗口标题
    SetConsoleTitleW(L"WebView2 Debug Console");

    std::wcout << L"=== WebView2 Debug Console Started ===" << std::endl;
}

std::wstring GetExeDirectory() {
    wchar_t buffer[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::wstring path(buffer);
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        path = path.substr(0, pos); // 取 exe 所在目录
    }
    return path;
}

struct WindowInfo {
    HWND hwnd;
    std::wstring title;
    std::wstring className;
    bool isVisible;
    bool isMinimized;
    bool isMaximized;
    DWORD processId;
    std::wstring processName;
};

std::wstring GetProcessName(DWORD processId) {
    const HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                        FALSE, processId);
    if (hProcess == nullptr) {
        return L"Unknown";
    }

    wchar_t processName[MAX_PATH];
    DWORD size = MAX_PATH;

    if (QueryFullProcessImageNameW(hProcess, 0, processName, &size)) {
        std::wstring fullPath(processName);
        size_t pos = fullPath.find_last_of(L'\\');
        if (pos != std::wstring::npos) {
            CloseHandle(hProcess);
            return fullPath.substr(pos + 1);
        }
    }

    CloseHandle(hProcess);
    return L"Unknown";
}

// 检查窗口是否应该在任务栏显示
bool ShouldShowInTaskbar(HWND hwnd) {
    // 检查窗口是否可见
    if (!IsWindowVisible(hwnd)) {
        return false;
    }

    // 获取窗口样式
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

    // 排除工具窗口（除非有 WS_EX_APPWINDOW 样式）
    if ((exStyle & WS_EX_TOOLWINDOW) && !(exStyle & WS_EX_APPWINDOW)) {
        return false;
    }

    // 检查是否有父窗口（排除子窗口）
    HWND parent = GetParent(hwnd);
    if (parent != nullptr && parent != GetDesktopWindow()) {
        return false;
    }

    // 检查窗口是否有标题
    int titleLength = GetWindowTextLengthW(hwnd);

    // 如果有 WS_EX_APPWINDOW 样式，即使没有标题也显示
    if (exStyle & WS_EX_APPWINDOW) {
        return true;
    }

    // 必须有标题才显示
    if (titleLength == 0) {
        return false;
    }

    // 排除一些特殊的窗口类
    wchar_t className[256];
    GetClassNameW(hwnd, className, sizeof(className) / sizeof(wchar_t));
    std::wstring classNameStr(className);

    // 排除一些系统窗口类
    if (classNameStr == L"Windows.UI.Core.CoreWindow" ||
        classNameStr == L"ApplicationFrameWindow") {
        return false;
    }

    return true;
}

// 窗口枚举回调函数
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    std::vector<WindowInfo> *windows =
            reinterpret_cast<std::vector<WindowInfo> *>(lParam);

    // 检查是否应该在任务栏显示
    if (!ShouldShowInTaskbar(hwnd)) {
        return TRUE; // 继续枚举
    }

    WindowInfo info;
    info.hwnd = hwnd;

    // 获取窗口标题
    int titleLength = GetWindowTextLengthW(hwnd);
    if (titleLength > 0) {
        std::vector<wchar_t> titleBuffer(titleLength + 1);
        GetWindowTextW(hwnd, titleBuffer.data(), titleLength + 1);
        info.title = std::wstring(titleBuffer.data());
    } else {
        info.title = L"(无标题)";
    }

    // 获取窗口类名
    wchar_t className[256];
    if (GetClassNameW(hwnd, className, sizeof(className) / sizeof(wchar_t))) {
        info.className = std::wstring(className);
    }

    // 获取窗口状态
    info.isVisible = IsWindowVisible(hwnd);
    info.isMinimized = IsIconic(hwnd);
    info.isMaximized = IsZoomed(hwnd);

    // 获取进程ID和进程名
    GetWindowThreadProcessId(hwnd, &info.processId);
    info.processName = GetProcessName(info.processId);

    windows->push_back(info);

    return TRUE; // 继续枚举
}

// 将宽字符串转换为多字节字符串（用于控制台输出）
std::string WStringToString(const std::wstring &wstr) {
    if (wstr.empty())
        return std::string();

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int) wstr.size(),
                                          nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int) wstr.size(), &strTo[0],
                        size_needed, nullptr, nullptr);
    return strTo;
}

// string(UTF-8) -> wstring
std::wstring ToWide(const std::string &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t> > conv;
    return conv.from_bytes(str);
}

std::string HwndToHexString(HWND hwnd) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase
            << reinterpret_cast<uintptr_t>(hwnd);
    return oss.str();
}

HWND HexStringToHwnd(const std::string &hexStr) {
    uintptr_t handle;
    std::istringstream iss(hexStr);
    iss >> std::hex >> handle;
    return reinterpret_cast<HWND>(handle);
}

void get_windows() {
    std::vector<WindowInfo> windows;
    if (!EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&windows))) {
        std::cerr << "枚举窗口失败！\n";
    }

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
        windowJson["handle"] = HwndToHexString(info.hwnd);
        result["windows"].push_back(windowJson);
    }
    webview->PostWebMessageAsString(ToWide(result.dump(2)).c_str());
}

void activate_window(std::wstring wHandle) {
    std::string handle = WStringToString(wHandle);
    HWND hwnd = HexStringToHwnd(handle);
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    keybd_event(VK_MENU, 0, 0, 0);
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);

    SetForegroundWindow(hwnd);
}

bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0,
                                 &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin;
}

void RelaunchAsAdmin() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    SHELLEXECUTEINFOW sei = {sizeof(sei)};
    sei.fMask = SEE_MASK_DEFAULT;
    sei.hwnd = nullptr;
    sei.lpVerb = L"runas"; // ← 请求管理员权限
    sei.lpFile = exePath; // 当前 exe 路径
    sei.lpParameters = L""; // 如果有命令行参数可以写这里
    sei.nShow = SW_NORMAL;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            std::wcout << L"用户取消了提升权限" << std::endl;
        }
    }
}

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
) {
    if (!IsRunningAsAdmin()) {
        RelaunchAsAdmin();
        return 0; // 原进程退出
    }

    SetProcessDPIAware();

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

    if (!RegisterClassEx(&wcex)) {
        MessageBox(nullptr,
                   _T("Call to RegisterClassEx failed!"),
                   _T("Windows Desktop Guided Tour"),
                   0);
        return 1;
    }

    hInst = hInstance;

    // CreateConsole();

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

    std::cout << "physicalWidth: " << physicalWidth << std::endl;
    std::cout << "physicalHeight: " << physicalHeight << std::endl;
    std::cout << "logicalWidth: " << logicalWidth << std::endl;
    std::cout << "logicalHeight: " << logicalHeight << std::endl;

    HWND hWnd = CreateWindowEx(
        WS_EX_TOOLWINDOW,
        szWindowClass,
        szTitle,
        WS_POPUP | WS_VISIBLE,
        x, y,
        windowWidth, windowHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hWnd) {
        MessageBox(nullptr,
                   _T("Call to CreateWindow failed!"),
                   _T("Windows Desktop Guided Tour"),
                   0);
        return 1;
    }

    constexpr DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

    ShowWindow(hWnd,
               nCmdShow);
    UpdateWindow(hWnd);

    CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
                                             Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                                                 [hWnd](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
                                                     // Create a CoreWebView2Controller and get the associated CoreWebView2 whose parent is the main window hWnd
                                                     env->CreateCoreWebView2Controller(
                                                         hWnd, Callback<
                                                             ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                                                             [hWnd](HRESULT result,
                                                                    ICoreWebView2Controller *controller) -> HRESULT {
                                                                 if (controller != nullptr) {
                                                                     webviewController = controller;
                                                                     webviewController->get_CoreWebView2(&webview);
                                                                 }

                                                                 wil::com_ptr<ICoreWebView2Settings> settings;
                                                                 webview->get_Settings(&settings);
                                                                 settings->put_IsScriptEnabled(TRUE);
                                                                 settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                                                                 settings->put_IsWebMessageEnabled(TRUE);

                                                                 RECT bounds;
                                                                 GetClientRect(hWnd, &bounds);
                                                                 webviewController->put_Bounds(bounds);

                                                                 EventRegistrationToken token;

                                                                 webview->AddScriptToExecuteOnDocumentCreated(
                                                                     L"Object.freeze(Object);", nullptr);

                                                                 webview->ExecuteScript(
                                                                     L"window.document.URL;",
                                                                     Callback<
                                                                         ICoreWebView2ExecuteScriptCompletedHandler>(
                                                                         [](HRESULT errorCode,
                                                                            LPCWSTR resultObjectAsJson) -> HRESULT {
                                                                             LPCWSTR URL = resultObjectAsJson;
                                                                             //doSomethingWithURL(URL);
                                                                             return S_OK;
                                                                         }).Get());

                                                                 webview->add_WebMessageReceived(
                                                                     Callback<
                                                                         ICoreWebView2WebMessageReceivedEventHandler>(
                                                                         [](ICoreWebView2 *webview,
                                                                            ICoreWebView2WebMessageReceivedEventArgs *
                                                                            args) -> HRESULT {
                                                                             wil::unique_cotaskmem_string message;
                                                                             args->TryGetWebMessageAsString(&message);
                                                                             // processMessage(&message);
                                                                             std::wstring msg = message.get();
                                                                             if (msg == L"quit") {
                                                                                 Shell_NotifyIconW(NIM_DELETE, &nid);
                                                                                 PostQuitMessage(0);
                                                                             }
                                                                             if (msg == L"getWindows") {
                                                                                 get_windows();
                                                                             }
                                                                             if (msg.rfind(L"activateWindow|", 0) ==
                                                                                 0) {
                                                                                 std::wcout << L"message1: " << msg <<
                                                                                         std::endl;
                                                                                 size_t pos = msg.find(L'|');
                                                                                 if (pos != std::wstring::npos && pos +
                                                                                     1 < msg.size()) {
                                                                                     std::wstring content = msg.substr(
                                                                                         pos + 1);
                                                                                     std::wcout << L"message2: " <<
                                                                                             content <<
                                                                                             std::endl;
                                                                                     activate_window(content);
                                                                                 }
                                                                             }
                                                                             return S_OK;
                                                                         }).Get(), &token);

                                                                 webview->AddScriptToExecuteOnDocumentCreated(
                                                                     L"window.chrome.webview.postMessage(window.document.URL);",
                                                                     nullptr);

                                                                 std::wstring exeDir = GetExeDirectory();
                                                                 std::wstring htmlPath = exeDir + L"/index.html";
                                                                 std::wstring url = L"file:///" + htmlPath;
                                                                 webview->Navigate(url.c_str());
                                                                 // webview->Navigate(
                                                                 // L"file:///C:/Users/icuxika/CLionProjects/webview2-win32/index.html");

                                                                 return S_OK;
                                                             }).Get());
                                                     return S_OK;
                                                 }).Get());
    AddTrayIcon(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_SIZE:
            if (webviewController != nullptr) {
                RECT bounds;
                GetClientRect(hWnd, &bounds);
                webviewController->put_Bounds(bounds);
            };
            break;
        // case WM_KILLFOCUS: {
        //     HWND hNewFocusWnd = reinterpret_cast<HWND>(wParam);
        //     WCHAR className[256] = {0};
        //     GetClassNameW(hNewFocusWnd, className, 255);
        //     bool x = hNewFocusWnd == nullptr;
        //     bool y = GetParent(hNewFocusWnd) != hWnd;
        //     std::cout << "hNewFocusWnd == nullptr: " << x << std::endl;
        //     std::cout << "GetParent(hNewFocusWnd) != hWnd: " << y << std::endl;
        //     std::wcout << L"className: " << className << std::endl;
        //
        //     if ((hNewFocusWnd == nullptr || GetParent(hNewFocusWnd) != hWnd) &&
        //         wcscmp(className, L"Chrome_WidgetWin_1") != 0) {
        //         ShowWindow(hWnd, SW_HIDE);
        //     }
        //     break;
        // }
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                HWND hNewActive = (HWND) lParam;
                WCHAR className[256] = {};
                if (hNewActive)
                    GetClassNameW(hNewActive, className, 255);

                std::wcout << L"WM_ACTIVATE lost focus to: " << className << std::endl;

                if (hNewActive == nullptr ||
                    (GetParent(hNewActive) != hWnd &&
                     wcscmp(className, L"Chrome_WidgetWin_1") != 0)) {
                    ShowWindow(hWnd, SW_HIDE); // 隐藏到托盘
                }
            }
            break;
        case WM_TRAY_ICON: {
            if (lParam == WM_LBUTTONUP) {
                // 单击左键显示窗口
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            } else if (lParam == WM_RBUTTONUP) {
                // 右键点击弹出菜单
                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_ABOUT, L"关于");
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出");

                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hWnd);
                TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
                DestroyMenu(hMenu);
            }
        }
        break;
        case WM_CLOSE: {
            const int result = MessageBoxW(hWnd,
                                           L"是否退出程序？\n点击“否”将最小化到托盘。",
                                           L"退出确认",
                                           MB_ICONQUESTION | MB_YESNO);

            if (result == IDYES) {
                Shell_NotifyIconW(NIM_DELETE, &nid); // 删除托盘图标
                PostQuitMessage(0); // 真正退出
            } else {
                ShowWindow(hWnd, SW_HIDE); // 否则只是隐藏窗口
            }
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_TRAY_ABOUT:
                    MessageBoxW(hWnd, L"Windows 任务栏窗口管理\nBy 浮木", L"关于", MB_ICONINFORMATION);
                    break;
                case ID_TRAY_EXIT:
                    Shell_NotifyIconW(NIM_DELETE, &nid); // 删除托盘图标
                    PostQuitMessage(0);
                    break;
                default: ;
            }
            break;
        case WM_DESTROY:
            Shell_NotifyIconW(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
