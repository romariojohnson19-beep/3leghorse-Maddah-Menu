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
    // Create context
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Config flags
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Keyboard controls

    // Theme
    set_theme_gold_black();

    // Backend init
    if (!ImGui_ImplWin32_Init(hwnd)) {
        return false;
    }
    if (!ImGui_ImplDX11_Init((ID3D11Device*)d3d_device, (ID3D11DeviceContext*)d3d_context)) {
        ImGui_ImplWin32_Shutdown();
        return false;
    }

    // Font (default or custom)
    io.Fonts->AddFontDefault();
    // Or embedded/custom:
    // static const unsigned char roboto_ttf[] = { /* compressed TTF data */ };
    // io.Fonts->AddFontFromMemoryCompressedTTF(roboto_compressed_data, roboto_compressed_size, 18.0f);

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
    if (!g_initialized) return;
    if (!g_imgui_ready) {
        if (!g_warned_not_ready) {
            OutputDebugStringA("[renderer] draw_overlay called but ImGui not initialized. Call renderer::imgui_init(hwnd, device, context) from your Present hook.\n");
            g_warned_not_ready = true;
        }
        return;
    }
    // NewFrame for platform/backends
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Toasts
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##3leghorse_toasts", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs);
    ImGui::SetCursorPos(ImVec2(ImGui::GetIO().DisplaySize.x - 420, 20));
    double now = ImGui::GetTime();
    for (auto it = g_toasts.begin(); it != g_toasts.end();) {
        double age = now - it->created_at;
        if (age > it->lifetime) { it = g_toasts.erase(it); continue; }
        float alpha = 1.0f - static_cast<float>(age / it->lifetime);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(g_gold.x, g_gold.y, g_gold.z, alpha));
        ImGui::Text("⚠️ Blocked crash event 0x%llX (Total: %u)", (unsigned long long)it->event_hash, (unsigned)it->total_blocked);
        ImGui::PopStyleColor();
        ++it;
    }
    ImGui::End();

    // Watermark & Theme editor
    ImGui::PushStyleColor(ImGuiCol_Text, g_gold);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##3leghorse_watermark", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoBackground);
    ImGui::SetCursorPos(ImVec2(ImGui::GetIO().DisplaySize.x - 220, ImGui::GetIO().DisplaySize.y - 30));
    ImGui::TextColored(g_gold, "3leghorse.com - 3leghorse Menu v1.0");
    ImGui::End();

    ImGui::Begin("3leghorse Theme", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    float col[4] = { g_settings.theme.r, g_settings.theme.g, g_settings.theme.b, g_settings.theme.a };
    if (ImGui::ColorEdit4("Theme Color (Gold)", col)) { g_settings.theme.r = col[0]; g_settings.theme.g = col[1]; g_settings.theme.b = col[2]; g_settings.theme.a = col[3]; g_gold = ImVec4(col[0], col[1], col[2], col[3]); }
    int hotkey = g_settings.toggle_hotkey; if (ImGui::InputInt("Toggle Hotkey (VK)", &hotkey)) g_settings.toggle_hotkey = hotkey;
    if (ImGui::Button("Save Settings")) cfg::save(g_settings);
    ImGui::Separator(); ImGui::Text("Protections"); uint32_t blocked = protections::blocked_count(); ImGui::Text("Blocked Events: %u", blocked);
    if (ImGui::Button("Reload Bad Events")) protections::reload_bad_events_from_ini();
    ImGui::End();
    ImGui::PopStyleColor();

    // Main menu window with feature toggles
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("3leghorse Menu", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    bool vis = menu_stub::overlay_visible();
    if (ImGui::Checkbox("Overlay Visible", &vis)) {
        menu_stub::set_overlay_visible(vis);
    }
    ImGui::Separator();
    ImGui::Text("Features");
    menu_stub::for_each_feature([](menu_stub::Feature& f){
        ImGui::Checkbox(f.name, &f.enabled);
    });
    ImGui::Separator();
    ImGui::Text("Hotkey (VK): %d", cfg::get().toggle_hotkey);
    ImGui::TextUnformatted(native_interface::status().c_str());
    ImGui::Text("Blocked Events: %u", protections::blocked_count());
    ImGui::End();

    // Render
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void notify_blocked_event(uint64_t event_hash, uint32_t total_blocked) {
    if (!g_initialized) return;
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
void notify_blocked_event(uint64_t event_hash, uint32_t total_blocked) {
    char buf[128]; sprintf(buf, "[3leghorse] Blocked event 0x%llX total=%u\n", (unsigned long long)event_hash, (unsigned)total_blocked); OutputDebugStringA(buf);
}
}

#endif
