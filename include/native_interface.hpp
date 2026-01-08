#pragma once

#include <string>
#include <cstdint>

namespace native_interface {

// Initialize native/hook layer. Returns false if required deps are missing.
bool init();

// Shutdown native/hook layer.
void shutdown();

// Gameplay toggles (no-ops until real natives are wired).
void set_player_invincible(bool enabled);
void set_infinite_ammo(bool enabled);
void apply_stealth_protections(bool enabled);
void enable_free_purchase(bool enabled);

// Utility to report current status of the native layer.
std::string status();

// Poll next script/network event; returns true and sets hash when an event is available.
bool poll_next_script_event(uint64_t &out_hash);

} // namespace native_interface
