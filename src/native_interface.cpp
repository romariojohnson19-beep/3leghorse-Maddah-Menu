#include "native_interface.hpp"

#include <windows.h>

#include <atomic>
#include <string>

#ifdef ENABLE_SCRIPTHOOKV
#include <natives.h>
#include <script.h>
#include <MinHook.h>

// Example: GET_HASH_KEY hook
using get_hash_key_fn = Hash(*)(const char*);
get_hash_key_fn orig_get_hash_key = nullptr;

Hash hooked_get_hash_key(const char* str) {
    // Example: obfuscate or randomize string at runtime
    OutputDebugStringA((std::string{"[YimMenuCustom] GET_HASH_KEY called: "} + str + "\n").c_str());
    // Optionally: modify str or return a fake hash for stealth
    return orig_get_hash_key(str);
}
#endif

namespace {
std::atomic_bool g_initialized{false};
}

namespace native_interface {

bool init() {
    g_initialized.store(true, std::memory_order_release);

    #ifdef ENABLE_SCRIPTHOOKV
    if (MH_Initialize() != MH_OK) {
        OutputDebugStringA("[YimMenuCustom] MinHook init failed\n");
    } else {
        // Hook GET_HASH_KEY as an example
        MH_CreateHook(reinterpret_cast<LPVOID>(&GET_HASH_KEY), reinterpret_cast<LPVOID>(&hooked_get_hash_key), reinterpret_cast<LPVOID*>(&orig_get_hash_key));
        MH_EnableHook(MH_ALL_HOOKS);
        OutputDebugStringA("[YimMenuCustom] MinHook GET_HASH_KEY hooked\n");
    }
    OutputDebugStringA("[YimMenuCustom] native interface init (ScriptHookV path)\n");
    #else
    OutputDebugStringA("[YimMenuCustom] native interface init (stub)\n");
    #endif
    return true;
}

void shutdown() {
    if (!g_initialized.exchange(false)) return;
    #ifdef ENABLE_SCRIPTHOOKV
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    OutputDebugStringA("[YimMenuCustom] native interface shutdown (ScriptHookV path)\n");
    #else
    OutputDebugStringA("[YimMenuCustom] native interface shutdown (stub)\n");
    #endif
}

void set_player_invincible(bool enabled) {
    if (!g_initialized.load()) return;
    OutputDebugStringA(enabled ? "[YimMenuCustom] invincible ON (stub)\n" : "[YimMenuCustom] invincible OFF (stub)\n");
#ifdef ENABLE_SCRIPTHOOKV
    PLAYER::SET_PLAYER_INVINCIBLE(PLAYER::PLAYER_ID(), enabled);
#endif
}

void set_infinite_ammo(bool enabled) {
    if (!g_initialized.load()) return;
    OutputDebugStringA(enabled ? "[YimMenuCustom] infinite ammo ON (stub)\n" : "[YimMenuCustom] infinite ammo OFF (stub)\n");
#ifdef ENABLE_SCRIPTHOOKV
    // Simple approach: set infinite ammo flags
    WEAPON::SET_PED_INFINITE_AMMO_CLIP(PLAYER::PLAYER_PED_ID(), enabled);
    WEAPON::SET_PED_INFINITE_AMMO(PLAYER::PLAYER_PED_ID(), enabled, 0);
#endif
}

void apply_stealth_protections(bool enabled) {
    if (!g_initialized.load()) return;
    OutputDebugStringA(enabled ? "[YimMenuCustom] protections ON (stub)\n" : "[YimMenuCustom] protections OFF (stub)\n");
#ifdef ENABLE_SCRIPTHOOKV
    // TODO: implement event blocking via script event filters or patterns
#endif
}

void enable_free_purchase(bool enabled) {
    if (!g_initialized.load()) return;
    OutputDebugStringA(enabled ? "[YimMenuCustom] free purchase ON (stub)\n" : "[YimMenuCustom] free purchase OFF (stub)\n");
#ifdef ENABLE_SCRIPTHOOKV
    // TODO: patch shop globals/locals or hook purchase routines
#endif
}

std::string status() {
    return g_initialized.load() ? "native layer: stub initialized" : "native layer: not initialized";
}

bool poll_next_script_event(uint64_t &out_hash) {
    // Stub: no script events available in this scaffold. Return false.
    (void)out_hash;
    return false;
}

} // namespace native_interface
