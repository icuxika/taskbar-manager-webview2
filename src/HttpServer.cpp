#include "HttpServer.h"

#include "Utils.h"
#include "spdlog/spdlog.h"

#include <future>
#include <iostream>
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

namespace v2_taskbar_manager {
    int HttpServer::Start() {
        int iResult = 0;

        WSADATA wsaData = {};
        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0) {
            return -1;
        }

        listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
        if (listenSocket == INVALID_SOCKET) {
            WSACleanup();
            return -1;
        }

        u_long iMode = 1;
        iResult = ioctlsocket(listenSocket, FIONBIO, &iMode);
        if (iResult != NO_ERROR) {
            closesocket(listenSocket);
            WSACleanup();
            return -1;
        }

        const int selectedPort = v1_taskbar_manager::HttpServer::GetPreferredPort();

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(selectedPort);

        iResult = bind(listenSocket, reinterpret_cast<SOCKADDR *>(&address), sizeof (address));
        if (iResult == SOCKET_ERROR) {
            closesocket(listenSocket);
            WSACleanup();
            return -1;
        }

        sockaddr_in actualAddr{};
        int len = sizeof(actualAddr);
        if (getsockname(listenSocket, reinterpret_cast<sockaddr *>(&actualAddr), &len) == SOCKET_ERROR) {
            closesocket(listenSocket);
            WSACleanup();
            return -1;
        }

        const int actualPort = ntohs(actualAddr.sin_port);

        iResult = listen(listenSocket, SOMAXCONN);
        if (iResult == SOCKET_ERROR) {
            closesocket(listenSocket);
            WSACleanup();
            return -1;
        }

        if (selectedPort == 0) {
            v1_taskbar_manager::Utils::SavePortToWindowsRegistry(actualPort);
        }

        completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, listenSocket, 0);
        if (completionPort == nullptr) {
            closesocket(listenSocket);
            WSACleanup();
            return -1;
        }
        CreateIoCompletionPort(reinterpret_cast<HANDLE>(listenSocket), completionPort, listenSocket, 0);

        GUID guidAcceptEx = WSAID_ACCEPTEX;
        DWORD bytes;
        iResult = WSAIoctl(listenSocket,SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAcceptEx, sizeof(guidAcceptEx),
                           &lpFnAcceptEx,
                           sizeof(lpFnAcceptEx), &bytes, nullptr, nullptr);
        if (iResult == SOCKET_ERROR) {
            CloseHandle(completionPort);
            closesocket(listenSocket);
            WSACleanup();
            return -1;
        }
        isRunning.store(true);
        worker = std::thread(&HttpServer::WorkerThread, this);
        PostAccept();
        return actualPort;
    }

    void HttpServer::Stop() {
        SPDLOG_INFO("正在停止 Socket 服务");
        if (!isRunning) {
            SPDLOG_INFO("Socket 服务已停止");
            return;
        }

        isRunning.store(false);

        if (completionPort) {
            PostQueuedCompletionStatus(completionPort, 0, 0, nullptr);
        }

        if (worker.joinable()) {
            SPDLOG_INFO("等待 Worker 线程结束");
            worker.join();
        }

        if (listenSocket != INVALID_SOCKET) {
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
        }

        if (completionPort) {
            CloseHandle(completionPort);
            completionPort = nullptr;
        }

        WSACleanup();
        SPDLOG_INFO("Socket 服务已停止");
    }

    void HttpServer::PostAccept() {
        const SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0,WSA_FLAG_OVERLAPPED);
        if (socket == INVALID_SOCKET) {
            return;
        }
        auto *context = new IOContext();
        context->socket = socket;
        context->op = IOContext::OP_ACCEPT;
        context->buffer.buf = context->recvData;
        context->buffer.len = sizeof(context->recvData);

        DWORD bytes = 0;
        const BOOL ok = lpFnAcceptEx(listenSocket, socket, context->recvData, 0, sizeof(sockaddr_in) + 16,
                                     sizeof(sockaddr_in) + 16, &bytes, &context->overlapped);
        if (!ok && WSAGetLastError() != ERROR_IO_PENDING) {
            closesocket(socket);
            delete context;
        }
    }

    void HttpServer::WorkerThread() {
        const auto logger = spdlog::get("spdlog");
        const std::wstring wStrHTML = v1_taskbar_manager::Utils::LoadWStringFromResource(302, 303);
        const std::string html = v1_taskbar_manager::Utils::WStringToString(wStrHTML);
        while (isRunning) {
            DWORD bytesTransferred;
            ULONG_PTR completionKey;
            LPOVERLAPPED overlapped;

            const BOOL ok = GetQueuedCompletionStatus(completionPort, &bytesTransferred, &completionKey, &overlapped,
                                                      INFINITE);

            if (!isRunning) {
                break;
            }

            if (!ok) {
                if (GetLastError() == ERROR_TIMEOUT) {
                    continue;
                }
                if (overlapped) {
                    const auto context = CONTAINING_RECORD(overlapped, IOContext, overlapped);
                    if (context->socket != INVALID_SOCKET) {
                        closesocket(context->socket);
                        delete context;
                    }
                }
                continue;
            }

            if (completionKey == 0 && overlapped == nullptr) {
                break;
            }

            if (!overlapped) {
                continue;
            }

            const auto context = CONTAINING_RECORD(overlapped, IOContext, overlapped);
            if (context->op == IOContext::OP_ACCEPT) {
                setsockopt(context->socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                           reinterpret_cast<char *>(&listenSocket), sizeof(listenSocket));

                CreateIoCompletionPort(reinterpret_cast<HANDLE>(context->socket), completionPort, context->socket, 0);

                ZeroMemory(&context->overlapped, sizeof(context->overlapped));
                context->op = IOContext::OP_RECV;
                context->buffer.buf = context->recvData;
                context->buffer.len = sizeof(context->recvData);
                DWORD flags = 0;
                WSARecv(context->socket, &context->buffer, 1, nullptr, &flags, &context->overlapped, nullptr);

                PostAccept();
            } else if (context->op == IOContext::OP_RECV) {
                if (bytesTransferred == 0) {
                    closesocket(context->socket);
                    delete context;
                    continue;
                }

                // 累积接收到的请求数据
                context->requestData.append(context->recvData, bytesTransferred);

                if (!context->headerComplete) {
                    const size_t headerEnd = context->requestData.find("\r\n\r\n");
                    if (headerEnd != std::string::npos) {
                        context->headerComplete = true;
                        // 解析请求行
                        const size_t lineEnd = context->requestData.find("\r\n");
                        if (lineEnd != std::string::npos) {
                            const std::string requestLine = context->requestData.substr(0, lineEnd);
                            std::istringstream iss(requestLine);
                            std::string method, path, protocol;
                            iss >> method >> path >> protocol;
                            logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info,
                                        "收到请求: Method[{}], Path[{}], Protocol[{}]", method, path, protocol);

                            if (method == "GET") {
                                if (path == "/" || path == "/index.html") {
                                    std::ostringstream stream;
                                    stream << "HTTP/1.1 200 OK\r\n";
                                    stream << "Content-Type: text/html; charset=utf-8\r\n";
                                    stream << "Content-Length: " << html.size() << "\r\n";
                                    stream << "Connection: close\r\n";
                                    stream << "\r\n";
                                    stream << html;
                                    context->sendData = stream.str();
                                } else {
                                    std::ostringstream stream;
                                    stream << "HTTP/1.1 404 Not Found\r\n";
                                    stream << "Content-Length: 0\r\n";
                                    stream << "Connection: close\r\n";
                                    stream << "\r\n";
                                    context->sendData = stream.str();
                                }
                            } else {
                                std::ostringstream stream;
                                stream << "HTTP/1.1 501 Not Implemented\r\n";
                                stream << "Content-Length: 0\r\n";
                                stream << "Connection: close\r\n";
                                stream << "\r\n";
                                context->sendData = stream.str();
                            }

                            ZeroMemory(&context->overlapped, sizeof(context->overlapped));
                            context->op = IOContext::OP_SEND;
                            context->buffer.buf = context->sendData.data();
                            context->buffer.len = context->sendData.size();
                            WSASend(context->socket, &context->buffer, 1, nullptr, 0, &context->overlapped, nullptr);
                        }
                    } else {
                        ZeroMemory(&context->overlapped, sizeof(context->overlapped));
                        context->op = IOContext::OP_RECV;
                        context->buffer.buf = context->recvData;
                        context->buffer.len = sizeof(context->recvData);
                        DWORD flags = 0;
                        WSARecv(context->socket, &context->buffer, 1, nullptr, &flags, &context->overlapped, nullptr);
                    }
                }
            } else if (context->op == IOContext::OP_SEND) {
                closesocket(context->socket);
                delete context;
            }
        }
    }
}