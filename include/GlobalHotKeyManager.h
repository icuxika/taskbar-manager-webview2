#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <windows.h>

namespace v1_taskbar_manager {

    /**
     * @brief 热键注册结果
     * @note 包含热键ID和错误信息
     */
    struct HotKeyRegistrationResult {
        int id;                   // 热键ID，成功时为正数，失败时为-1
        std::string errorMessage; // 错误信息，成功时为空
        int errorCode;            // 错误代码，成功时为0

        HotKeyRegistrationResult(int id = -1, const std::string &message = "", int code = 0)
            : id(id), errorMessage(message), errorCode(code) {}

        bool Success() const { return id != -1; }
    };

    class GlobalHotKeyManager {
      public:
        explicit GlobalHotKeyManager(HWND hWnd);

        ~GlobalHotKeyManager();

        HotKeyRegistrationResult RegisterGlobalHotKey(UINT vkCode, UINT modifiers, std::function<void()> callback);

        HotKeyRegistrationResult RegisterGlobalHotKey(bool ctrl, bool shift, bool alt, const std::string &key,
                                                      std::function<void()> callback);

        bool UnregisterHotKey(int id);

        void UnregisterAll();

        bool IsHotKeyAvailable(UINT vkCode, UINT modifiers);

        static std::wstring GetLastErrorString();

        int RegisterHotKeyWithFallback(const std::vector<std::pair<UINT, UINT>> &keyOptions,
                                       std::function<void()> callback);

        bool HandleHotKeyMessage(WPARAM wParam);

      private:
        HWND hWnd;
        std::map<int, std::function<void()>> callbacks;
        int nextId;

        static UINT GetVirtualKeyCode(const std::string &key);
    };
} // namespace v1_taskbar_manager
