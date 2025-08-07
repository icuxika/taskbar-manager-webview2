cmake_minimum_required(VERSION 3.31)
project(webview2_win32)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "/utf-8 /EHsc")

add_executable(webview2_win32 WIN32 main.cpp)

find_program(NUGET_EXE NAMES nuget)
if(NOT NUGET_EXE)
    message("NUGET.EXE not found.")
    message(FATAL_ERROR "Please install this executable, and run CMake again.")
endif()
execute_process(COMMAND ${NUGET_EXE} install "Microsoft.Web.WebView2" -Version 1.0.3351.48 -ExcludeVersion -OutputDirectory ${CMAKE_BINARY_DIR}/packages)
execute_process(COMMAND ${NUGET_EXE} install "Microsoft.Windows.ImplementationLibrary" -Version 1.0.250325.1 -ExcludeVersion -OutputDirectory ${CMAKE_BINARY_DIR}/packages)
target_include_directories(webview2_win32 PUBLIC ${CMAKE_BINARY_DIR}/packages/Microsoft.Web.WebView2/build/native/include)
target_include_directories(webview2_win32 PUBLIC ${CMAKE_BINARY_DIR}/packages/Microsoft.Windows.ImplementationLibrary/include)
target_link_directories(webview2_win32 PUBLIC ${CMAKE_BINARY_DIR}/packages/Microsoft.Web.WebView2/build/native/x64)
target_link_libraries(webview2_win32 PUBLIC WebView2LoaderStatic)
