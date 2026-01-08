3leghorse Hook Integration

This document shows two common integration options for Legacy Yim forks where you want
3leghorse's protections to block known bad script events.

Files: Drop the snippets into your fork's `received_event` hook implementation (typical
locations: `src/hooks/protections/received_event.cpp` or `src/hooks/script/received_event.cpp`).

Style 1 (Recommended): Inline blocking + ack
-----------------------------------------

1) Include protections header at top of file:

    #include "protections.hpp"

2) Extract the event hash in your hook. Common patterns:

    // If event is an object with args array
    uint64_t event_hash = event->m_args[0];

    // Or, if using rage::datBitBuffer
    buffer->Seek(0);
    uint64_t event_hash = buffer->Read<uint64_t>();

3) Check and ack/consume blocked events:

    if (protections::is_bad_event(event_hash)) {
        // Use your fork's ack call. Example placeholder:
        // g_pointers->m_send_event_ack(event_manager, source, target, index, handled_bitset);
        return; // consume - do not call original
    }

4) Otherwise call the original handler.

Style 2 (Optional): Fetcher registration
---------------------------------------

If you prefer centralized logic, register a fetcher during initialization:

    protections::set_event_fetcher([](uint64_t &out_hash)->bool {
        if (!current_buffer) return false;
        current_buffer->Seek(0);
        out_hash = current_buffer->Read<uint64_t>();
        return true;
    });

This method allows the protections background thread to perform matching. The direct
inline check (Style 1) is preferred for reliability and lower latency.

Runtime bad-hash list
---------------------

Place a file `3leghorse_protections.ini` next to your injected DLL with entries like:

    ; lines starting with ; or # are comments
    hash=0x5F59E4C8
    hash=0xBCEC9F7C
    0xAD6D9C95

You can use the in-menu "Reload Bad Events" button (Theme window) to reload the list at runtime.

Acknowledgements
----------------

This integration template was generated for 3leghorse Menu and aims to be minimal and safe.
Always test in invite-only sessions first.
