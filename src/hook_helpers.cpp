#include "hook_helpers.hpp"
#include "renderer.hpp"
#include "menu_stub.hpp"

#if defined(ENABLE_IMGUI)
#include <d3d11.h>
#include <dxgi.h>
#include <cstdio>
#include "../external/imgui/imgui.h"
#include "../external/imgui/backends/imgui_impl_win32.h"

// Forward declare the WndProc helper (intentionally commented out in the header)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace hook_helpers {

bool on_present(IDXGISwapChain* swap_chain) {
    if (!swap_chain) return false;

    static bool initialized = false;
    static int frame_count = 0;
    frame_count++;

    // More frequent debug output - every 10 frames instead of 100
    if (frame_count % 10 == 0) {
        char buf[128];
        sprintf(buf, "[hook_helpers] on_present called - frame %d, initialized: %d\n", frame_count, initialized);
        OutputDebugStringA(buf);
    }

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    DXGI_SWAP_CHAIN_DESC desc{};

    bool ok = false;
    if (SUCCEEDED(swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&device)) && device) {
        device->GetImmediateContext(&context);
        if (SUCCEEDED(swap_chain->GetDesc(&desc))) {
            if (!initialized) {
                OutputDebugStringA("[hook_helpers] Initializing ImGui\n");
                initialized = renderer::imgui_init(desc.OutputWindow, device, context);
                if (initialized) {
                    OutputDebugStringA("[hook_helpers] ImGui initialized successfully\n");
                } else {
                    OutputDebugStringA("[hook_helpers] ImGui initialization failed\n");
                }
            }
            ok = renderer::on_present(swap_chain);
        }
    }

    if (context) context->Release();
    if (device) device->Release();
    return ok;
}

bool on_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}

} // namespace hook_helpers

#else // ENABLE_IMGUI

namespace hook_helpers {
bool on_present(void*) { return false; }
bool on_wndproc(HWND, UINT, WPARAM, LPARAM) { return false; }
} // namespace hook_helpers

#endif
