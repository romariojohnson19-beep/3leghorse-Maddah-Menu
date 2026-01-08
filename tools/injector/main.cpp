#include <windows.h>
#include <string>

// Keep console open and verbose for debugging the injector
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrl) {
    if (ctrl == CTRL_CLOSE_EVENT) {
        printf("Exit requested - holding for debug...\n");
        Sleep(5000); // give time to copy output
    }
    return TRUE;
}

#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdio>
#include <exception>

// Enable SeDebugPrivilege for better handle access
bool EnableSeDebug() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return false;
    }
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    if (!LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &tp.Privileges[0].Luid)) {
        CloseHandle(token);
        return false;
    }
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(token, FALSE, &tp, 0, nullptr, nullptr);
    bool ok = (GetLastError() == ERROR_SUCCESS);
    CloseHandle(token);
    return ok;
}

// Get PID by process name (wide)
DWORD GetPID(const std::wstring& name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD pid = 0;
    if (Process32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(name.c_str(), entry.szExeFile) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return pid;
}

// Minimal x64 manual mapper
bool ManualMap(HANDLE proc, const std::vector<BYTE>& dllData) {
    if (dllData.size() < sizeof(IMAGE_DOS_HEADER)) return false;
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(dllData.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(dllData.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    SIZE_T imageSize = nt->OptionalHeader.SizeOfImage;
    LPVOID remoteBase = VirtualAllocEx(proc, nullptr, imageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteBase) return false;

    // Copy headers
    if (!WriteProcessMemory(proc, remoteBase, dllData.data(), nt->OptionalHeader.SizeOfHeaders, nullptr)) {
        VirtualFreeEx(proc, remoteBase, 0, MEM_RELEASE);
        return false;
    }

    // Copy sections
    auto* section = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        LPCVOID src = dllData.data() + section[i].PointerToRawData;
        LPVOID dst = static_cast<BYTE*>(remoteBase) + section[i].VirtualAddress;
        SIZE_T size = section[i].SizeOfRawData;
        if (size && !WriteProcessMemory(proc, dst, src, size, nullptr)) {
            VirtualFreeEx(proc, remoteBase, 0, MEM_RELEASE);
            return false;
        }
    }

    // Relocations
    ULONGLONG delta = reinterpret_cast<ULONGLONG>(remoteBase) - nt->OptionalHeader.ImageBase;
    if (delta && nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
        auto* reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(static_cast<BYTE*>(remoteBase) +
            nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
        const auto relocEnd = reinterpret_cast<BYTE*>(reloc) + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
        while (reloc->VirtualAddress && reinterpret_cast<BYTE*>(reloc) < relocEnd) {
            auto* entry = reinterpret_cast<WORD*>(reloc + 1);
            int count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            for (int j = 0; j < count; ++j) {
                WORD type = entry[j] >> 12;
                WORD offset = entry[j] & 0x0FFF;
                if (type == IMAGE_REL_BASED_DIR64) {
                    BYTE* patchAddr = static_cast<BYTE*>(remoteBase) + reloc->VirtualAddress + offset;
                    ULONGLONG patched = 0;
                    if (ReadProcessMemory(proc, patchAddr, &patched, sizeof(patched), nullptr)) {
                        patched += delta;
                        WriteProcessMemory(proc, patchAddr, &patched, sizeof(patched), nullptr);
                    }
                }
            }
            reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(reloc) + reloc->SizeOfBlock);
        }
    }

    // Imports
    if (nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size) {
        auto* importDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(static_cast<BYTE*>(remoteBase) +
            nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        while (importDesc->Name) {
            auto* modName = reinterpret_cast<LPCSTR>(static_cast<BYTE*>(remoteBase) + importDesc->Name);
            HMODULE mod = LoadLibraryA(modName); // local load for addresses (assumes same base for system DLLs)
            if (!mod) return false;

            auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(static_cast<BYTE*>(remoteBase) + importDesc->OriginalFirstThunk);
            auto* iat   = reinterpret_cast<IMAGE_THUNK_DATA64*>(static_cast<BYTE*>(remoteBase) + importDesc->FirstThunk);
            while (thunk->u1.AddressOfData) {
                FARPROC func = nullptr;
                if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) {
                    func = GetProcAddress(mod, reinterpret_cast<LPCSTR>(thunk->u1.Ordinal & 0xFFFF));
                } else {
                    auto* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(static_cast<BYTE*>(remoteBase) + thunk->u1.AddressOfData);
                    func = GetProcAddress(mod, importByName->Name);
                }
                if (!func) return false;
                WriteProcessMemory(proc, &iat->u1.Function, &func, sizeof(func), nullptr);
                ++thunk; ++iat;
            }
            ++importDesc;
        }
    }

    // Call DllMain remotely (DLL_PROCESS_ATTACH)
    LPTHREAD_START_ROUTINE entryPoint = reinterpret_cast<LPTHREAD_START_ROUTINE>(static_cast<BYTE*>(remoteBase) + nt->OptionalHeader.AddressOfEntryPoint);
    HANDLE th = CreateRemoteThread(proc, nullptr, 0, entryPoint, remoteBase, 0, nullptr);
    if (!th) {
        VirtualFreeEx(proc, remoteBase, 0, MEM_RELEASE);
        return false;
    }
    WaitForSingleObject(th, INFINITE);
    CloseHandle(th);

    // Erase headers for stealth
    std::vector<BYTE> zeros(nt->OptionalHeader.SizeOfHeaders, 0);
    WriteProcessMemory(proc, remoteBase, zeros.data(), zeros.size(), nullptr);

    return true;
}

int main(int argc, char* argv[]) {
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    SetConsoleTitleA("3leghorse Injector Debug");
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    printf("[+] 3leghorse Debug Mode Enabled\n");

    if (argc < 3) {
        printf("[!] Usage: launcher.exe <dll_path> <process_name>\n");
        printf("[?] Example: launcher.exe C:\\path\\3leghorse.dll GTA5.exe\n");
        system("pause");
        return 1;
    }

    std::string dllPath = argv[1];
    std::string procNameUtf8 = argv[2];
    std::wstring procName(procNameUtf8.begin(), procNameUtf8.end());

    try {
        if (!EnableSeDebug()) {
            printf("[!] EnableSeDebug failed (continuing). LastError: %lu\n", GetLastError());
        }

        std::ifstream file(dllPath, std::ios::binary);
        if (!file) {
            printf("[!] Failed to open DLL file: %s\n", dllPath.c_str());
            system("pause");
            return 1;
        }
        std::vector<BYTE> dllData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (dllData.empty()) {
            printf("[!] DLL file empty: %s\n", dllPath.c_str());
            system("pause");
            return 1;
        }

        DWORD pid = GetPID(procName);
        if (!pid) {
            printf("[!] Process '%s' not found.\n", procNameUtf8.c_str());
            system("pause");
            return 1;
        }

        HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!proc) {
            printf("[!] OpenProcess failed. LastError: %lu\n", GetLastError());
            system("pause");
            return 1;
        }
        printf("[+] Target PID: %lu\n", pid);

        // Try LoadLibrary first
        SIZE_T pathSize = dllPath.size() + 1;
        LPVOID alloc = VirtualAllocEx(proc, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (alloc && WriteProcessMemory(proc, alloc, dllPath.c_str(), pathSize, nullptr)) {
            printf("[ ] Attempting LoadLibrary path...\n");
            HANDLE thread = CreateRemoteThread(proc, nullptr, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, alloc, 0, nullptr);
            if (thread) {
                WaitForSingleObject(thread, INFINITE);
                CloseHandle(thread);
                printf("[+] Injected via LoadLibrary\n");
                VirtualFreeEx(proc, alloc, 0, MEM_RELEASE);
                CloseHandle(proc);
                printf("[!] Injection complete — press any key to exit.\n");
                system("pause");
                return 0;
            }
            printf("[!] LoadLibrary thread creation failed. LastError: %lu\n", GetLastError());
        }
        printf("[ ] LoadLibrary path failed, trying manual map...\n");

        if (alloc) VirtualFreeEx(proc, alloc, 0, MEM_RELEASE);

        if (ManualMap(proc, dllData)) {
            printf("[+] Manual map success!\n");
        } else {
            printf("[!] Manual map failed\n");
            CloseHandle(proc);
            system("pause");
            return 1;
        }

        CloseHandle(proc);
    } catch (const std::exception& e) {
        printf("[!] Exception: %s\n", e.what());
    } catch (...) {
        printf("[!] Unknown error during injection.\n");
    }

    printf("[!] Injection complete — press any key to exit.\n");
    system("pause");
    return 0;
}

// Some MinGW builds may default to a wide GUI entrypoint (wWinMain). Provide
// a shim that forwards to the existing narrow main so linkage succeeds
// regardless of the chosen subsystem/entry convention.
#if defined(_WIN32)
extern "C" int main(int argc, char** argv);

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    return main(__argc, __argv);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    return wWinMain(hInst, hPrev, nullptr, nCmdShow);
}
#endif
