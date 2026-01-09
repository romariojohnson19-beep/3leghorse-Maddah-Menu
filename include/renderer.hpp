#pragma once
#include <cstdint>

// Forward declare to avoid forcing DXGI includes on all callers
struct IDXGISwapChain;

namespace renderer {
    void init();
    void shutdown();
    void draw_overlay();
    void set_theme_gold_black();
    // Optional: provide platform-specific init when ImGui is enabled
    bool imgui_init(void* hwnd, void* d3d_device, void* d3d_context);
    void imgui_shutdown();

    // Called from the real Present hook; expects a valid swap chain.
    bool on_present(IDXGISwapChain* swap_chain);

    // Called from ResizeBuffers hook before and after the resize.
    void on_resize_begin();
    void on_resize_end(IDXGISwapChain* swap_chain);

    // Notify the renderer that protections blocked an event (thread-safe).
    void notify_blocked_event(uint64_t event_hash, uint32_t total_blocked);
}
