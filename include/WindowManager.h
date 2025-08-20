#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace v1_taskbar_manager {
    struct WindowInfo {
        HWND hWnd;
        std::wstring title;
        std::wstring className;
        bool isVisible;
        bool isMinimized;
        bool isMaximized;
        DWORD processId;
        std::wstring processName;
    };

    class WindowManager {
    public:
        static std::vector<WindowInfo> GetTaskbarWindows();

        static void ActivateWindow(const std::string &handle);

    private:
        static bool ShouldShowInTaskbar(HWND hWnd);

        static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam);
    };
}
