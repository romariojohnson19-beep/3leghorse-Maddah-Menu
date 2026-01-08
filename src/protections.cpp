#include "protections.hpp"
#include "native_interface.hpp"
#include <windows.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <mutex>
#include "config.hpp"
#include "renderer.hpp"

namespace {
std::atomic_bool g_block_crash{false};
std::atomic_bool g_force_host{false};

// Curated list of known-bad event hashes (common crash/kick vectors)
// Values collected from community reports (unsigned 32 / 64-bit compatible)
static std::vector<uint64_t> bad_event_hashes = {
    0x5F59E4C8ULL,   // sound crash variant
    0xBCEC9F7CULL,   // CEO/MC kick variant
    0xAD6D9C95ULL,   // vehicle-related crash variant
    static_cast<uint64_t>(static_cast<int64_t>(-1386010354LL)), // signed variant preserved
};

static std::mutex g_bad_hash_mutex;

bool is_bad_event(uint64_t hash) {
    std::lock_guard<std::mutex> lk(g_bad_hash_mutex);
    for (auto h : bad_event_hashes) if (h == hash) return true;
    return false;
}

// Optional fetcher registered by the fork-specific hook to supply event hashes.
static protections::event_fetcher_t g_event_fetcher = nullptr;
static std::atomic_uint32_t g_blocked_counter{0};
}

namespace protections {
void init() {
    OutputDebugStringA("[YimMenuCustom] protections init\n");
    // Load bad-event list on startup
    protections::reload_bad_events_from_ini();
}
void shutdown() {
    OutputDebugStringA("[YimMenuCustom] protections shutdown\n");
    // Reset blocked counter if configured to do so on session end
    if (cfg::get().reset_blocked_on_session) {
        g_blocked_counter.store(0);
    }
}
void set_block_crash_events(bool enabled) {
    g_block_crash.store(enabled);
    OutputDebugStringA(enabled ? "[YimMenuCustom] block crash events ON\n" : "[YimMenuCustom] block crash events OFF\n");
    if (enabled) start_event_thread(); else stop_event_thread();
}
void set_force_host(bool enabled) {
    g_force_host.store(enabled);
    OutputDebugStringA(enabled ? "[YimMenuCustom] force host ON\n" : "[YimMenuCustom] force host OFF\n");
    // TODO: implement session force-host logic
}

void set_event_fetcher(event_fetcher_t fetcher) {
    g_event_fetcher = std::move(fetcher);
}

bool is_bad_event(uint64_t hash) {
    return ::is_bad_event(hash);
}

void increment_blocked_count() {
    g_blocked_counter.fetch_add(1, std::memory_order_relaxed);
}

uint32_t blocked_count() {
    return g_blocked_counter.load(std::memory_order_relaxed);
}

    static std::string protections_ini_path() {
        char buf[MAX_PATH];
        GetModuleFileNameA(NULL, buf, MAX_PATH);
        std::string p(buf);
        auto pos = p.find_last_of("\\/");
        if (pos != std::string::npos) p = p.substr(0, pos);
        return p + "\\3leghorse_protections.ini";
    }

    void reload_bad_events_from_ini() {
        std::string path = protections_ini_path();
        std::ifstream ifs(path);
        if (!ifs.is_open()) return;

        std::vector<uint64_t> new_list;
        std::string line;
        while (std::getline(ifs, line)) {
            auto trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
            if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') continue;
            std::size_t eq = trimmed.find('=');
            std::string token = (eq == std::string::npos) ? trimmed : trimmed.substr(eq + 1);
            std::istringstream iss(token);
            uint64_t val = 0;
            if (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0) {
                iss >> std::hex >> val;
            } else {
                iss >> val;
            }
            if (val != 0) new_list.push_back(val);
        }

        if (!new_list.empty()) {
            std::lock_guard<std::mutex> lk(g_bad_hash_mutex);
            bad_event_hashes = std::move(new_list);
        }
    }

// Event thread
static std::atomic_bool g_event_thread_running{false};
static std::thread g_event_thread;

static void event_thread_fn() {
    using namespace std::chrono_literals;
    OutputDebugStringA("[YimMenuCustom] protections event thread started\n");
        while (g_event_thread_running.load()) {
            uint64_t hash = 0;
            bool have = false;
            // Prefer a registered fetcher (hooked into fork). Fallback to native poller.
            if (g_event_fetcher) {
                have = g_event_fetcher(hash);
            } else {
                have = native_interface::poll_next_script_event(hash);
            }

            if (have) {
                if (g_block_crash.load() && is_bad_event(hash)) {
                    // In a real hook you'd ack/consume the event. Here we log/block.
                    protections::increment_blocked_count();
                    uint32_t total = protections::blocked_count();
                    // Notify UI (ImGui) of the blocked event
                    renderer::notify_blocked_event(hash, total);
                    OutputDebugStringA("[YimMenuCustom] Blocked bad event\n");
                    continue; // skip further processing
                }
            }
            std::this_thread::sleep_for(50ms);
        }
    OutputDebugStringA("[YimMenuCustom] protections event thread stopping\n");
}

void start_event_thread() {
    if (g_event_thread_running.exchange(true)) return;
    g_event_thread = std::thread(event_thread_fn);
}

void stop_event_thread() {
    if (!g_event_thread_running.exchange(false)) return;
    if (g_event_thread.joinable()) g_event_thread.join();
}
} // namespace protections
