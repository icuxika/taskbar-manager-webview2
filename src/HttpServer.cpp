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
        // 从资源文件加载HTML内容
        const std::wstring wStrHTML = Utils::LoadWStringFromResource(302, 303);
        const std::string html = Utils::WStringToString(wStrHTML);

        std::promise<int> portPromise;
        auto portFuture = portPromise.get_future();

        serverThread = std::thread([html, p = std::move(portPromise), this]() mutable {
            const auto logger = spdlog::get("spdlog");

            WSADATA wsaData;
            if (const int result = WSAStartup(MAKEWORD(2, 2), &wsaData); result != 0) {
                p.set_value(-1);
                return;
            }

            SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (serverSocket == INVALID_SOCKET) {
                WSACleanup();
                p.set_value(-1);
                return;
            }

            // 设置套接字为非阻塞模式
            u_long iMode = 1;
            if (ioctlsocket(serverSocket, FIONBIO, &iMode) != NO_ERROR) {
                closesocket(serverSocket);
                WSACleanup();
                p.set_value(-1);
                return;
            }

            // 设置套接字选项以允许地址重用
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

            // 获取实际分配的端口号
            sockaddr_in actualAddr{};
            int len = sizeof(actualAddr);
            if (getsockname(serverSocket, reinterpret_cast<sockaddr *>(&actualAddr), &len) == SOCKET_ERROR) {
                closesocket(serverSocket);
                WSACleanup();
                p.set_value(-1);
                return;
            }

            int actualPort = ntohs(actualAddr.sin_port);

            if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
                closesocket(serverSocket);
                WSACleanup();
                p.set_value(-1);
                return;
            }

            // 将端口号保存到Windows注册表
            Utils::SavePortToWindowsRegistry(actualPort);
            p.set_value(actualPort);

            // 客户端套接字管理
            std::vector<SOCKET> clientSockets;
            std::unordered_map<SOCKET, std::string> clientRecvBuffers;
            std::unordered_map<SOCKET, std::string> clientSendBuffers;
            std::unordered_map<SOCKET, int> sendCount;

            while (!shouldStop.load()) {
                fd_set readFds, writeFds;
                FD_ZERO(&readFds);
                FD_ZERO(&writeFds);

                // 添加服务器套接字到读集合
                FD_SET(serverSocket, &readFds);

                // 添加所有客户端套接字到适当的集合
                for (SOCKET client : clientSockets) {
                    FD_SET(client, &readFds);
                    // 如果客户端有待发送的数据，添加到写集合
                    if (clientSendBuffers.find(client) != clientSendBuffers.end() &&
                        sendCount[client] < clientSendBuffers[client].size()) {
                        FD_SET(client, &writeFds);
                    }
                }

                // 设置select超时时间
                timeval timeout = {0, 100000};

                int selectResult = select(0, &readFds, &writeFds, nullptr, &timeout);

                if (selectResult == SOCKET_ERROR) {
                    if (WSAGetLastError() == WSAENOTSOCK) {
                        break;
                    }
                    continue;
                }

                if (selectResult > 0) {
                    // 处理新的客户端连接
                    if (FD_ISSET(serverSocket, &readFds)) {
                        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
                        if (clientSocket != INVALID_SOCKET) {
                            u_long clientMode = 1;
                            ioctlsocket(clientSocket, FIONBIO, &clientMode);
                            clientSockets.push_back(clientSocket);
                            sendCount[clientSocket] = 0;
                            clientRecvBuffers[clientSocket] = "";
                        }
                    }

                    std::vector<SOCKET> socketsToMove;

                    // 处理已连接客户端的数据
                    for (SOCKET client : clientSockets) {
                        if (FD_ISSET(client, &readFds)) {
                            char buffer[2048];
                            int bytesReceived = recv(client, buffer, sizeof(buffer) - 1, 0);

                            if (bytesReceived > 0) {
                                clientRecvBuffers[client] += std::string(buffer, bytesReceived);

                                size_t headerEnd = clientRecvBuffers[client].find("\r\n\r\n");
                                if (headerEnd != std::string::npos) {
                                    const size_t lineEnd = clientRecvBuffers[client].find("\r\n");
                                    if (lineEnd != std::string::npos) {
                                        const std::string requestLine = clientRecvBuffers[client].substr(0, lineEnd);
                                        std::istringstream iss(requestLine);
                                        std::string method, path, protocol;
                                        iss >> method >> path >> protocol;
                                        logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},
                                                    spdlog::level::info,
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
                                                clientSendBuffers[client] = stream.str();
                                            } else {
                                                std::ostringstream stream;
                                                stream << "HTTP/1.1 404 Not Found\r\n";
                                                stream << "Content-Length: 0\r\n";
                                                stream << "Connection: close\r\n";
                                                stream << "\r\n";
                                                clientSendBuffers[client] = stream.str();
                                            }
                                        } else {
                                            std::ostringstream stream;
                                            stream << "HTTP/1.1 501 Not Implemented\r\n";
                                            stream << "Content-Length: 0\r\n";
                                            stream << "Connection: close\r\n";
                                            stream << "\r\n";
                                            clientSendBuffers[client] = stream.str();
                                        }
                                        sendCount[client] = 0;
                                    }
                                }
                            } else if (bytesReceived == 0) {
                                socketsToMove.push_back(client);
                            } else {
                                if (WSAGetLastError() != WSAEWOULDBLOCK) {
                                    socketsToMove.push_back(client);
                                }
                            }
                        }

                        // 处理客户端发送数据
                        if (FD_ISSET(client, &writeFds) && clientSendBuffers.find(client) != clientSendBuffers.end()) {
                            const std::string &responseStr = clientSendBuffers[client];
                            int totalCount = static_cast<int>(responseStr.size());
                            int &totalSendCount = sendCount[client];
                            if (totalSendCount < totalCount) {
                                int count = send(client, responseStr.c_str() + totalSendCount,
                                                 totalCount - totalSendCount, 0);
                                if (count == SOCKET_ERROR) {
                                    if (WSAGetLastError() != WSAEWOULDBLOCK) {
                                        socketsToMove.push_back(client);
                                    }
                                } else {
                                    totalSendCount += count;
                                    if (totalSendCount >= totalCount) {
                                        socketsToMove.push_back(client);
                                    }
                                }
                            }
                        }
                    }

                    // 清理已断开连接的客户端
                    for (SOCKET client : socketsToMove) {
                        auto it = std::find(clientSockets.begin(), clientSockets.end(), client);
                        if (it != clientSockets.end()) {
                            clientSockets.erase(it);
                        }
                        shutdown(client, SD_SEND);
                        closesocket(client);
                        clientSendBuffers.erase(client);
                        sendCount.erase(client);
                    }
                }
            }

            // 关闭所有客户端连接
            for (SOCKET client : clientSockets) {
                shutdown(client, SD_SEND);
                closesocket(client);
            }

            // 清理服务器套接字和Winsock
            closesocket(serverSocket);
            WSACleanup();
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

        // 设置套接字为非阻塞模式
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

        // 获取实际分配的端口号
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

        // 创建I/O完成端口
        completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, listenSocket, 0);
        if (completionPort == nullptr) {
            closesocket(listenSocket);
            WSACleanup();
            return -1;
        }
        CreateIoCompletionPort(reinterpret_cast<HANDLE>(listenSocket), completionPort, listenSocket, 0);

        // 获取AcceptEx函数指针
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

        // 启动服务器
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

    /**
     * @brief 提交接受连接请求
     *
     * 提交一个异步接受连接请求，将新连接添加到客户端列表中。
     */
    void HttpServer::PostAccept() {
        const SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0,WSA_FLAG_OVERLAPPED);
        if (socket == INVALID_SOCKET) {
            return;
        }
        // 创建一个新的I/O上下文来跟踪这个异步操作
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
        // 从资源文件加载HTML内容
        const std::wstring wStrHTML = v1_taskbar_manager::Utils::LoadWStringFromResource(302, 303);
        const std::string html = v1_taskbar_manager::Utils::WStringToString(wStrHTML);
        while (isRunning) {
            // 传输的字节数
            DWORD bytesTransferred;
            // 完成键
            ULONG_PTR completionKey;
            // 重叠结构指针
            LPOVERLAPPED overlapped;

            // 从完成端口获取完成的I/O操作
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
            // 根据操作类型处理不同的I/O完成事件
            if (context->op == IOContext::OP_ACCEPT) {
                setsockopt(context->socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                           reinterpret_cast<char *>(&listenSocket), sizeof(listenSocket));

                CreateIoCompletionPort(reinterpret_cast<HANDLE>(context->socket), completionPort, context->socket, 0);

                // 重置overlapped结构并准备接收数据
                ZeroMemory(&context->overlapped, sizeof(context->overlapped));
                context->op = IOContext::OP_RECV;
                context->buffer.buf = context->recvData;
                context->buffer.len = sizeof(context->recvData);

                // 投递异步接收操作
                DWORD flags = 0;
                WSARecv(context->socket, &context->buffer, 1, nullptr, &flags, &context->overlapped, nullptr);

                // 投递下一个接受连接操作，保持服务器能够接受新连接
                PostAccept();
            } else if (context->op == IOContext::OP_RECV) {
                if (bytesTransferred == 0) {
                    closesocket(context->socket);
                    delete context;
                    continue;
                }

                // 累积接收到的请求数据
                context->requestData.append(context->recvData, bytesTransferred);

                // 检查是否已接收完整的HTTP头部
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

                            // 投递异步发送操作
                            ZeroMemory(&context->overlapped, sizeof(context->overlapped));
                            context->op = IOContext::OP_SEND;
                            context->buffer.buf = context->sendData.data();
                            context->buffer.len = context->sendData.size();
                            WSASend(context->socket, &context->buffer, 1, nullptr, 0, &context->overlapped, nullptr);
                        }
                    } else {
                        // HTTP头部还未完整接收，继续接收数据
                        ZeroMemory(&context->overlapped, sizeof(context->overlapped));
                        context->op = IOContext::OP_RECV;
                        context->buffer.buf = context->recvData;
                        context->buffer.len = sizeof(context->recvData);
                        DWORD flags = 0;
                        WSARecv(context->socket, &context->buffer, 1, nullptr, &flags, &context->overlapped, nullptr);
                    }
                }
            } else if (context->op == IOContext::OP_SEND) {
                context->sendCount += bytesTransferred;
                if (context->sendCount < context->sendData.size()) {
                    ZeroMemory(&context->overlapped, sizeof(context->overlapped));
                    context->op = IOContext::OP_SEND;
                    context->buffer.buf = context->sendData.data() + context->sendCount;
                    context->buffer.len = context->sendData.size() - context->sendCount;
                    WSASend(context->socket, &context->buffer, 1, nullptr, 0, &context->overlapped, nullptr);
                } else {
                    closesocket(context->socket);
                    delete context;
                }
            }
        }
    }
}