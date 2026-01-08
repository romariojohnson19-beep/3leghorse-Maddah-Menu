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
    g_imgui_ready = false;
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

    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("3leghorse Overlay", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
    
    // Test message - make it very visible
    ImGui::TextColored(ImVec4(1,0,0,1), "=== 3LEGHORSE MENU LOADED ===");
    ImGui::TextColored(ImVec4(0,1,0,1), "Press F11 to toggle visibility");
    ImGui::Separator();
    ImGui::Text("3leghorse Menu v1.0");
    ImGui::TextColored(ImVec4(1,1,0,1), "Blocked Events: %u", protections::blocked_count());
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

bool on_present() {
    if (!g_initialized || !g_imgui_ready) {
        if (!g_warned_not_ready) {
            OutputDebugStringA("[renderer] on_present called before imgui_init\n");
            g_warned_not_ready = true;
        }
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
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
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

namespace renderer {
void init() { OutputDebugStringA("[3leghorse] renderer stub init\n"); }
void shutdown() { OutputDebugStringA("[3leghorse] renderer stub shutdown\n"); }
void set_theme_gold_black() { }
void draw_overlay() { OutputDebugStringA("[3leghorse] 3leghorse Menu v1.0 (stub)\n"); }
bool on_present() { return false; }
void notify_blocked_event(uint64_t event_hash, uint32_t total_blocked) {
    char buf[128]; sprintf(buf, "[3leghorse] Blocked event 0x%llX total=%u\n", (unsigned long long)event_hash, (unsigned)total_blocked); OutputDebugStringA(buf);
}
}

#endif
