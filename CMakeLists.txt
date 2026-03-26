cmake_minimum_required(VERSION 3.20)
project(saper_webview LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(WEBVIEW2_ROOT "$ENV{USERPROFILE}/.nuget/packages/microsoft.web.webview2")
if(NOT EXISTS "${WEBVIEW2_ROOT}")
    message(FATAL_ERROR "WebView2 SDK package was not found in ${WEBVIEW2_ROOT}")
endif()

file(GLOB WEBVIEW2_VERSIONS LIST_DIRECTORIES true "${WEBVIEW2_ROOT}/*")
if(NOT WEBVIEW2_VERSIONS)
    message(FATAL_ERROR "No WebView2 SDK versions were found in ${WEBVIEW2_ROOT}")
endif()

list(SORT WEBVIEW2_VERSIONS COMPARE NATURAL ORDER DESCENDING)
list(GET WEBVIEW2_VERSIONS 0 WEBVIEW2_DIR)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WEBVIEW2_ARCH x64)
else()
    set(WEBVIEW2_ARCH x86)
endif()

set(WEBVIEW2_INCLUDE_DIR "${WEBVIEW2_DIR}/build/native/include")
set(WEBVIEW2_LOADER_LIB "${WEBVIEW2_DIR}/build/native/${WEBVIEW2_ARCH}/WebView2LoaderStatic.lib")

if(NOT EXISTS "${WEBVIEW2_INCLUDE_DIR}/WebView2.h")
    message(FATAL_ERROR "WebView2.h was not found under ${WEBVIEW2_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${WEBVIEW2_LOADER_LIB}")
    message(FATAL_ERROR "WebView2 loader library was not found: ${WEBVIEW2_LOADER_LIB}")
endif()

add_executable(saper_webview WIN32
    src/main.cpp
)

target_include_directories(saper_webview PRIVATE
    "${WEBVIEW2_INCLUDE_DIR}"
)

target_compile_definitions(saper_webview PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
)

target_link_libraries(saper_webview PRIVATE
    "${WEBVIEW2_LOADER_LIB}"
    user32
    ole32
)

add_custom_command(TARGET saper_webview POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:saper_webview>/ui"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/ui" "$<TARGET_FILE_DIR:saper_webview>/ui"
)
