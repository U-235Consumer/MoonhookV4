cmake_minimum_required(VERSION 3.15)
project(MoonhookV4 VERSION 4.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

FetchContent_Declare(
    luau
    GIT_REPOSITORY https://github.com/luau-lang/luau.git
    GIT_TAG        0.723
)

set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(luau)

set(CURL_ENABLE_SSL ON CACHE BOOL "" FORCE)
if(WIN32)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
else()
    set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
    curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG        curl-8_2_0
)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_TESTS ON CACHE BOOL "" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(curl)

if (MSVC)
    add_compile_options(/utf-8)
endif()

add_executable(moonhookv4
    src/main.cpp
    src/bot.cpp
    src/webhook.cpp
    src/plugins.cpp
    src/safe_fs.cpp
)

target_link_libraries(moonhookv4 PRIVATE
    Luau.VM
    Luau.Compiler
    CURL::libcurl
)

target_include_directories(moonhookv4 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${luau_SOURCE_DIR}/VM/include
    ${luau_SOURCE_DIR}/Compiler/include
)

add_custom_command(TARGET moonhookv4 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:CURL::libcurl>
        $<TARGET_FILE_DIR:moonhookv4>
)