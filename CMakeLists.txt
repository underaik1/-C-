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

set(UI_ASSETS
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/index.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/styles.css"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/app.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/vendor/anime.min.js"
)

# Keep UI files visible in IDE and force resource recompilation when they change.
set_source_files_properties(${UI_ASSETS} PROPERTIES HEADER_FILE_ONLY ON)
set_source_files_properties(src/ui_resources.rc PROPERTIES OBJECT_DEPENDS "${UI_ASSETS}")

add_executable(saper_webview WIN32
    src/main.cpp
    src/ui_resources.rc
    ${UI_ASSETS}
)

target_include_directories(saper_webview PRIVATE
    src
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
