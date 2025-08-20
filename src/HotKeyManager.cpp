#include "GlobalHotKeyManager.h"

namespace v1_taskbar_manager {
    GlobalHotKeyManager::GlobalHotKeyManager(HWND hWnd): hWnd(hWnd), nextId(1) {
    }

    GlobalHotKeyManager::~GlobalHotKeyManager() {
        UnregisterAll();
    }

    int GlobalHotKeyManager::RegisterGlobalHotKey(UINT vkCode, UINT modifiers, std::function<void()> callback) {
        int id = nextId++;

        if (RegisterHotKey(hWnd, id, modifiers, vkCode)) {
            callbacks[id] = callback;
            return id;
        }

        // 注册失败，输出错误信息
        DWORD error = GetLastError();
        OutputDebugStringA("RegisterHotKey failed: ");

        switch (error) {
            case ERROR_HOTKEY_ALREADY_REGISTERED:
                OutputDebugStringA("热键已被其他程序注册\n");
                break;
            case ERROR_INVALID_PARAMETER:
                OutputDebugStringA("无效参数\n");
                break;
            case ERROR_ACCESS_DENIED:
                OutputDebugStringA("访问被拒绝\n");
                break;
            default:
                char buffer[256];
                sprintf_s(buffer, "未知错误，错误代码: %lu\n", error);
                OutputDebugStringA(buffer);
                break;
        }

        return -1; // 注册失败
    }

    bool GlobalHotKeyManager::UnregisterHotKey(int id) {
        if (callbacks.find(id) != callbacks.end()) {
            if (::UnregisterHotKey(hWnd, id)) {
                callbacks.erase(id);
                return true;
            }
        }
        return false;
    }

    void GlobalHotKeyManager::UnregisterAll() {
        for (auto &pair: callbacks) {
            ::UnregisterHotKey(hWnd, pair.first);
        }
        callbacks.clear();
    }

    bool GlobalHotKeyManager::IsHotKeyAvailable(UINT vkCode, UINT modifiers) {
        int tempId = 9999; // 使用一个临时ID
        bool available = RegisterHotKey(hWnd, tempId, modifiers, vkCode);
        if (available) {
            UnregisterHotKey(tempId); // 立即注销
        }
        return available;
    }

    std::wstring GlobalHotKeyManager::GetLastErrorString() {
        DWORD error = GetLastError();
        switch (error) {
            case ERROR_HOTKEY_ALREADY_REGISTERED:
                return L"热键已被其他程序或窗口注册";
            case ERROR_INVALID_PARAMETER:
                return L"无效的参数组合";
            case ERROR_ACCESS_DENIED:
                return L"访问被拒绝，可能需要管理员权限";
            case ERROR_HOTKEY_NOT_REGISTERED:
                return L"热键未注册";
            default:
                return L"未知错误，错误代码: " + std::to_wstring(error);
        }
    }

    int GlobalHotKeyManager::RegisterHotKeyWithFallback(const std::vector<std::pair<UINT, UINT> > &keyOptions,
                                                        std::function<void()> callback) {
        for (const auto &option: keyOptions) {
            int result = RegisterGlobalHotKey(option.first, option.second, callback);
            if (result != -1) {
                return result; // 成功注册
            }
        }
        return -1; // 所有选项都失败
    }

    bool GlobalHotKeyManager::HandleHotKeyMessage(WPARAM wParam) {
        const int id = static_cast<int>(wParam);
        if (const auto it = callbacks.find(id); it != callbacks.end()) {
            it->second(); // 调用回调函数
            return true;
        }
        return false;
    }
}
