#include "hooks/present_hook.hpp"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <atomic>
#include <cstdio>
#include <vector>
#include <cstring>

#include "hook_helpers.hpp"
#include "menu_stub.hpp"
#include "renderer.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

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

namespace {
#if HAVE_MINHOOK
using PresentFn       = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

// Forward declarations
HRESULT __stdcall hk_present(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);
HRESULT __stdcall hk_resizebuffers(IDXGISwapChain* swap_chain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags);

static PresentFn g_orig_present   = nullptr;
static ResizeBuffersFn g_orig_resize = nullptr;
static std::atomic_bool g_hooks_installed{false};
static std::atomic_bool g_min_hook_ready{false};
static WNDPROC g_orig_wndproc     = nullptr;
static HWND g_hwnd                = nullptr;
static std::atomic<IDXGISwapChain*> g_swap_chain{nullptr};

static std::vector<int> pattern_to_bytes(const char* pattern) {
    std::vector<int> bytes;
    const char* end = pattern + std::strlen(pattern);
    for (const char* current = pattern; current < end; ++current) {
        if (*current == ' ') continue;
        if (*current == '?') {
            ++current; // skip second '?' if present
            bytes.push_back(-1);
        } else {
            char* next = nullptr;
            bytes.push_back(static_cast<int>(std::strtoul(current, &next, 16)));
            current = next - 1;
        }
    }
    return bytes;
}

static uintptr_t find_pattern(const char* module_name, const char* signature) {
    HMODULE mod = module_name ? GetModuleHandleA(module_name) : GetModuleHandleA(nullptr);
    if (!mod) return 0;

    auto* dos = reinterpret_cast<PIMAGE_DOS_HEADER>(mod);
    auto* nt  = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uint8_t*>(mod) + dos->e_lfanew);
    size_t size = nt->OptionalHeader.SizeOfImage;
    auto* data  = reinterpret_cast<uint8_t*>(mod);

    auto bytes = pattern_to_bytes(signature);
    size_t bytes_size = bytes.size();

    for (size_t i = 0; i + bytes_size < size; ++i) {
        bool found = true;
        for (size_t j = 0; j < bytes_size; ++j) {
            if (bytes[j] != -1 && data[i + j] != bytes[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return reinterpret_cast<uintptr_t>(&data[i]);
        }
    }
    return 0;
}

static IDXGISwapChain* resolve_swapchain_from_pattern() {
    // Signature taken from YimMenu swapchain pointer (Legacy): 48 8B 0D ? ? ? ? 48 8B 01 44 8D 43 01 33 D2 FF 50 40 8B C8
    constexpr const char* k_swap_sig = "48 8B 0D ? ? ? ? 48 8B 01 44 8D 43 01 33 D2 FF 50 40 8B C8";
    uintptr_t addr = find_pattern(nullptr, k_swap_sig);
    if (!addr) {
        OutputDebugStringA("[present_hook] swapchain pattern not found\n");
        return nullptr;
    }

    int32_t rip = *reinterpret_cast<int32_t*>(addr + 3);
    auto swap_ptr = reinterpret_cast<IDXGISwapChain**>(addr + 7 + rip);
    IDXGISwapChain* sc = swap_ptr ? *swap_ptr : nullptr;

    char buf[200];
    sprintf(buf, "[present_hook] swapchain pattern resolved ptr=%p swapchain=%p\n", swap_ptr, sc);
    OutputDebugStringA(buf);
    return sc;
}

LRESULT CALLBACK wndproc_hook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (hook_helpers::on_wndproc(hWnd, msg, wParam, lParam)) {
        return 1; // ImGui consumed input
    }
    return CallWindowProc(g_orig_wndproc, hWnd, msg, wParam, lParam);
}

static void install_wndproc_if_needed(IDXGISwapChain* swap_chain) {
    if (g_hwnd || !swap_chain) return;

    DXGI_SWAP_CHAIN_DESC desc{};
    if (SUCCEEDED(swap_chain->GetDesc(&desc))) {
        g_hwnd = desc.OutputWindow;
        char buf[160];
        sprintf(buf, "[present_hook] Captured HWND=%p from swapchain=%p\n", g_hwnd, swap_chain);
        OutputDebugStringA(buf);
        if (g_hwnd && !g_orig_wndproc) {
            g_orig_wndproc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)wndproc_hook);
            OutputDebugStringA("[present_hook] Installed WndProc hook on real game window\n");
        }
    }
}

static bool install_swapchain_hooks(IDXGISwapChain* swap_chain) {
    if (g_hooks_installed.load(std::memory_order_acquire)) return true;
    if (!swap_chain) {
        OutputDebugStringA("[present_hook] swap_chain is null; cannot install hooks\n");
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(swap_chain);
    void* present_target = vtable[8];
    void* resize_target  = vtable[13];

    char buf[200];
    sprintf(buf, "[present_hook] Installing hooks on real swapchain=%p (Present=%p, ResizeBuffers=%p)\n", swap_chain, present_target, resize_target);
    OutputDebugStringA(buf);

    if (MH_CreateHook(present_target, &hk_present, reinterpret_cast<void**>(&g_orig_present)) != MH_OK ||
        MH_EnableHook(present_target) != MH_OK) {
        OutputDebugStringA("[present_hook] Failed to hook Present on real swapchain\n");
        return false;
    }

    if (MH_CreateHook(resize_target, &hk_resizebuffers, reinterpret_cast<void**>(&g_orig_resize)) != MH_OK ||
        MH_EnableHook(resize_target) != MH_OK) {
        OutputDebugStringA("[present_hook] Failed to hook ResizeBuffers on real swapchain\n");
        MH_RemoveHook(present_target);
        g_orig_present = nullptr;
        return false;
    }

    install_wndproc_if_needed(swap_chain);
    g_swap_chain.store(swap_chain, std::memory_order_release);
    g_hooks_installed.store(true, std::memory_order_release);
    OutputDebugStringA("[present_hook] Present/ResizeBuffers hooks installed successfully\n");
    return true;
}

HRESULT __stdcall hk_present(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) {
    if (flags & DXGI_PRESENT_TEST) {
        return g_orig_present ? g_orig_present(swap_chain, sync_interval, flags) : S_OK;
    }

    install_wndproc_if_needed(swap_chain);

    // Ensure we install on the exact swapchain instance we see in the first real Present.
    if (!g_hooks_installed.load(std::memory_order_acquire) && swap_chain) {
        install_swapchain_hooks(swap_chain);
    }

    hook_helpers::on_present(swap_chain);
    return g_orig_present ? g_orig_present(swap_chain, sync_interval, flags) : S_OK;
}

HRESULT __stdcall hk_resizebuffers(IDXGISwapChain* swap_chain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) {
    OutputDebugStringA("[hk_resizebuffers] ResizeBuffers called\n");
    renderer::on_resize_begin();
    HRESULT hr = g_orig_resize ? g_orig_resize(swap_chain, bufferCount, width, height, newFormat, swapChainFlags) : S_OK;
    if (SUCCEEDED(hr)) {
        renderer::on_resize_end(swap_chain);
    }
    return hr;
}
#endif // HAVE_MINHOOK
} // namespace

namespace hooks {

bool init_present_hook(IDXGISwapChain* swap_chain) {
#if HAVE_MINHOOK
    if (!g_min_hook_ready.load(std::memory_order_acquire)) {
        MH_STATUS init_status = MH_Initialize();
        if (init_status != MH_OK && init_status != MH_ERROR_ALREADY_INITIALIZED) {
            char buf[128];
            sprintf(buf, "[hooks] MH_Initialize failed: %d\n", init_status);
            OutputDebugStringA(buf);
            return false;
        }
        g_min_hook_ready.store(true, std::memory_order_release);
    }

    IDXGISwapChain* target = swap_chain ? swap_chain : resolve_swapchain_from_pattern();
    if (!target) {
        OutputDebugStringA("[hooks] init_present_hook: swap_chain is null; call set_swapchain_ptr when available\n");
        return false;
    }

    return install_swapchain_hooks(target);
#else
    OutputDebugStringA("[hooks] MinHook not available; present hook not installed\n");
    return false;
#endif
}

void set_swapchain_ptr(IDXGISwapChain* swap_chain) {
#if HAVE_MINHOOK
    if (swap_chain) {
        if (!g_min_hook_ready.load(std::memory_order_acquire)) {
            MH_STATUS init_status = MH_Initialize();
            if (init_status == MH_OK || init_status == MH_ERROR_ALREADY_INITIALIZED) {
                g_min_hook_ready.store(true, std::memory_order_release);
            }
        }
        g_swap_chain.store(swap_chain, std::memory_order_release);
        if (g_min_hook_ready.load(std::memory_order_acquire)) {
            install_swapchain_hooks(swap_chain);
        }
    } else {
        OutputDebugStringA("[hooks] set_swapchain_ptr called with null pointer\n");
    }
#else
    (void)swap_chain;
#endif
}

void shutdown_present_hook() {
#if HAVE_MINHOOK
    if (g_hooks_installed.load(std::memory_order_acquire)) {
        void* present_target = nullptr;
        void* resize_target  = nullptr;
        if (auto sc = g_swap_chain.load(std::memory_order_acquire)) {
            void** vtable = *reinterpret_cast<void***>(sc);
            present_target = vtable[8];
            resize_target  = vtable[13];
        }

        if (present_target) MH_DisableHook(present_target);
        if (resize_target) MH_DisableHook(resize_target);

        g_hooks_installed.store(false, std::memory_order_release);
        g_orig_present = nullptr;
        g_orig_resize  = nullptr;
    }

    if (g_orig_wndproc && g_hwnd) {
        SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_orig_wndproc);
        g_orig_wndproc = nullptr;
        g_hwnd = nullptr;
    }

    if (g_min_hook_ready.load(std::memory_order_acquire)) {
        MH_Uninitialize();
        g_min_hook_ready.store(false, std::memory_order_release);
    }

    g_swap_chain.store(nullptr, std::memory_order_release);
    OutputDebugStringA("[hooks] Present/ResizeBuffers hooks shut down\n");
#endif
}

} // namespace hooks
