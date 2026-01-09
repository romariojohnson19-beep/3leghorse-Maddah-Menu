#include "renderer.hpp"

#include <windows.h>
#include <string>
#include <vector>
#include <cstdio>

// Detect ImGui availability at compile time
#if defined(ENABLE_IMGUI) && defined(__has_include)
#  if __has_include(<imgui.h>)
#    include <imgui.h>
#    define HAVE_IMGUI 1
#  else
#    define HAVE_IMGUI 0
#  endif
#else
#  define HAVE_IMGUI 0
#endif

#if HAVE_IMGUI
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "config.hpp"
#include "protections.hpp"
#include "menu_stub.hpp"
#include "native_interface.hpp"
#include <d3d11.h>

struct Toast {
    uint64_t event_hash;
    uint32_t total_blocked;
    double created_at;
    float lifetime;
};

static std::vector<Toast> g_toasts;
static double g_last_toast_time = 0.0;
static bool g_imgui_ready = false;
static bool g_warned_not_ready = false;
static ID3D11RenderTargetView* g_rtv = nullptr;

namespace renderer {

static bool g_initialized = false;
static ImVec4 g_gold = ImVec4(0.92f, 0.74f, 0.0f, 1.0f);
static ImVec4 g_black = ImVec4(0.f, 0.f, 0.f, 1.0f);
static cfg::Settings g_settings;

static void load_settings() {
    if (!cfg::load(g_settings)) cfg::save(g_settings);
    g_gold = ImVec4(g_settings.theme.r, g_settings.theme.g, g_settings.theme.b, g_settings.theme.a);
}

void init() { g_initialized = true; load_settings(); }
void shutdown() { g_initialized = false; g_imgui_ready = false; g_warned_not_ready = false; g_toasts.clear(); g_last_toast_time = 0.0; }

bool imgui_init(void* hwnd, void* d3d_device, void* d3d_context) {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    set_theme_gold_black();

    if (!ImGui_ImplWin32_Init(hwnd)) {
        return false;
    }
    if (!ImGui_ImplDX11_Init(static_cast<ID3D11Device*>(d3d_device), static_cast<ID3D11DeviceContext*>(d3d_context))) {
        ImGui_ImplWin32_Shutdown();
        return false;
    }

    io.Fonts->AddFontDefault();

    g_imgui_ready = true;
    return true;
}

void imgui_shutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
    }
    g_imgui_ready = false;
}

static bool ensure_render_target(IDXGISwapChain* swap_chain) {
    if (g_rtv) return true;
    if (!swap_chain) return false;

    ID3D11Texture2D* back_buffer = nullptr;
    ID3D11Device* device = nullptr;
    if (FAILED(swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&device)) || !device) {
        OutputDebugStringA("[renderer] ensure_render_target: failed to get device\n");
        return false;
    }

    HRESULT hr = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
    if (FAILED(hr) || !back_buffer) {
        device->Release();
        OutputDebugStringA("[renderer] ensure_render_target: failed to get back buffer\n");
        return false;
    }

    hr = device->CreateRenderTargetView(back_buffer, nullptr, &g_rtv);
    back_buffer->Release();
    device->Release();

    if (FAILED(hr) || !g_rtv) {
        OutputDebugStringA("[renderer] ensure_render_target: failed to create RTV\n");
        return false;
    }
    OutputDebugStringA("[renderer] ensure_render_target: RTV created\n");
    return true;
}

void on_resize_begin() {
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
        OutputDebugStringA("[renderer] on_resize_begin: RTV released\n");
    }
}

void on_resize_end(IDXGISwapChain* swap_chain) {
    ensure_render_target(swap_chain);
}

void set_theme_gold_black() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = g_black;
    style.Colors[ImGuiCol_Text] = ImVec4(1.f,1.f,1.f,1.f);
    style.Colors[ImGuiCol_Button] = g_gold;
}

void draw_overlay() {
    if (!g_initialized || !g_imgui_ready) {
        OutputDebugStringA("[renderer] draw_overlay called but not ready\n");
        return;
    }

    OutputDebugStringA("[renderer] Drawing overlay\n");

    // Float the main window near the top-left but let users move it; first-use positions only
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520, 320), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.92f);

    ImGui::Begin("3leghorse Menu", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.f, 1.f), "3LEGHORSE");
        ImGui::SameLine();
        ImGui::Text("ImGui: %s | RTV: %s", g_imgui_ready ? "ready" : "init", g_rtv ? "ok" : "none");
        ImGui::SameLine();
        ImGui::Text("Blocked: %u", protections::blocked_count());
        ImGui::EndMenuBar();
    }

    const auto hotkey = cfg::get().toggle_hotkey;

    ImGui::TextColored(ImVec4(0.6f, 1.f, 0.6f, 1.f), "Overlay visible: %s", menu_stub::overlay_visible() ? "YES" : "NO");
    ImGui::SameLine();
    ImGui::Text("Toggle key: 0x%X (F9 in config)", hotkey);
    ImGui::Separator();

    ImGui::Columns(2, nullptr, false);
    ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.f, 1.f), "Actions");
    ImGui::Separator();
    menu_stub::for_each_feature([](menu_stub::Feature& f) {
        ImGui::Checkbox(f.name, &f.enabled);
    });

    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.f, 1.f), "Status");
    ImGui::Separator();
    ImGui::Text("Present hook: live (DX11)");
    ImGui::Text("RTV ready: %s", g_rtv ? "yes" : "pending");
    ImGui::Text("Blocked events: %u", protections::blocked_count());
    ImGui::Spacing();
    ImGui::TextWrapped("Tips: open GTA V windowed/borderless to see overlay. If you don't see this window, check DebugView for [present_hook] logs and confirm the DLL is injected.");

    ImGui::Columns(1);

    ImGui::Separator();
    ImGui::TextColored(g_gold, "Build: Maddah Menu | DX11 overlay | Hotkey F9");
    ImGui::End();

    // Toast stack
    if (!g_toasts.empty()) {
        ImGui::Begin("##toasts", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        double now = ImGui::GetTime();
        for (auto it = g_toasts.begin(); it != g_toasts.end();) {
            double age = now - it->created_at;
            if (age > it->lifetime) { it = g_toasts.erase(it); continue; }
            float alpha = 1.0f - static_cast<float>(age / it->lifetime);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f,1.f,0.f, alpha));
            ImGui::Text("Blocked 0x%llX (total %u)", (unsigned long long)it->event_hash, (unsigned)it->total_blocked);
            ImGui::PopStyleColor();
            ++it;
        }
        ImGui::End();
    }
}

bool on_present(IDXGISwapChain* swap_chain) {
    if (!g_initialized || !g_imgui_ready) {
        if (!g_warned_not_ready) {
            OutputDebugStringA("[renderer] on_present called before imgui_init\n");
            g_warned_not_ready = true;
        }
        return false;
    }

    // Ensure RTV exists (creates once per swapchain, recreated after resize)
    if (!ensure_render_target(swap_chain)) {
        OutputDebugStringA("[renderer] on_present: failed to ensure RTV\n");
        return false;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (menu_stub::overlay_visible()) {
        draw_overlay();
    }

    ImGui::EndFrame();
    ImGui::Render();
    ID3D11DeviceContext* ctx = nullptr;
    ID3D11Device* dev = nullptr;
    if (SUCCEEDED(swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&dev)) && dev) {
        dev->GetImmediateContext(&ctx);
    }
    if (ctx && g_rtv) {
        ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    }
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    if (ctx) ctx->Release();
    if (dev) dev->Release();
    return true;
}

void notify_blocked_event(uint64_t event_hash, uint32_t total_blocked) {
    if (!g_initialized || !g_imgui_ready) return;
    double now = ImGui::GetTime();
    if (now - g_last_toast_time < 1.0) return; // rate limit
    g_last_toast_time = now;
    g_toasts.push_back({ event_hash, total_blocked, now, 4.0f });
}

} // namespace renderer

#else // no ImGui available -> stubs

#include <stdio.h>
struct IDXGISwapChain;

namespace renderer {
void init() { OutputDebugStringA("[3leghorse] renderer stub init\n"); }
void shutdown() { OutputDebugStringA("[3leghorse] renderer stub shutdown\n"); }
void set_theme_gold_black() { }
void draw_overlay() { OutputDebugStringA("[3leghorse] 3leghorse Menu v1.0 (stub)\n"); }
bool on_present(IDXGISwapChain*) { return false; }
void on_resize_begin() {}
void on_resize_end(IDXGISwapChain*) {}
void notify_blocked_event(uint64_t event_hash, uint32_t total_blocked) {
    char buf[128]; sprintf(buf, "[3leghorse] Blocked event 0x%llX total=%u\n", (unsigned long long)event_hash, (unsigned)total_blocked); OutputDebugStringA(buf);
}
}

#endif
