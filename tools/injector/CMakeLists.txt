cmake_minimum_required(VERSION 3.15)
project(dll_injector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dll_injector main.cpp)

if (MSVC)
    target_compile_definitions(dll_injector PRIVATE _UNICODE UNICODE)
endif()

target_include_directories(dll_injector PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
    target_link_libraries(dll_injector PRIVATE ws2_32)
endif()

if (MINGW)
    # Ensure MinGW generates Unicode-aware entrypoints and links properly
    target_compile_definitions(dll_injector PRIVATE UNICODE _UNICODE)
    target_link_options(dll_injector PRIVATE "-municode")
endif()
