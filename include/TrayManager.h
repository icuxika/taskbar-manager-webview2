#pragma once
#include <windows.h>
#include <shellapi.h>

namespace v1_taskbar_manager {
    class TrayManager {
    public:
        explicit TrayManager(HWND hWnd);

        ~TrayManager();

        void AddTrayIcon();

        void RemoveTrayIcon();

        LRESULT HandleTrayMessage(WPARAM wParam, LPARAM lParam) const;

    private:
        HWND hWnd;
        NOTIFYICONDATAW nid;
    };
}
