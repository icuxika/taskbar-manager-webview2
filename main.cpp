#include <windows.h>
#include <string>
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
static TCHAR szWindowClass[] = _T("DesktopApp");

// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T("WebView sample");

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
    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    lstrcpyW(nid.szTip, L"WebView2 示例应用");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
) {
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

    HWND hWnd = CreateWindow(
        szWindowClass,
        szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1200, 900,
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
                                                                             webview->PostWebMessageAsString(
                                                                                 message.get());
                                                                             return S_OK;
                                                                         }).Get(), &token);

                                                                 webview->AddScriptToExecuteOnDocumentCreated(
                                                                     L"window.chrome.webview.addEventListener(\'message\', event => alert(event.data));"
                                                                     L"window.chrome.webview.postMessage(window.document.URL);",
                                                                     nullptr);

                                                                 webview->NavigateToString(LR"(
<!DOCTYPE html>
    <html>
    <head>
        <title>Hello WebView2</title>
        <style>
            body { font-family: sans-serif; background-color: #f0f0f0; padding: 20px; }
        </style>
    </head>
    <body>
        <h1>你好，WebView2！</h1>
        <p>这是一段从 C++ 传入的 HTML 字符串。</p>
    </body>
    </html>
)");

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
                    MessageBoxW(hWnd, L"WebView2 示例程序\nBy ChatGPT", L"关于", MB_ICONINFORMATION);
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
