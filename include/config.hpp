#pragma once

#include <string>

namespace cfg {

struct Theme {
    float r = 0.92f;
    float g = 0.74f;
    float b = 0.0f;
    float a = 1.0f;
};

struct Settings {
    Theme theme;
    int toggle_hotkey = 0x76; // F7 default
    bool reset_blocked_on_session = true;
};

// Load settings from disk (returns true if loaded)
bool load(Settings& out);

// Save settings to disk
bool save(const Settings& s);

// Path used for settings file
std::string config_path();

// Access singleton settings (loads on first call)
const Settings& get();
Settings& get_mutable();

} // namespace cfg
