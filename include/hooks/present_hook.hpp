#pragma once

namespace hooks {
// Initialize the DX11 Present hook (MinHook). Returns true on success.
bool init_present_hook();
// Disable the Present hook if it was installed.
void shutdown_present_hook();
}
