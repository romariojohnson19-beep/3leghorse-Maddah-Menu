# YimMenu Custom (Legacy Fork Scaffold)

This workspace scaffolds a GTA V YimMenu Legacy-style project for VS Code on Windows using **CMake + Ninja + MinGW/Clang**. It includes:
- Minimal C++ DLL target (`YimMenuCustom`) with a standalone stub tick loop and a native interface shim (currently stubbed; safe no-op).
- Lua folder for drop-in scripts (stealth/free-purchase examples).
- VS Code settings preconfigured for CMake Tools with clang/clang++.

> To become a real in-game menu, replace/extend the stub with the actual YimMenu Legacy sources and integrate ImGui, ScriptHookV, MinHook, and native bindings. The `native_interface` layer is where to wire real natives/hooks.

## Prerequisites (Windows)
- MSYS2 with `mingw-w64-clang-x86_64` toolchain, CMake, and Ninja available on `PATH` (e.g., `C:\msys64\mingw64\bin`).
- VS Code extensions: C/C++, CMake Tools, Clangd.

## Configure & Build
```pwsh
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --config Release
```
The output DLL will be at `build/YimMenuCustom.dll`.

### Standalone stub (what it does now)
- Spins a background tick thread on init and logs to OutputDebugString.
- Routes placeholder features to `native_interface` stubs: God Mode, Infinite Ammo, Stealth Protections, Free Purchase (no GTA effect yet without SDKs).
- Safe to inject in a non-GTA process or Story Mode; does nothing game-facing until hooks/natives are wired.

### Using ScriptHookV + MinHook (optional)
- Set CMake cache variables when configuring:
	- `-DENABLE_SCRIPTHOOKV=ON`
	- `-DSHV_DIR="C:/path/to/ScriptHookV_SDK"` (must contain `inc/natives.h`, `inc/script.h`)
	- `-DMINHOOK_DIR="C:/path/to/MinHook"` (must contain `include/MinHook.h` and `lib/MinHook.x64.lib`)
- Reconfigure and rebuild; toggles will call the real natives (e.g., `SET_PLAYER_INVINCIBLE`, `SET_PED_INFINITE_AMMO_CLIP`).
- Protections/free-purchase are still TODO placeholders—you need to add event filters and shop global/locals patches.

## Next Steps
1. Drop the real YimMenu Legacy sources into `src/` and `include/` and wire deps (ImGui, MinHook, ScriptHookV).
2. Add Lua scripts under `lua/` and reload in-game.
3. Inject the built DLL into GTA V (use `-nobattleye` and FSL `version.dll`).
4. Replace the stub feature loop with real natives/hooks when integrating the YimMenu fork.
