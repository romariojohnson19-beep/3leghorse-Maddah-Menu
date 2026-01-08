cmake_minimum_required(VERSION 3.20)
project(imgui_vendor)


include(FetchContent)
# Prefer local canonical ImGui sources (placed in this folder). If they are
# missing, fall back to FetchContent to download upstream ImGui.

set(LOCAL_IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)

if(EXISTS ${LOCAL_IMGUI_DIR}/imgui.cpp AND EXISTS ${LOCAL_IMGUI_DIR}/imgui.h)
    message(STATUS "Using local ImGui sources from ${LOCAL_IMGUI_DIR}")
    set(IMGUI_SOURCE_DIR ${LOCAL_IMGUI_DIR})
else()
    message(STATUS "Local ImGui not found; Fetching Dear ImGui from github.com/ocornut/imgui (master).")
    FetchContent_Declare(
        imgui_populate
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(imgui_populate)
    set(IMGUI_SOURCE_DIR ${imgui_populate_SOURCE_DIR})
endif()

# Core sources
set(IMGUI_CORE_SOURCES
    ${IMGUI_SOURCE_DIR}/imgui.cpp
    ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
    ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
)

# Backends may live in a 'backends/' subdirectory in upstream, or directly in
# the source directory when vendored. Prefer backends/ if present.
if(EXISTS ${IMGUI_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
    set(IMGUI_BACKEND_DIR ${IMGUI_SOURCE_DIR}/backends)
else()
    set(IMGUI_BACKEND_DIR ${IMGUI_SOURCE_DIR})
endif()

set(IMGUI_SOURCES
    ${IMGUI_CORE_SOURCES}
    ${IMGUI_BACKEND_DIR}/imgui_impl_dx11.cpp
    ${IMGUI_BACKEND_DIR}/imgui_impl_win32.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC ${IMGUI_SOURCE_DIR} ${IMGUI_BACKEND_DIR})

if (MSVC)
    target_compile_definitions(imgui PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
