#include "hook_helpers.hpp"
#include "renderer.hpp"

#if defined(ENABLE_IMGUI)
#include <d3d11.h>
#include <dxgi.h>
#include "../external/imgui/imgui.h"
#include "../external/imgui/backends/imgui_impl_win32.h"

// Forward declare the WndProc helper (intentionally commented out in the header)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace hook_helpers {

bool on_present(IDXGISwapChain* swap_chain) {
    if (!swap_chain) return false;

    static bool initialized = false;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    DXGI_SWAP_CHAIN_DESC desc{};

    bool ok = false;
    if (SUCCEEDED(swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&device)) && device) {
        device->GetImmediateContext(&context);
        if (SUCCEEDED(swap_chain->GetDesc(&desc))) {
            if (!initialized) {
                initialized = renderer::imgui_init(desc.OutputWindow, device, context);
            }
            renderer::draw_overlay();
            ok = true;
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
bool on_wndproc(void*, unsigned int, unsigned long, long) { return false; }
} // namespace hook_helpers

#endif
