#pragma once

#include <functional>
#include <cstdint>

namespace protections {
    using event_fetcher_t = std::function<bool(uint64_t &out_hash)>;

    void init();
    void shutdown();
    void set_block_crash_events(bool enabled);
    void set_force_host(bool enabled);
    void start_event_thread();
    void stop_event_thread();

    // Register a fetcher that returns the next event hash via out parameter.
    // The fetcher should return true if an event was provided, false otherwise.
    // Typical use: the fork's received_event hook can set a lambda that extracts
    // the hash from the event buffer.
    void set_event_fetcher(event_fetcher_t fetcher);

    // Check if an event hash is considered bad (public helper for hook files)
    bool is_bad_event(uint64_t hash);

    // Runtime reload bad-event list from INI file
    void reload_bad_events_from_ini();

    // Blocked events counter (session)
    void increment_blocked_count();
    uint32_t blocked_count();
}
