cmake_minimum_required(VERSION 3.15)
project(taskbar-manager-webview2 VERSION 1.0.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "/utf-8 /EHsc")

set(APP_ICON_PATH "${CMAKE_SOURCE_DIR}/resource/icon.ico")
set(HTML_PATH "${CMAKE_SOURCE_DIR}/resource/index.html")
set(BRIDGE_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/resource/bridge.js")
configure_file(
        "${CMAKE_SOURCE_DIR}/resource/WindowsProject.rc.in"
        "${CMAKE_BINARY_DIR}/WindowsProject.rc"
        @ONLY
)

set_source_files_properties(
        ${CMAKE_BINARY_DIR}/WindowsProject.rc
        PROPERTIES
        OBJECT_DEPENDS "${HTML_PATH};${APP_ICON_PATH};${BRIDGE_SCRIPT_PATH}"
)

add_compile_definitions(UNICODE _UNICODE)

file(GLOB SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} WIN32 main.cpp ${SOURCES} ${CMAKE_BINARY_DIR}/WindowsProject.rc)
target_include_directories(${PROJECT_NAME} PUBLIC include)

find_program(NUGET_EXE NAMES nuget)
if (NOT NUGET_EXE)
    message("NUGET.EXE not found.")
    message(FATAL_ERROR "Please install this executable, and run CMake again.")
endif ()
execute_process(COMMAND ${NUGET_EXE} install "Microsoft.Web.WebView2" -Version 1.0.3351.48 -ExcludeVersion -OutputDirectory ${CMAKE_BINARY_DIR}/packages)
execute_process(COMMAND ${NUGET_EXE} install "Microsoft.Windows.ImplementationLibrary" -Version 1.0.250325.1 -ExcludeVersion -OutputDirectory ${CMAKE_BINARY_DIR}/packages)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_BINARY_DIR}/packages/Microsoft.Web.WebView2/build/native/include)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_BINARY_DIR}/packages/Microsoft.Windows.ImplementationLibrary/include)
target_link_directories(${PROJECT_NAME} PUBLIC ${CMAKE_BINARY_DIR}/packages/Microsoft.Web.WebView2/build/native/x64)
target_link_libraries(${PROJECT_NAME} PUBLIC WebView2LoaderStatic)

target_include_directories(${PROJECT_NAME} PRIVATE third-party\\include)
target_link_libraries(${PROJECT_NAME} PRIVATE Dwmapi Ws2_32 Shlwapi)

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION .)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "TaskbarManager")
set(CPACK_PACKAGE_VENDOR "浮木")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "一个使用 Microsoft Edge WebView2 构建的任务栏管理工具")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-win64")
set(CPACK_PACKAGING_INSTALL_PREFIX "/")
include(CPack)
