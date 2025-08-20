#include "Utils.h"

#include <codecvt>
#include <ios>
#include <iostream>
#include <sstream>

namespace v1_taskbar_manager {
    std::wstring Utils::GetExeDirectory() {
        wchar_t buffer[MAX_PATH] = {0};
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        std::wstring path(buffer);
        size_t pos = path.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            path = path.substr(0, pos); // 取 exe 所在目录
        }
        return path;
    }

    std::string Utils::WStringToString(const std::wstring &wStr) {
        if (wStr.empty())
            return std::string();

        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wStr[0], (int) wStr.size(),
                                              nullptr, 0, nullptr, nullptr);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wStr[0], (int) wStr.size(), &strTo[0],
                            size_needed, nullptr, nullptr);
        return strTo;
    }

    std::wstring Utils::StringToWString(const std::string &str) {
        std::wstring_convert<std::codecvt_utf8<wchar_t> > conv;
        return conv.from_bytes(str);
    }

    std::string Utils::HWndToHexString(HWND hWnd) {
        std::ostringstream oss;
        oss << "0x" << std::hex << std::uppercase
                << reinterpret_cast<uintptr_t>(hWnd);
        return oss.str();
    }

    HWND Utils::HexStringToHWnd(const std::string &hexStr) {
        uintptr_t handle;
        std::istringstream iss(hexStr);
        iss >> std::hex >> handle;
        return reinterpret_cast<HWND>(handle);
    }

    bool Utils::IsRunningAsAdmin() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = nullptr;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

        if (AllocateAndInitializeSid(&ntAuthority, 2,
                                     SECURITY_BUILTIN_DOMAIN_RID,
                                     DOMAIN_ALIAS_RID_ADMINS,
                                     0, 0, 0, 0, 0, 0,
                                     &adminGroup)) {
            CheckTokenMembership(nullptr, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }

        return isAdmin;
    }

    void Utils::RelaunchAsAdmin() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);

        SHELLEXECUTEINFOW sei = {sizeof(sei)};
        sei.fMask = SEE_MASK_DEFAULT;
        sei.hwnd = nullptr;
        sei.lpVerb = L"runas"; // ← 请求管理员权限
        sei.lpFile = exePath; // 当前 exe 路径
        sei.lpParameters = L""; // 如果有命令行参数可以写这里
        sei.nShow = SW_NORMAL;

        if (!ShellExecuteExW(&sei)) {
            DWORD err = GetLastError();
            if (err == ERROR_CANCELLED) {
                std::wcout << L"用户取消了提升权限" << std::endl;
            }
        }
    }

    std::wstring Utils::GetProcessName(DWORD processId) {
        const HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                            FALSE, processId);
        if (hProcess == nullptr) {
            return L"Unknown";
        }

        wchar_t processName[MAX_PATH];
        DWORD size = MAX_PATH;

        if (QueryFullProcessImageNameW(hProcess, 0, processName, &size)) {
            std::wstring fullPath(processName);
            size_t pos = fullPath.find_last_of(L'\\');
            if (pos != std::wstring::npos) {
                CloseHandle(hProcess);
                return fullPath.substr(pos + 1);
            }
        }

        CloseHandle(hProcess);
        return L"Unknown";
    }

    void Utils::CreateConsole() {
        // 分配控制台
        AllocConsole();

        // 重定向标准输出到控制台
        FILE *pCout;
        freopen_s(&pCout, "CONOUT$", "w", stdout);

        // 重定向标准输入到控制台
        FILE *pCin;
        freopen_s(&pCin, "CONIN$", "r", stdin);

        // 重定向标准错误到控制台
        FILE *pCerr;
        freopen_s(&pCerr, "CONOUT$", "w", stderr);

        // 设置控制台窗口标题
        SetConsoleTitleW(L"WebView2 Debug Console");

        std::wcout << L"=== WebView2 Debug Console Started ===" << std::endl;
    }
}
