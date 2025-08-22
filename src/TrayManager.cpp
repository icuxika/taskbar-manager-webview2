#include "TrayManager.h"

#include "Constants.h"

namespace v1_taskbar_manager {
    TrayManager::TrayManager(HWND hWnd): hWnd(hWnd) {
        ZeroMemory(&nid, sizeof(nid));
    }

    TrayManager::~TrayManager() {
        RemoveTrayIcon();
    }

    /**
     * @brief 添加任务栏图标
     * @note 调用Shell_NotifyIconW添加任务栏图标
     */
    void TrayManager::AddTrayIcon() {
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hWnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAY_ICON;
        nid.hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(301));
        lstrcpyW(nid.szTip, L"Windows 任务栏窗口管理");
        Shell_NotifyIconW(NIM_ADD, &nid);
    }

    /**
     * @brief 移除任务栏图标
     * @note 调用Shell_NotifyIconW移除任务栏图标
     */
    void TrayManager::RemoveTrayIcon() {
        Shell_NotifyIconW(NIM_DELETE, &nid);
    }

    /**
     * @brief 处理任务栏图标消息
     * @param wParam 消息参数
     * @param lParam 消息参数
     * @return LRESULT 消息处理结果
     * @note 处理任务栏图标消息，包括左键点击和右键点击
     */
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
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_ENABLE_HOTKEY, L"注册全局快捷键 Ctrl+Alt+T");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_DISABLE_HOTKEY, L"取消全局快捷键 Ctrl+Alt+T");

            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
            DestroyMenu(hMenu);
        }
        return DefWindowProc(hWnd, WM_USER, wParam, lParam);
    }
}
