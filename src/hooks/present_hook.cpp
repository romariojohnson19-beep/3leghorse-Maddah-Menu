#include "hooks/present_hook.hpp"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <atomic>

#include "hook_helpers.hpp"
#include "menu_stub.hpp"

#if defined(__has_include)
#  if __has_include(<MinHook.h>)
#    include <MinHook.h>
#    define HAVE_MINHOOK 1
#  else
#    define HAVE_MINHOOK 0
#  endif
#else
#  define HAVE_MINHOOK 0
#endif

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace {
#if HAVE_MINHOOK
using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
static PresentFn g_orig_present = nullptr;
static std::atomic_bool g_hooked{false};
static WNDPROC g_orig_wndproc = nullptr;
static HWND g_hwnd = nullptr;

LRESULT CALLBACK wndproc_hook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (hook_helpers::on_wndproc(hWnd, msg, wParam, lParam)) {
        return 1; // ImGui consumed
    }
    return CallWindowProc(g_orig_wndproc, hWnd, msg, wParam, lParam);
}

HRESULT __stdcall hk_present(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) {
    if (!swap_chain) {
        return g_orig_present ? g_orig_present(swap_chain, sync_interval, flags) : S_OK;
    }

    if (!g_hwnd) {
        DXGI_SWAP_CHAIN_DESC desc{};
        if (SUCCEEDED(swap_chain->GetDesc(&desc))) {
            g_hwnd = desc.OutputWindow;
            if (g_hwnd && !g_orig_wndproc) {
                g_orig_wndproc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)wndproc_hook);
            }
        }
    }

    hook_helpers::on_present(swap_chain);
    return g_orig_present ? g_orig_present(swap_chain, sync_interval, flags) : S_OK;
}

void* resolve_present_target() {
    // Create a dummy DX11 device and swap chain to fetch the vtable Present pointer
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = GetForegroundWindow();
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    IDXGISwapChain* swapChain = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;

    HWND dummy_wnd = nullptr;
    WNDCLASSEX wc{ sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"3leghorse_dummy", nullptr };
    if (!RegisterClassEx(&wc)) {
        return nullptr;
    }
    dummy_wnd = CreateWindow(wc.lpszClassName, L"3leghorse_dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    sd.OutputWindow = dummy_wnd;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevelArray, 2,
                                               D3D11_SDK_VERSION, &sd, &swapChain, &device, &featureLevel, &context);

    void* target = nullptr;
    if (SUCCEEDED(hr) && swapChain) {
        void** vtable = *reinterpret_cast<void***>(swapChain);
        target = vtable[8]; // Present
    }

    if (swapChain) swapChain->Release();
    if (device) device->Release();
    if (context) context->Release();
    if (dummy_wnd) {
        DestroyWindow(dummy_wnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
    }
    return target;
}
#endif // HAVE_MINHOOK
} // namespace

namespace hooks {

bool init_present_hook() {
#if HAVE_MINHOOK
    if (g_hooked.load(std::memory_order_acquire)) return true;

    void* target = resolve_present_target();
    if (!target) {
        OutputDebugStringA("[hooks] Failed to resolve IDXGISwapChain::Present target\n");
        return false;
    }

    MH_STATUS init_status = MH_Initialize();
    if (init_status != MH_OK && init_status != MH_ERROR_ALREADY_INITIALIZED) {
        OutputDebugStringA("[hooks] MH_Initialize failed\n");
        return false;
    }
    if (MH_CreateHook(target, &hk_present, reinterpret_cast<void**>(&g_orig_present)) != MH_OK) {
        OutputDebugStringA("[hooks] MH_CreateHook failed\n");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        OutputDebugStringA("[hooks] MH_EnableHook failed\n");
        MH_RemoveHook(target);
        return false;
    }

    g_hooked.store(true, std::memory_order_release);
    OutputDebugStringA("[hooks] Present hook installed\n");
    return true;
#else
    OutputDebugStringA("[hooks] MinHook not available; present hook not installed\n");
    return false;
#endif
}

void shutdown_present_hook() {
#if HAVE_MINHOOK
    if (!g_hooked.load(std::memory_order_acquire)) return;
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    g_hooked.store(false, std::memory_order_release);
    OutputDebugStringA("[hooks] Present hook removed\n");
#endif
}

} // namespace hooks
