#include "menu_stub.hpp"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "native_interface.hpp"
#include "protections.hpp"
#include "renderer.hpp"
#include "config.hpp"

namespace {
std::atomic_bool g_running{false};
std::thread g_worker;
std::mutex g_mutex;
std::vector<menu_stub::Feature> g_features{
    {"God Mode", false},
    {"Infinite Ammo", false},
    {"Stealth Protections", true},
    {"Free Purchase", false},
    {"Block Crash Events", true},
    {"Force Host", false},
};

static std::atomic_bool g_overlay_visible{true};

void log(const std::string& msg) {
    OutputDebugStringA(msg.c_str());
}

void tick_loop() {
    using namespace std::chrono_literals;
    log("[YimMenuCustom] tick loop start\n");
    while (g_running.load(std::memory_order_acquire)) {
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            for (auto& f : g_features) {
                const std::string name = f.name;
                if (name == "God Mode") {
                    native_interface::set_player_invincible(f.enabled);
                } else if (name == "Infinite Ammo") {
                    native_interface::set_infinite_ammo(f.enabled);
                } else if (name == "Stealth Protections") {
                    native_interface::apply_stealth_protections(f.enabled);
                } else if (name == "Free Purchase") {
                    native_interface::enable_free_purchase(f.enabled);
                } else if (name == "Block Crash Events") {
                    protections::set_block_crash_events(f.enabled);
                } else if (name == "Force Host") {
                    protections::set_force_host(f.enabled);
                }
            }
        }
        // Hotkey handler: toggle overlay
        int vk = cfg::get().toggle_hotkey;
        if (GetAsyncKeyState(vk) & 1) {
            g_overlay_visible.store(!g_overlay_visible.load());
        }
        if (g_overlay_visible.load()) renderer::draw_overlay();
        std::this_thread::sleep_for(200ms);
    }
    log("[YimMenuCustom] tick loop stop\n");
}
} // namespace

namespace menu_stub {

void init() {
    if (g_running.exchange(true)) {
        log("[YimMenuCustom] init skipped (already running)\n");
        return;
    }
    native_interface::init();
    protections::init();
    renderer::init();
    log("[YimMenuCustom] init called\n");
    g_worker = std::thread(tick_loop);
}

void shutdown() {
    if (!g_running.exchange(false)) {
        return;
    }
    log("[YimMenuCustom] shutdown requested\n");
    if (g_worker.joinable()) {
        g_worker.join();
    }
    native_interface::shutdown();
    protections::shutdown();
    renderer::shutdown();
    log("[YimMenuCustom] shutdown complete\n");
}

void for_each_feature(const std::function<void(menu_stub::Feature&)>& fn) {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto& f : g_features) {
        fn(f);
    }
}

bool overlay_visible() {
    return g_overlay_visible.load(std::memory_order_relaxed);
}

void set_overlay_visible(bool visible) {
    g_overlay_visible.store(visible, std::memory_order_relaxed);
}

} // namespace menu_stub
