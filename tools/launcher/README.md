# Menu Launcher (Standalone)

A minimal standalone launcher to inject the menu DLL into a target process using `LoadLibraryW`.

## Build (MSYS2 MinGW64)

```pwsh
cd tools/launcher
cmake -S . -B build -G "Ninja"
cmake --build build --config Release
```

## Usage

```pwsh
menu_launcher.exe <full_path_to_dll> <process_name>
```

Example:
```pwsh
menu_launcher.exe C:\path\YimMenuCustom.dll GTA5.exe
```

Tips:
- Run as Administrator when targeting protected processes.
- The console stays open and logs detailed errors (GetLastError) for troubleshooting.
- If your DLL path contains spaces, quote it when launching.
