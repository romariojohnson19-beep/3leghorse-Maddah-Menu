#pragma once

#include <cstdint>
#include <Windows.h>

// Minimal helpers to wire ImGui rendering and input from your DX11 present hook
// and Win32 WndProc. These are no-ops when ENABLE_IMGUI is off.

struct IDXGISwapChain;

namespace hook_helpers {
    // Call from your DX11 Present hook. Grabs device/context/HWND from the swapchain,
    // initializes ImGui (renderer::imgui_init) once, then calls renderer::draw_overlay().
    // Returns true if ImGui overlay was drawn this frame.
    bool on_present(IDXGISwapChain* swap_chain);

    // Call from your WndProc before game handling; returns true if ImGui consumed the input.
    bool on_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
}
