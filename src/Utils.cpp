#include "Utils.h"

#include <codecvt>
#include <ios>
#include <iostream>
#include <sstream>

#include "Constants.h"
#include "spdlog/spdlog.h"

namespace v1_taskbar_manager {
    /**
     * @brief 获取当前可执行文件所在的目录路径
     * @return 包含可执行文件目录的宽字符串路径
     */
    std::wstring Utils::GetExeDirectory() {
        wchar_t buffer[MAX_PATH] = {0};
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        std::wstring path(buffer);
        if (const size_t pos = path.find_last_of(L"\\/"); pos != std::wstring::npos) {
            path = path.substr(0, pos);
        }
        return path;
    }

    /**
     * @brief 将宽字符串(std::wstring)转换为UTF-8编码的多字节字符串(std::string)
     * @param wStr 待转换的宽字符串
     * @return 转换后的UTF-8多字节字符串，若输入为空则返回空字符串
     */
    std::string Utils::WStringToString(const std::wstring &wStr) {
        if (wStr.empty())
            return {};

        const int narrowLength = WideCharToMultiByte(
            CP_UTF8,
            0,
            &wStr[0],
            static_cast<int>(wStr.size()),
            nullptr,
            0,
            nullptr,
            nullptr
            );
        if (narrowLength <= 0) {
            return {};
        }

        std::string strTo(narrowLength, 0);

        WideCharToMultiByte(
            CP_UTF8,
            0,
            &wStr[0],
            static_cast<int>(wStr.size()),
            &strTo[0],
            narrowLength,
            nullptr,
            nullptr
            );
        return strTo;
    }

    /**
     * @brief 将UTF-8编码的多字节字符串(std::string)转换为宽字符串(std::wstring)
     * @param str 待转换的多字节字符串(UTF-8编码)
     * @return 转换后的宽字符串，若输入为空则返回空字符串
     */
    std::wstring Utils::StringToWString(const std::string &str) {
        if (str.empty()) {
            return {};
        }

        const int wideLength = MultiByteToWideChar(
            CP_UTF8,
            0,
            str.c_str(),
            static_cast<int>(str.size()),
            nullptr,
            0
            );

        if (wideLength <= 0) {
            return {};
        }

        std::wstring wStr(wideLength, L'\0');

        MultiByteToWideChar(
            CP_UTF8,
            0,
            str.c_str(),
            static_cast<int>(str.size()),
            &wStr[0],
            wideLength
            );

        return wStr;
    }

    /**
     * @brief 将窗口句柄(HWND)转换为十六进制字符串表示
     * @param hWnd 待转换的窗口句柄
     * @return 格式为"0xXXXXXXXX"的十六进制字符串
     */
    std::string Utils::HWndToHexString(HWND hWnd) {
        std::ostringstream oss;
        oss << "0x" << std::hex << std::uppercase
            << reinterpret_cast<uintptr_t>(hWnd);
        return oss.str();
    }

    /**
     * @brief 将十六进制字符串转换为窗口句柄(HWND)
     * @param hexStr 格式为"0xXXXXXXXX"的十六进制字符串
     * @return 解析得到的窗口句柄
     */
    HWND Utils::HexStringToHWnd(const std::string &hexStr) {
        uintptr_t handle;
        std::istringstream iss(hexStr);
        iss >> std::hex >> handle;
        return reinterpret_cast<HWND>(handle);
    }

    /**
     * @brief 检查当前进程是否以管理员权限运行
     * @return 若进程拥有管理员权限则返回true，否则返回false
     */
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

    /**
     * @brief 以管理员权限重新启动当前应用程序
     * @note 使用"runas"动词通过ShellExecuteExW实现权限提升
     */
    void Utils::RelaunchAsAdmin() {
        int nArgs;
        LPWSTR *szArgList = CommandLineToArgvW(GetCommandLineW(), &nArgs);
        if (szArgList == nullptr) {
            return;
        }
        std::wstring parameters;
        if (nArgs > 1) {
            for (int i = 1; i < nArgs; i++) {
                parameters += szArgList[i];
            }
        }
        LocalFree(szArgList);

        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);

        SHELLEXECUTEINFOW sei = {sizeof(sei)};
        sei.fMask = SEE_MASK_DEFAULT;
        sei.hwnd = nullptr;
        sei.lpVerb = L"runas";
        sei.lpFile = exePath;
        sei.lpParameters = parameters.c_str();
        sei.nShow = SW_NORMAL;

        if (!ShellExecuteExW(&sei)) {
            if (const DWORD err = GetLastError(); err == ERROR_CANCELLED) {
                SPDLOG_TRACE("用户取消了提升权限");
            }
        }
    }

    /**
     * @brief 根据进程ID获取进程名称(不含路径)
     * @param processId 目标进程ID
     * @return 进程名称字符串，获取失败则返回L"Unknown"
     */
    std::wstring Utils::GetProcessName(const DWORD processId) {
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

    /**
     * @brief 创建调试控制台并重定向标准输入输出流
     * @note 分配新控制台窗口，并重定向stdout、stdin、stderr到控制台，设置窗口标题为"WebView2 Debug Console"
     */
    void Utils::CreateConsole() {
        // 分配控制台
        AllocConsole();

        // 切换到 UTF-8
        SetConsoleOutputCP(CP_UTF8);

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
        SPDLOG_TRACE("=== 控制台 ===");
    }

    /**
     * @brief 检查应用程序是否已在运行(通过全局互斥量实现)
     * @param mutexName 互斥量名称
     * @param mutex [输出] 指向互斥量句柄的指针，若已运行则为nullptr
     * @return 若应用程序已运行则返回true，否则返回false
     */
    bool Utils::IsAlreadyRunning(const std::wstring &mutexName, HANDLE &mutex) {
        const std::wstring globalMutexName = L"Global\\" + mutexName;
        mutex = CreateMutexW(nullptr, false, globalMutexName.c_str());
        if (mutex != nullptr) {
            if (GetLastError() == ERROR_ALREADY_EXISTS) {
                CloseHandle(mutex);
                mutex = nullptr;
                return true;
            }
            return false;
        }
        // 这里暂不判断CreateMutexW创建失败的情况
        return false;
    }

    /**
     * @brief 从资源中加载字符串并转换为宽字符串
     * @param id 资源ID
     * @param type 资源类型
     * @return 加载并转换后的宽字符串
     * @note 假设资源中存储的是UTF-8编码的字符串
     */
    std::wstring Utils::LoadWStringFromResource(const int id, const int type) {
        const HRSRC hRes = FindResource(nullptr,MAKEINTRESOURCE(id), MAKEINTRESOURCE(type));
        DWORD size = SizeofResource(GetModuleHandle(nullptr), hRes);
        HGLOBAL hGlobal = LoadResource(GetModuleHandle(nullptr), hRes);
        LPVOID pBuffer = LockResource(hGlobal);
        const char *data = static_cast<const char *>(pBuffer);
        std::string str(data, size);
        return StringToWString(str);
    }

    /**
     * @brief 将端口号保存到Windows注册表
     * @param port 要保存的端口号(1024-65535)
     * @note 保存路径为HKCU\SOFTWARE\TaskbarManager\HttpServer，键名为"Port"，类型为REG_DWORD
     */
    void Utils::SavePortToWindowsRegistry(int port) {
        HKEY hKey;
        std::wstring regPath(L"SOFTWARE\\");
        regPath += APP_IDENTIFIER;
        regPath += L"\\HttpServer";
        LONG result = RegCreateKeyEx(HKEY_CURRENT_USER, regPath.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,KEY_WRITE,
                                     nullptr, &hKey, nullptr);
        if (result != ERROR_SUCCESS) {
            SPDLOG_ERROR("无法在注册表新建项");
            return;
        }
        const auto portValue = static_cast<DWORD>(port);
        result = RegSetValueEx(hKey, L"Port", 0, REG_DWORD,
                               reinterpret_cast<const BYTE *>(&portValue),
                               sizeof(DWORD));

        if (result != ERROR_SUCCESS) {
            SPDLOG_ERROR("无法在注册表保存端口号");
        }
        RegCloseKey(hKey);
    }

    /**
     * @brief 从Windows注册表读取端口号
     * @return 读取到的有效端口号(1024-65535)，若读取失败或端口无效则返回0
     * @note 读取路径为HKCU\SOFTWARE\TaskbarManager\HttpServer，键名为"Port"
     */
    int Utils::ReadPortFromWindowsRegistry() {
        HKEY hKey;
        std::wstring regPath(L"SOFTWARE\\");
        regPath += APP_IDENTIFIER;
        regPath += L"\\HttpServer";
        LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, regPath.c_str(), 0, KEY_READ, &hKey);

        if (result != ERROR_SUCCESS) {
            return 0;
        }

        DWORD port = 0;
        DWORD dataSize = sizeof(DWORD);
        DWORD dataType = REG_DWORD;

        result = RegQueryValueEx(hKey, L"Port", nullptr, &dataType,
                                 reinterpret_cast<LPBYTE>(&port), &dataSize);

        RegCloseKey(hKey);

        if (result != ERROR_SUCCESS) {
            return 0;
        }

        // 验证端口号范围
        if (port < 1024 || port > 65535) {
            return 0;
        }
        return static_cast<int>(port);
    }

    /**
     * @brief 检查指定TCP端口是否可用(未被占用)
     * @param port 要检查的端口号
     * @return 若端口可用则返回true，否则返回false
     * @note 通过尝试绑定到localhost:port来判断端口是否被占用
     */
    bool Utils::IsPortAvailable(int port) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        const SOCKET testSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (testSocket == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);

        const bool available = (bind(testSocket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != SOCKET_ERROR);

        closesocket(testSocket);
        WSACleanup();

        return available;
    }
}