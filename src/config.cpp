#include "config.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace cfg {

std::string config_path() {
    auto p = fs::current_path() / "3leghorse_settings.ini";
    return p.string();
}

bool load(Settings& out) {
    std::ifstream in(config_path());
    if (!in.good()) return false;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key)) continue;
        if (key == "theme_r") iss >> out.theme.r;
        else if (key == "theme_g") iss >> out.theme.g;
        else if (key == "theme_b") iss >> out.theme.b;
        else if (key == "theme_a") iss >> out.theme.a;
        else if (key == "hotkey") iss >> out.toggle_hotkey;
        else if (key == "reset_blocked_on_session") iss >> out.reset_blocked_on_session;
    }
    return true;
}

bool save(const Settings& s) {
    std::ofstream out(config_path(), std::ios::trunc);
    if (!out.good()) return false;
    out << "theme_r " << s.theme.r << "\n";
    out << "theme_g " << s.theme.g << "\n";
    out << "theme_b " << s.theme.b << "\n";
    out << "theme_a " << s.theme.a << "\n";
    out << "hotkey " << s.toggle_hotkey << "\n";
    out << "reset_blocked_on_session " << (s.reset_blocked_on_session ? 1 : 0) << "\n";
    return true;
}

} // namespace cfg

namespace cfg {

const Settings& get() {
    static Settings s;
    static bool loaded = false;
    if (!loaded) {
        load(s);
        loaded = true;
    }
    return s;
}

Settings& get_mutable() {
    static Settings s;
    static bool loaded = false;
    if (!loaded) {
        load(s);
        loaded = true;
    }
    return s;
}

} // namespace cfg
