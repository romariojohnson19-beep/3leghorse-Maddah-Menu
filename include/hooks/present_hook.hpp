#pragma once

struct IDXGISwapChain;

namespace hooks {
// Initialize Present/ResizeBuffers hooks on the real swapchain vtable (Present index 8, ResizeBuffers index 13).
// If swap_chain is nullptr, the call logs and returns false; call set_swapchain_ptr later to retry.
bool init_present_hook(IDXGISwapChain* swap_chain = nullptr);

// Provide the real game swapchain pointer after it becomes available; installs hooks immediately if not already installed.
void set_swapchain_ptr(IDXGISwapChain* swap_chain);

// Disable the Present/ResizeBuffers hooks and restore WndProc if installed.
void shutdown_present_hook();
}
