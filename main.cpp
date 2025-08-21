#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Application.h"

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
) {
    return v1_taskbar_manager::Application::GetInstance().Run(hInstance, nCmdShow);
}