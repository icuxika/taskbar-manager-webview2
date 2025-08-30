#pragma once
#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>
#include <atomic>
#include <string>
#include <thread>

namespace v1_taskbar_manager {
    class HttpServer {
        std::thread serverThread;
        std::atomic<bool> shouldStop{false};

    public:
        HttpServer();

        ~HttpServer();

        int Start();

        void Stop();

        static int GetPreferredPort();
    };
}

namespace v2_taskbar_manager {
    class HttpServer {
        struct IOContext {
            OVERLAPPED overlapped;
            SOCKET socket = INVALID_SOCKET;
            WSABUF buffer;
            char recvData[2048];
            std::string sendData;
            enum { OP_ACCEPT, OP_RECV, OP_SEND } op;

            std::string requestData;
            bool headerComplete = false;
        };

        SOCKET listenSocket = INVALID_SOCKET;
        std::atomic<bool> isRunning{false};
        HANDLE completionPort = INVALID_HANDLE_VALUE;
        LPFN_ACCEPTEX lpFnAcceptEx = nullptr;
        std::thread worker;

        void PostAccept();
        void WorkerThread();

    public:
        int Start();

        void Stop();
    };
}