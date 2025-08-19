cmake_minimum_required(VERSION 3.15)
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

target_include_directories(${PROJECT_NAME} PRIVATE third-party\\nlohmann)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/index.html $<TARGET_FILE_DIR:${PROJECT_NAME}>/index.html)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/icon.ico $<TARGET_FILE_DIR:${PROJECT_NAME}>/icon.ico)

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION .)

install(FILES ${CMAKE_SOURCE_DIR}/index.html
        DESTINATION .)

install(FILES ${CMAKE_SOURCE_DIR}/icon.ico
        DESTINATION .)

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
