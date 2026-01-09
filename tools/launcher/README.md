# Menu Launcher (Standalone)

A minimal standalone launcher to inject the menu DLL into a target process using `LoadLibraryW`, with an automatic manual-map fallback if `LoadLibrary` fails (no dummy stubs).

## Build (MSYS2 MinGW64)

```pwsh
cd tools/launcher
cmake -S . -B build -G "Ninja"
cmake --build build --config Release
```

## Usage

```pwsh
# With explicit args
menu_launcher.exe <full_path_to_dll> <process_name>

# Or just run (auto-picks YimMenuCustom.dll from cwd or ../build) targeting PlayGTAV.exe by default
menu_launcher.exe
```

Examples:
```pwsh
menu_launcher.exe C:\path\YimMenuCustom.dll GTA5.exe
menu_launcher.exe             # auto-find DLL, default target PlayGTAV.exe
```

Tips:
- Run as Administrator when targeting protected processes.
- The console stays open and logs detailed errors (GetLastError) for troubleshooting.
- If your DLL path contains spaces, quote it when launching.
- If `LoadLibrary` injection fails, the launcher will attempt a manual map (x64 only) automatically and report the remote base.
