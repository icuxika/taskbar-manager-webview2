#pragma once
#include <winsock2.h>
#include <windows.h>
#include <atomic>
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

        int GetPreferredPort();
    };
}