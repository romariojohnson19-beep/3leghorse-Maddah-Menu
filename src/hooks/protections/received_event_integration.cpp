// Template integration for received_event hooks in Yim-derived forks.
// Drop this file (or copy its logic) into your fork's received_event implementation.
// Two styles shown:
//  - Style 1 (recommended): Inline blocking + ack in the hook.
//  - Style 2 (fetcher): Register a lambda that extracts hashes for the protections thread.

#include "protections.hpp"
#include "native_interface.hpp"
#include <string>

// NOTE: This file is a template and will not compile as-is in this scaffold because
// it references fork-specific types like rage::datBitBuffer and g_pointers. Copy
// the relevant snippets into your fork's received_event file and adapt names.

// ---------- Style 1: Direct in-hook blocking (recommended) ----------
/*
void hooks::received_event(rage::netEventMgr* event_manager, CNetGamePlayer* source, CNetGamePlayer* target, uint16_t event_id, int index, int handled_bitset, int size, rage::datBitBuffer* buffer) {
    // Example: extract the hash from the buffer or event args. Common patterns:
    // uint64_t event_hash = event->m_args[0];
    // or: buffer->Seek(0); uint64_t event_hash = buffer->Read<uint64_t>();

    uint64_t event_hash = 0; // <-- replace with real extraction

    if (protections::is_bad_event(event_hash)) {
        // Ack to prevent queue buildup / desync. Replace with your fork's ack function.
        // Example: g_pointers->m_send_event_ack(event_manager, source, target, index, handled_bitset);
        return; // consume the event: do NOT call original
    }

    // Otherwise call original handler
    return hooks::received_event_orig(event_manager, source, target, event_id, index, handled_bitset, size, buffer);
}
*/

// ---------- Style 2: Fetcher registration (optional) ----------
/*
// Call this once during your hook initialization (where you set hooks):
protections::set_event_fetcher([](uint64_t &out_hash)->bool {
    // if there's an event pending, extract its hash and return true
    // Example using buffer:
    if (!g_current_buffer) return false;
    g_current_buffer->Seek(0);
    out_hash = g_current_buffer->Read<uint64_t>();
    return true;
});
*/

// How to use:
// 1) For reliability and simplicity add the inline check (Style 1) into your
//    existing received_event hook and use the fork's ACK function to consume blocked events.
// 2) If you prefer centralized logic, register a fetcher during hook init (Style 2) and let
//    protections' thread perform matching.
