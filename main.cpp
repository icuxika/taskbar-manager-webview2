#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Application.h"

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nShowCmd) {
    return v1_taskbar_manager::Application::GetInstance().Run(hInstance, nShowCmd);
}