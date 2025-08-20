#include "TrayManager.h"

#include "Constants.h"

namespace v1_taskbar_manager {
    TrayManager::TrayManager(HWND hWnd): hWnd(hWnd) {
        ZeroMemory(&nid, sizeof(nid));
    }

    TrayManager::~TrayManager() {
        RemoveTrayIcon();
    }

    void TrayManager::AddTrayIcon() {
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hWnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAY_ICON;
        nid.hIcon = static_cast<HICON>(LoadImage(nullptr, "icon.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE));
        lstrcpyW(nid.szTip, L"Windows 任务栏窗口管理");
        Shell_NotifyIconW(NIM_ADD, &nid);
    }

    void TrayManager::RemoveTrayIcon() {
        Shell_NotifyIconW(NIM_DELETE, &nid);
    }

    LRESULT TrayManager::HandleTrayMessage(WPARAM wParam, LPARAM lParam) {
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
        return DefWindowProc(hWnd, WM_USER, wParam, lParam);
    }
}
