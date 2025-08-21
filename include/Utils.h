#pragma once
#include <string>
#include <windows.h>

namespace v1_taskbar_manager {
    class Utils {
    public:
        static std::wstring GetExeDirectory();

        static std::string WStringToString(const std::wstring &wStr);

        static std::wstring StringToWString(const std::string &str);

        static std::string HWndToHexString(HWND hWnd);

        static HWND HexStringToHWnd(const std::string &hexStr);

        static bool IsRunningAsAdmin();

        static void RelaunchAsAdmin();

        static std::wstring GetProcessName(DWORD processId);

        static void CreateConsole();

        static bool IsAlreadyRunning(const std::wstring &mutexName);
    };
}
