#pragma once
#include <functional>

namespace menu_stub {

    struct Feature {
        const char* name;
        bool enabled;
    };

    // Initialize the standalone menu stub (spins a lightweight tick thread).
    void init();

    // Shutdown and join background work.
    void shutdown();

    // Enumerate features for UI; the callback is invoked under a mutex.
    void for_each_feature(const std::function<void(Feature&)>& fn);

    // Overlay visibility controls (shared with renderer UI & hotkey).
    bool overlay_visible();
    void set_overlay_visible(bool visible);
}
