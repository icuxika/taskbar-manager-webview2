#include "GlobalHotKeyManager.h"

namespace v1_taskbar_manager {
    GlobalHotKeyManager::GlobalHotKeyManager(HWND hWnd): hWnd(hWnd), nextId(1) {
    }

    GlobalHotKeyManager::~GlobalHotKeyManager() {
        UnregisterAll();
    }

    /**
     * @brief 注册全局热键
     * @param vkCode 虚拟键码
     * @param modifiers 修饰键
     * @param callback 回调函数
     * @return int 热键ID
     * @note 注册全局热键，当热键被按下时，会调用回调函数
     */
    int GlobalHotKeyManager::RegisterGlobalHotKey(UINT vkCode, UINT modifiers, std::function<void()> callback) {
        int id = nextId++;

        if (RegisterHotKey(hWnd, id, modifiers, vkCode)) {
            callbacks[id] = callback;
            return id;
        }

        // 注册失败，输出错误信息
        const DWORD error = GetLastError();
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

    /**
     * @brief 注册全局热键
     * @param ctrl 是否包含Ctrl键
     * @param shift 是否包含Shift键
     * @param alt 是否包含Alt键
     * @param key 热键的键位
     * @param callback 回调函数
     * @return int 热键ID
     * @note 注册全局热键，当热键被按下时，会调用回调函数
     */
    int GlobalHotKeyManager::RegisterGlobalHotKey(const bool ctrl, const bool shift, const bool alt,
                                                  const std::string &key,
                                                  std::function<void()> callback) {
        UINT modifiers = 0;
        if (ctrl)
            modifiers |= MOD_CONTROL;
        if (shift)
            modifiers |= MOD_SHIFT;
        if (alt)
            modifiers |= MOD_ALT;

        const UINT vk = GetVirtualKeyCode(key);
        return RegisterGlobalHotKey(vk, modifiers, std::move(callback));
    }

    /**
     * @brief 注销全局热键
     * @param id 热键ID
     * @return bool 是否注销成功
     * @note 注销全局热键，释放资源
     */
    bool GlobalHotKeyManager::UnregisterHotKey(int id) {
        if (callbacks.find(id) != callbacks.end()) {
            if (::UnregisterHotKey(hWnd, id)) {
                callbacks.erase(id);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 注销所有全局热键
     * @note 注销所有已注册的全局热键，释放资源
     */
    void GlobalHotKeyManager::UnregisterAll() {
        for (auto &pair : callbacks) {
            ::UnregisterHotKey(hWnd, pair.first);
        }
        callbacks.clear();
    }

    /**
     * @brief 检查热键是否可用
     * @param vkCode 虚拟键码
     * @param modifiers 修饰键
     * @return bool 是否可用
     * @note 检查热键是否可用，通过尝试注册热键并立即注销来判断
     */
    bool GlobalHotKeyManager::IsHotKeyAvailable(UINT vkCode, UINT modifiers) {
        int tempId = 9999; // 使用一个临时ID
        bool available = RegisterHotKey(hWnd, tempId, modifiers, vkCode);
        if (available) {
            UnregisterHotKey(tempId); // 立即注销
        }
        return available;
    }

    /**
     * @brief 获取最后一个错误信息
     * @return std::wstring 错误信息
     * @note 获取最近一次操作的错误信息，用于调试
     */
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

    /**
     * @brief 注册全局热键（带回退）
     * @param keyOptions 热键选项列表
     * @param callback 回调函数
     * @return int 热键ID
     * @note 尝试注册多个热键选项，返回第一个成功注册的热键ID
     */
    int GlobalHotKeyManager::RegisterHotKeyWithFallback(const std::vector<std::pair<UINT, UINT> > &keyOptions,
                                                        std::function<void()> callback) {
        for (const auto &option : keyOptions) {
            int result = RegisterGlobalHotKey(option.first, option.second, callback);
            if (result != -1) {
                return result; // 成功注册
            }
        }
        return -1; // 所有选项都失败
    }

    /**
     * @brief 处理热键消息
     * @param wParam 热键ID
     * @return bool 是否处理成功
     * @note 当热键被按下时，会调用此函数处理消息
     */
    bool GlobalHotKeyManager::HandleHotKeyMessage(WPARAM wParam) {
        const int id = static_cast<int>(wParam);
        if (const auto it = callbacks.find(id); it != callbacks.end()) {
            it->second(); // 调用回调函数
            return true;
        }
        return false;
    }

    /**
     * @brief 获取虚拟键码
     * @param key 键位字符串
     * @return UINT 虚拟键码
     * @note 将键位字符串转换为虚拟键码
     */
    UINT GlobalHotKeyManager::GetVirtualKeyCode(const std::string &key) {
        static std::unordered_map<std::string, UINT> keyMap = {
            {"A", 'A'}, {"B", 'B'}, {"C", 'C'}, {"D", 'D'},
            {"E", 'E'}, {"F", 'F'}, {"G", 'G'}, {"H", 'H'},
            {"I", 'I'}, {"J", 'J'}, {"K", 'K'}, {"L", 'L'},
            {"M", 'M'}, {"N", 'N'}, {"O", 'O'}, {"P", 'P'},
            {"Q", 'Q'}, {"R", 'R'}, {"S", 'S'}, {"T", 'T'},
            {"U", 'U'}, {"V", 'V'}, {"W", 'W'}, {"X", 'X'},
            {"Y", 'Y'}, {"Z", 'Z'},
            {"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4},
            {"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8},
            {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},
            {"ESC", VK_ESCAPE}, {"TAB", VK_TAB}, {"SPACE", VK_SPACE},
            {"ENTER", VK_RETURN}, {"UP", VK_UP}, {"DOWN", VK_DOWN},
            {"LEFT", VK_LEFT}, {"RIGHT", VK_RIGHT}
        };

        if (const auto it = keyMap.find(key); it != keyMap.end())
            return it->second;
        return 0;
    }
}