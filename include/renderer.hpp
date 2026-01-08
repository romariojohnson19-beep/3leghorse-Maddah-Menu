#pragma once
#include <cstdint>

namespace renderer {
    void init();
    void shutdown();
    void draw_overlay();
    void set_theme_gold_black();
    // Optional: provide platform-specific init when ImGui is enabled
    bool imgui_init(void* hwnd, void* d3d_device, void* d3d_context);
    void imgui_shutdown();
    // Notify the renderer that protections blocked an event (thread-safe).
    void notify_blocked_event(uint64_t event_hash, uint32_t total_blocked);
}
