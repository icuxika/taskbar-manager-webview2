#pragma once
#include <string_view>

// 消息定义
#define WM_TRAY_ICON (WM_USER + 1)

// 托盘菜单ID
#define ID_TRAY_ABOUT 1001
#define ID_TRAY_EXIT  1002
#define ID_TRAY_ENABLE_HOTKEY 1003
#define ID_TRAY_DISABLE_HOTKEY 1004
#define ID_TRAY_LOCAL_APP_DATA 1005

// 窗口类名和标题
inline constexpr auto szWindowClass = L"TaskbarManager";
inline constexpr auto szTitle = L"Windows 任务栏窗口管理器";

// 应用程序标识符
inline constexpr std::wstring_view APP_IDENTIFIER = L"TaskbarManager";

// 程序目录下如果包含了此文件夹，会优先使用此文件夹下的WebView2 Runtime
inline constexpr std::wstring_view WEBVIEW2_RUNTIME_PATH = L"webview2_runtime";