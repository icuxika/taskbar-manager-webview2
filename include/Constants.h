#pragma once
#include <tchar.h>
#include <windows.h>

// 消息定义
#define WM_TRAY_ICON (WM_USER + 1)

// 托盘菜单ID
#define ID_TRAY_ABOUT 1001
#define ID_TRAY_EXIT  1002
#define ID_TRAY_ENABLE_HOTKEY 1003
#define ID_TRAY_DISABLE_HOTKEY 1004

// 窗口类名和标题
static TCHAR szWindowClass[] = _T("TaskbarManagerClass");
static TCHAR szTitle[] = _T("TaskbarManager");
