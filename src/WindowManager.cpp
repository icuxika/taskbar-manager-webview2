#include "WindowManager.h"

#include <iostream>

#include "Utils.h"

namespace v1_taskbar_manager {
    std::vector<WindowInfo> WindowManager::GetTaskbarWindows() {
        std::vector<WindowInfo> windows;
        if (!EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&windows))) {
            std::cerr << "枚举窗口失败！\n";
        }
        return windows;
    }

    void WindowManager::ActivateWindow(const std::string &handle) {
        HWND hWnd = Utils::HexStringToHWnd(handle);
        if (IsIconic(hWnd)) {
            ShowWindow(hWnd, SW_RESTORE);
        }
        keybd_event(VK_MENU, 0, 0, 0);
        keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);

        SetForegroundWindow(hWnd);
    }

    bool WindowManager::ShouldShowInTaskbar(HWND hWnd) {
        // 检查窗口是否可见
        if (!IsWindowVisible(hWnd)) {
            return false;
        }

        // 获取窗口样式
        LONG style = GetWindowLong(hWnd, GWL_STYLE);
        LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);

        // 排除工具窗口（除非有 WS_EX_APPWINDOW 样式）
        if ((exStyle & WS_EX_TOOLWINDOW) && !(exStyle & WS_EX_APPWINDOW)) {
            return false;
        }

        // 检查是否有父窗口（排除子窗口）
        HWND parent = GetParent(hWnd);
        if (parent != nullptr && parent != GetDesktopWindow()) {
            return false;
        }

        // 检查窗口是否有标题
        int titleLength = GetWindowTextLengthW(hWnd);

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
        GetClassNameW(hWnd, className, sizeof(className) / sizeof(wchar_t));
        std::wstring classNameStr(className);

        // 排除一些系统窗口类
        if (classNameStr == L"Windows.UI.Core.CoreWindow" ||
            classNameStr == L"ApplicationFrameWindow") {
            return false;
        }

        return true;
    }

    BOOL CALLBACK WindowManager::EnumWindowsProc(HWND hWnd, LPARAM lParam) {
        std::vector<WindowInfo> *windows =
                reinterpret_cast<std::vector<WindowInfo> *>(lParam);

        // 检查是否应该在任务栏显示
        if (!ShouldShowInTaskbar(hWnd)) {
            return TRUE; // 继续枚举
        }

        WindowInfo info;
        info.hWnd = hWnd;

        // 获取窗口标题
        int titleLength = GetWindowTextLengthW(hWnd);
        if (titleLength > 0) {
            std::vector<wchar_t> titleBuffer(titleLength + 1);
            GetWindowTextW(hWnd, titleBuffer.data(), titleLength + 1);
            info.title = std::wstring(titleBuffer.data());
        } else {
            info.title = L"(无标题)";
        }

        // 获取窗口类名
        wchar_t className[256];
        if (GetClassNameW(hWnd, className, sizeof(className) / sizeof(wchar_t))) {
            info.className = std::wstring(className);
        }

        // 获取窗口状态
        info.isVisible = IsWindowVisible(hWnd);
        info.isMinimized = IsIconic(hWnd);
        info.isMaximized = IsZoomed(hWnd);

        // 获取进程ID和进程名
        GetWindowThreadProcessId(hWnd, &info.processId);
        info.processName = Utils::GetProcessName(info.processId);

        windows->push_back(info);

        return TRUE;
    }
}
