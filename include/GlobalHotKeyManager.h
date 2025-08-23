#pragma once
#include <windows.h>
#include <map>
#include <vector>
#include <functional>
#include <string>

namespace v1_taskbar_manager {
    class GlobalHotKeyManager {
    public:
        explicit GlobalHotKeyManager(HWND hWnd);

        ~GlobalHotKeyManager();

        int RegisterGlobalHotKey(UINT vkCode, UINT modifiers, std::function<void()> callback);

        int RegisterGlobalHotKey(bool ctrl, bool shift, bool alt, const std::string &key,
                                 std::function<void()> callback);

        bool UnregisterHotKey(int id);

        void UnregisterAll();

        bool IsHotKeyAvailable(UINT vkCode, UINT modifiers);

        static std::wstring GetLastErrorString();

        int RegisterHotKeyWithFallback(const std::vector<std::pair<UINT, UINT> > &keyOptions,
                                       std::function<void()> callback);

        bool HandleHotKeyMessage(WPARAM wParam);

    private:
        HWND hWnd;
        std::map<int, std::function<void()> > callbacks;
        int nextId;

        static UINT GetVirtualKeyCode(const std::string &key);
    };
}