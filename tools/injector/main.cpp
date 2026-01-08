#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include <cwctype>
#include <cstdint>
#include <vector>

static DWORD FindProcessId(const std::wstring& processName) {
    PROCESSENTRY32W processInfo;
    processInfo.dwSize = sizeof(processInfo);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    Process32FirstW(snapshot, &processInfo);
    if (!processName.empty()) {
        do {
            if (!processName.compare(processInfo.szExeFile)) {
                CloseHandle(snapshot);
                return processInfo.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &processInfo));
    }
    CloseHandle(snapshot);
    return 0;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3) {
        std::wcout << L"Usage: dll_injector <pid|name> <full_path_to_dll>\n";
        std::wcout << L"Examples: dll_injector 1234 C:\\temp\\YimMenuCustom.dll\n";
        std::wcout << L"          dll_injector GTA5.exe C:\\temp\\YimMenuCustom.dll\n";
        return 1;
    }

    DWORD pid = 0;
    std::wstring target = argv[1];
    bool byName = false;
    // if numeric, treat as PID
    if (iswdigit(target[0])) {
        pid = std::stoul(target);
    } else {
        byName = true;
    }

    std::wstring dllPath = argv[2];

    if (byName) {
        pid = FindProcessId(target);
        if (pid == 0) {
            std::wcerr << L"Process not found: " << target << L"\n";
            return 2;
        }
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    std::wcerr << L"[injector] OpenProcess returned handle=" << (void*)hProcess << L"\n";
    if (!hProcess) {
        DWORD err = GetLastError();
        std::wcerr << L"[injector] OpenProcess failed, GetLastError=" << err << L"\n";
        return 3;
    }

    size_t size = (dllPath.size() + 1) * sizeof(wchar_t);
    LPVOID remoteMem = VirtualAllocEx(hProcess, NULL, size, MEM_COMMIT, PAGE_READWRITE);
    std::wcerr << L"[injector] VirtualAllocEx returned addr=" << remoteMem << L" size=" << size << L"\n";
    if (!remoteMem) {
        DWORD err = GetLastError();
        std::wcerr << L"[injector] VirtualAllocEx failed, GetLastError=" << err << L"\n";
        CloseHandle(hProcess);
        return 4;
    }

    BOOL wpm = WriteProcessMemory(hProcess, remoteMem, dllPath.c_str(), size, NULL);
    std::wcerr << L"[injector] WriteProcessMemory returned " << (wpm ? L"TRUE" : L"FALSE") << L"\n";
    if (!wpm) {
        DWORD err = GetLastError();
        std::wcerr << L"[injector] WriteProcessMemory failed, GetLastError=" << err << L"\n";
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 5;
    }

    // Resolve LoadLibraryW locally and compute corresponding address in remote process
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    std::wcerr << L"[injector] local kernel32 base = " << (void*)hKernel32 << L"\n";
    FARPROC loadLibLocal = GetProcAddress(hKernel32, "LoadLibraryW");
    std::wcerr << L"[injector] local LoadLibraryW = " << (void*)loadLibLocal << L"\n";
    if (!loadLibLocal) {
        DWORD err = GetLastError();
        std::wcerr << L"[injector] GetProcAddress(LoadLibraryW) failed, GetLastError=" << err << L"\n";
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 6;
    }

    // Find kernel32 base in remote process
    uintptr_t remoteKernel32Base = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me;
        me.dwSize = sizeof(me);
        if (Module32FirstW(snap, &me)) {
            do {
                // Compare module name case-insensitively
                if (_wcsicmp(me.szModule, L"kernel32.dll") == 0) {
                    remoteKernel32Base = (uintptr_t)me.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
    }
    std::wcerr << L"[injector] remote kernel32 base = " << (void*)remoteKernel32Base << L"\n";

    uintptr_t localKernel32Base = (uintptr_t)hKernel32;
    uintptr_t offset = (uintptr_t)loadLibLocal - localKernel32Base;
    uintptr_t remoteLoadLib = remoteKernel32Base ? (remoteKernel32Base + offset) : 0;
    std::wcerr << L"[injector] computed remote LoadLibraryW = " << (void*)remoteLoadLib << L" (offset=0x" << std::hex << offset << std::dec << L")\n";

    // Shellcode-based LoadLibraryW stub (to avoid CFG/parameter issues)
    auto inject_via_shellcode = [&](uintptr_t loadlib_addr) -> bool {
        if (!loadlib_addr) return false;
        const size_t code_size = 10 + 10 + 3 + 2 + 3 + 1; // 29 bytes
        size_t path_bytes = (dllPath.size() + 1) * sizeof(wchar_t);
        size_t total_size = code_size + path_bytes;
        LPVOID remote_code = VirtualAllocEx(hProcess, NULL, total_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!remote_code) {
            std::wcerr << L"[injector] shellcode alloc failed, GLE=" << GetLastError() << L"\n";
            return false;
        }
        uintptr_t remote_base = (uintptr_t)remote_code;
        uintptr_t remote_path_addr = remote_base + code_size;

        std::vector<uint8_t> code;
        code.reserve(code_size);
        auto emit64 = [&](uint64_t v) {
            for (int i = 0; i < 8; ++i) code.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
        };

        // mov rcx, remote_path_addr
        code.push_back(0x48); code.push_back(0xB9); emit64(remote_path_addr);
        // mov rax, loadlib_addr
        code.push_back(0x48); code.push_back(0xB8); emit64(loadlib_addr);
        // sub rsp, 0x28
        code.push_back(0x48); code.push_back(0x83); code.push_back(0xEC); code.push_back(0x28);
        // call rax
        code.push_back(0xFF); code.push_back(0xD0);
        // add rsp, 0x28
        code.push_back(0x48); code.push_back(0x83); code.push_back(0xC4); code.push_back(0x28);
        // ret
        code.push_back(0xC3);

        BOOL w1 = WriteProcessMemory(hProcess, remote_code, code.data(), code.size(), NULL);
        BOOL w2 = WriteProcessMemory(hProcess, (LPVOID)remote_path_addr, dllPath.c_str(), path_bytes, NULL);
        std::wcerr << L"[injector] shellcode write code=" << (w1 ? L"ok" : L"fail") << L" path=" << (w2 ? L"ok" : L"fail") << L"\n";
        if (!w1 || !w2) {
            std::wcerr << L"[injector] shellcode write failed, GLE=" << GetLastError() << L"\n";
            VirtualFreeEx(hProcess, remote_code, 0, MEM_RELEASE);
            return false;
        }

        HANDLE ht = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)remote_base, NULL, 0, NULL);
        std::wcerr << L"[injector] shellcode CRT returned=" << (void*)ht << L" GLE=" << GetLastError() << L"\n";
        if (!ht) {
            VirtualFreeEx(hProcess, remote_code, 0, MEM_RELEASE);
            return false;
        }
        WaitForSingleObject(ht, INFINITE);
        DWORD ec = 0; GetExitCodeThread(ht, &ec);
        std::wcerr << L"[injector] shellcode thread exit code=" << ec << L"\n";
        CloseHandle(ht);
        VirtualFreeEx(hProcess, remote_code, 0, MEM_RELEASE);
        return ec != 0;
    };

    // Try CreateRemoteThread with the remote function pointer
    HANDLE hThread = NULL;
    if (remoteLoadLib) {
        hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)remoteLoadLib, remoteMem, 0, NULL);
        std::wcerr << L"[injector] CreateRemoteThread (remote addr) returned hThread=" << (void*)hThread << L"\n";
        if (!hThread) {
            DWORD err = GetLastError();
            std::wcerr << L"[injector] CreateRemoteThread (remote) failed, GetLastError=" << err << L"\n";
        }
    } else {
        std::wcerr << L"[injector] remoteLoadLib not computed; skipping CreateRemoteThread(remote)\n";
    }

    // If CreateRemoteThread failed (or we couldn't compute remote addr), try NtCreateThreadEx as a fallback
    if (!hThread) {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) ntdll = LoadLibraryW(L"ntdll.dll");
        if (ntdll) {
            typedef LONG (NTAPI *PFN_NtCreateThreadEx)(PHANDLE, ACCESS_MASK, LPVOID, HANDLE, LPTHREAD_START_ROUTINE, LPVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, LPVOID);
            PFN_NtCreateThreadEx NtCreateThreadEx = (PFN_NtCreateThreadEx)GetProcAddress(ntdll, "NtCreateThreadEx");
            if (NtCreateThreadEx) {
                HANDLE hRemoteThread = NULL;
                LONG status = NtCreateThreadEx(&hRemoteThread, 0x1FFFFF, NULL, hProcess, (LPTHREAD_START_ROUTINE)remoteLoadLib, remoteMem, FALSE, 0, 0, 0, NULL);
                std::wcerr << L"[injector] NtCreateThreadEx status=0x" << std::hex << status << std::dec << L" hThread=" << (void*)hRemoteThread << L"\n";
                if (status >= 0 && hRemoteThread) {
                    hThread = hRemoteThread;
                }
            } else {
                std::wcerr << L"[injector] NtCreateThreadEx not available\n";
            }
        } else {
            std::wcerr << L"[injector] failed to load ntdll.dll\n";
        }
    }

    // If thread creation is still blocked, try shellcode
    if (!hThread) {
        std::wcerr << L"[injector] trying shellcode-based LoadLibraryW stub\n";
        bool ok = inject_via_shellcode(remoteLoadLib);
        if (ok) {
            CloseHandle(hProcess);
            return 0;
        }
    }

    if (!hThread) {
        std::wcerr << L"[injector] All remote thread creation attempts failed\n";
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 7;
    }

    WaitForSingleObject(hThread, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);

    std::wcout << L"Injection thread exit code: " << exitCode << L"\n";

    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return 0;
}

// Some toolchains (MinGW) may try to link against WinMain; provide a thin
// WinMain wrapper that converts the command-line to wide argv and forwards
// to wmain so the program works regardless of the subsystem/linker expectations.
#if defined(_WIN32) && !defined(__MINGW32__)
// For non-MinGW toolchains, nothing special is required — wmain is fine.
#endif

#if defined(_WIN32)
extern "C" int wmain(int argc, wchar_t* argv[]);

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc = 0;
    wchar_t** argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvw) {
        return wmain(0, nullptr);
    }
    int ret = wmain(argc, argvw);
    LocalFree(argvw);
    return ret;
}
#endif
