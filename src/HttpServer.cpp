#include "HttpServer.h"

#include "Utils.h"
#include "spdlog/spdlog.h"

#include <future>
#include <sstream>

namespace v1_taskbar_manager {

    HttpServer::HttpServer() {
    }

    HttpServer::~HttpServer() {
    }

    /**
     * @brief 启动 HTTP 服务器
     *
     * 启动 HTTP 服务器的运行，监听指定端口。
     *
     * @return int 服务器监听的端口号
     */
    int HttpServer::Start() {
        const std::wstring wStrHTML = Utils::LoadWStringFromResource(302, 303);
        const std::string html = Utils::WStringToString(wStrHTML);

        std::promise<int> portPromise;
        auto portFuture = portPromise.get_future();

        serverThread = std::thread([html, p = std::move(portPromise), this]() mutable {
            // 定义智能指针删除器
            auto socketDeleter = [](const SOCKET *sock) {
                if (*sock != INVALID_SOCKET) {
                    closesocket(*sock);
                }
                WSACleanup();
                delete sock;
            };

            // 创建智能指针管理套接字
            std::unique_ptr<SOCKET, decltype(socketDeleter)> serverSocketPtr(new SOCKET(INVALID_SOCKET), socketDeleter);
            SOCKET &serverSocket = *serverSocketPtr;

            WSADATA wsaData;
            if (const int result = WSAStartup(MAKEWORD(2, 2), &wsaData); result != 0) {
                p.set_value(-1);
                return;
            }

            serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (serverSocket == INVALID_SOCKET) {
                WSACleanup();
                p.set_value(-1);
                return;
            }

            constexpr int reuseAddr = 1;
            setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuseAddr),
                       sizeof(reuseAddr));

            int selectedPort = GetPreferredPort();

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(selectedPort);

            if (bind(serverSocket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == SOCKET_ERROR) {
                closesocket(serverSocket);
                WSACleanup();
                p.set_value(-1);
                return;
            }

            sockaddr_in actualAddr{};
            int len = sizeof(actualAddr);
            if (getsockname(serverSocket, reinterpret_cast<sockaddr *>(&actualAddr), &len) == SOCKET_ERROR) {
                closesocket(serverSocket);
                WSACleanup();
                p.set_value(-1);
                return;
            }
            int actualPort = ntohs(actualAddr.sin_port);

            if (listen(serverSocket, 5) == SOCKET_ERROR) {
                // 增加backlog到5
                closesocket(serverSocket);
                WSACleanup();
                p.set_value(-1);
                return;
            }

            Utils::SavePortToWindowsRegistry(actualPort);
            p.set_value(actualPort);

            while (!shouldStop.load()) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(serverSocket, &fds);

                timeval timeout = {0, 100000}; // 100ms超时

                if (int selectResult = select(0, &fds, nullptr, nullptr, &timeout);
                    selectResult > 0 && FD_ISSET(serverSocket, &fds)) {
                    if (SOCKET clientSocket = accept(serverSocket, nullptr, nullptr); clientSocket != INVALID_SOCKET) {
                        // 暂时为 recv 设置一个超时时间，避免在一些机器上 recv 阻塞导致程序无法正常退出
                        int recvTimeout = 1000;
                        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&recvTimeout),
                                   sizeof(recvTimeout));

                        // 读取客户端请求
                        char buffer[4096];
                        if (const int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
                            bytesReceived > 0) {
                            buffer[bytesReceived] = '\0';
                        }

                        std::ostringstream response;
                        response << "HTTP/1.1 200 OK\r\n";
                        response << "Content-Type: text/html; charset=utf-8\r\n";
                        response << "Content-Length: " << html.size() << "\r\n";
                        response << "Connection: close\r\n";
                        response << "\r\n";
                        response << html;

                        std::string responseStr = response.str();

                        // 发送响应
                        int totalSent = 0;
                        int responseSize = static_cast<int>(responseStr.size());
                        while (totalSent < responseSize && !shouldStop.load()) {
                            int sent = send(clientSocket, responseStr.c_str() + totalSent, responseSize - totalSent, 0);
                            if (sent == SOCKET_ERROR) {
                                break;
                            }
                            totalSent += sent;
                        }
                        shutdown(clientSocket, SD_SEND);
                        closesocket(clientSocket);
                    }
                }
            }
        });
        return portFuture.get();
    }

    /**
     * @brief 停止 HTTP 服务器
     *
     * 停止 HTTP 服务器的运行，等待服务器线程结束。
     */
    void HttpServer::Stop() {
        SPDLOG_INFO("正在停止 Socket 服务");
        shouldStop.store(true);
        if (serverThread.joinable()) {
            SPDLOG_INFO("等待 Socket 服务线程结束");
            serverThread.join();
        }
        SPDLOG_INFO("Socket 服务已停止");
    }

    /**
     * @brief 获取首选端口
     *
     * 从 Windows 注册表中读取上次使用的端口，如果端口不可用或不存在，则返回 0。
     *
     * @return int 首选端口号
     */
    int HttpServer::GetPreferredPort() {
        const int savedPort = Utils::ReadPortFromWindowsRegistry();
        if (savedPort > 0) {
            if (Utils::IsPortAvailable(savedPort)) {
                return savedPort;
            }
            SPDLOG_INFO("程序上次使用的端口不再可用，将使用系统分配的新端口");
        }
        return 0;
    }

}