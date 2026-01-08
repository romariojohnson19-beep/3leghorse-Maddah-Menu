DLL Injector

A tiny, single-file DLL injector using CreateRemoteThread + LoadLibraryW.

Build (MSYS2 MinGW64):

    cd tools/injector
    cmake -S . -B build -G "Ninja"
    cmake --build build --config Release

Usage:

    dll_injector <pid|process_name> <full_path_to_dll>

Examples:

    dll_injector 1234 C:\\temp\\YimMenuCustom.dll
    dll_injector GTA5.exe C:\\temp\\YimMenuCustom.dll

Notes:
- Run as administrator when injecting into protected processes.
- This is a minimal example for development/testing; use responsibly.
