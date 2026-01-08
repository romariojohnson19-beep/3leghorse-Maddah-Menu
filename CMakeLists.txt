option(COPY_FSL_VERSION_DLL "Copy version.dll (FSL) to output dir" OFF)
cmake_minimum_required(VERSION 3.20)
project(YimMenuCustom LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_GENERATOR)
    set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "")
endif()

option(ENABLE_SCRIPTHOOKV "Enable ScriptHookV + MinHook integration" OFF)
set(SHV_DIR "" CACHE PATH "Path to ScriptHookV SDK root (contains inc/natives.h)" )
set(MINHOOK_DIR "" CACHE PATH "Path to MinHook root (contains include/MinHook.h)" )

option(ENABLE_IMGUI "Enable ImGui + renderer integration" OFF)
set(IMGUI_DIR "" CACHE PATH "Path to ImGui root (contains imgui.h)" )

if(ENABLE_IMGUI)
    add_definitions(-DENABLE_IMGUI)
endif()


add_library(YimMenuCustom SHARED
    src/dllmain.cpp
    src/menu_stub.cpp
    src/native_interface.cpp
    src/protections.cpp
    src/renderer.cpp
    src/config.cpp
    src/hook_helpers.cpp
    src/hooks/present_hook.cpp
)

target_include_directories(YimMenuCustom PRIVATE include)

if(ENABLE_SCRIPTHOOKV)
    if(SHV_DIR)
        target_include_directories(YimMenuCustom PRIVATE ${SHV_DIR}/inc)
    endif()
    if(MINHOOK_DIR)
        target_include_directories(YimMenuCustom PRIVATE ${MINHOOK_DIR}/include)
        if(EXISTS ${MINHOOK_DIR}/lib/MinHook.x64.lib)
            target_link_libraries(YimMenuCustom PRIVATE ${MINHOOK_DIR}/lib/MinHook.x64.lib)
        elseif(EXISTS ${MINHOOK_DIR}/build/MinHook.x64.lib)
            target_link_libraries(YimMenuCustom PRIVATE ${MINHOOK_DIR}/build/MinHook.x64.lib)
        endif()
    endif()
endif()

target_compile_definitions(YimMenuCustom PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Ensure Windows-style DLL name without 'lib' prefix
set_target_properties(YimMenuCustom PROPERTIES
    PREFIX ""
    OUTPUT_NAME "YimMenuCustom"
)

install(TARGETS YimMenuCustom
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .
)

if(COPY_FSL_VERSION_DLL)
    set(FSL_DLL_PATH "${CMAKE_SOURCE_DIR}/version.dll" CACHE FILEPATH "Path to FSL version.dll")
    add_custom_command(TARGET YimMenuCustom POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FSL_DLL_PATH}" "$<TARGET_FILE_DIR:YimMenuCustom>/version.dll"
        COMMENT "Copying FSL version.dll to output directory"
    )
endif()

# ImGui vendoring/linking (after target exists)
if(ENABLE_IMGUI)
    # If vendored imgui is present, add it
    if(EXISTS ${CMAKE_SOURCE_DIR}/external/imgui/CMakeLists.txt)
        add_subdirectory(${CMAKE_SOURCE_DIR}/external/imgui)
        target_link_libraries(YimMenuCustom PRIVATE imgui)
        # external/imgui/CMakeLists.txt defines IMGUI_SOURCE_DIR via FetchContent
        if (DEFINED IMGUI_SOURCE_DIR)
            # Prefer the fetched ImGui headers first (build area) then source dir
            target_include_directories(YimMenuCustom PRIVATE ${CMAKE_BINARY_DIR}/_deps/imgui_populate-src ${IMGUI_SOURCE_DIR} ${IMGUI_SOURCE_DIR}/backends)
        else()
            target_include_directories(YimMenuCustom PRIVATE ${CMAKE_SOURCE_DIR}/external/imgui)
        endif()
        # Ensure backends folder (for imgui_impl_win32.h / imgui_impl_dx11.h) is reachable
        target_include_directories(YimMenuCustom PRIVATE ${CMAKE_SOURCE_DIR}/external/imgui ${CMAKE_SOURCE_DIR}/external/imgui/backends)
        # Link D3D11/DXGI for the DX11 backend
        find_library(D3D11_LIB d3d11)
        find_library(DXGI_LIB dxgi)
        if(D3D11_LIB)
            target_link_libraries(YimMenuCustom PRIVATE ${D3D11_LIB})
        else()
            target_link_libraries(YimMenuCustom PRIVATE d3d11)
        endif()
        if(DXGI_LIB)
            target_link_libraries(YimMenuCustom PRIVATE ${DXGI_LIB})
        else()
            target_link_libraries(YimMenuCustom PRIVATE dxgi)
        endif()
        # Link additional system libs used by the ImGui backends
        find_library(D3DCOMPILER_LIB d3dcompiler)
        find_library(DWMAPI_LIB dwmapi)
        if(D3DCOMPILER_LIB)
            target_link_libraries(YimMenuCustom PRIVATE ${D3DCOMPILER_LIB})
        else()
            target_link_libraries(YimMenuCustom PRIVATE d3dcompiler)
        endif()
        if(DWMAPI_LIB)
            target_link_libraries(YimMenuCustom PRIVATE ${DWMAPI_LIB})
        else()
            target_link_libraries(YimMenuCustom PRIVATE dwmapi)
        endif()

        # Ensure headers for imgui + backends are visible even when FetchContent vars are unset
        target_include_directories(YimMenuCustom PRIVATE
            ${CMAKE_SOURCE_DIR}/external/imgui
            ${CMAKE_SOURCE_DIR}/external/imgui/backends
            ${CMAKE_BINARY_DIR}/_deps/imgui_populate-src
            ${CMAKE_BINARY_DIR}/_deps/imgui_populate-src/backends
        )
        target_compile_options(YimMenuCustom PRIVATE
            -I${CMAKE_SOURCE_DIR}/external/imgui
            -I${CMAKE_SOURCE_DIR}/external/imgui/backends
            -I${CMAKE_BINARY_DIR}/_deps/imgui_populate-src
            -I${CMAKE_BINARY_DIR}/_deps/imgui_populate-src/backends
        )
    else()
        # Fallback: use vendored headers/sources directly when no ImGui CMake project is present
        set(IMGUI_VENDOR_DIR "${CMAKE_SOURCE_DIR}/external/imgui")
        set(IMGUI_BACKEND_DIR "${IMGUI_VENDOR_DIR}/backends")

        target_include_directories(YimMenuCustom PRIVATE ${IMGUI_VENDOR_DIR} ${IMGUI_BACKEND_DIR})

        target_sources(YimMenuCustom PRIVATE
            ${IMGUI_VENDOR_DIR}/imgui.cpp
            ${IMGUI_VENDOR_DIR}/imgui_draw.cpp
            ${IMGUI_VENDOR_DIR}/imgui_tables.cpp
            ${IMGUI_VENDOR_DIR}/imgui_widgets.cpp
            ${IMGUI_BACKEND_DIR}/imgui_impl_dx11.cpp
            ${IMGUI_BACKEND_DIR}/imgui_impl_win32.cpp
        )

        # Link D3D/DXGI dependencies for the backends
        find_library(D3D11_LIB d3d11)
        find_library(DXGI_LIB dxgi)
        find_library(D3DCOMPILER_LIB d3dcompiler)
        find_library(DWMAPI_LIB dwmapi)

        if(D3D11_LIB)
            target_link_libraries(YimMenuCustom PRIVATE ${D3D11_LIB})
        else()
            target_link_libraries(YimMenuCustom PRIVATE d3d11)
        endif()

        if(DXGI_LIB)
            target_link_libraries(YimMenuCustom PRIVATE ${DXGI_LIB})
        else()
            target_link_libraries(YimMenuCustom PRIVATE dxgi)
        endif()

        if(D3DCOMPILER_LIB)
            target_link_libraries(YimMenuCustom PRIVATE ${D3DCOMPILER_LIB})
        else()
            target_link_libraries(YimMenuCustom PRIVATE d3dcompiler)
        endif()

        if(DWMAPI_LIB)
            target_link_libraries(YimMenuCustom PRIVATE ${DWMAPI_LIB})
        else()
            target_link_libraries(YimMenuCustom PRIVATE dwmapi)
        endif()

        if(IMGUI_DIR)
            target_include_directories(YimMenuCustom PRIVATE ${IMGUI_DIR})
        endif()
    endif()
endif()
