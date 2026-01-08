#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <cstdio>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <locale>
#include <codecvt>

// Keep console open and allow time to read errors when closing
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrl) {
    if (ctrl == CTRL_CLOSE_EVENT) {
        printf("[!] Exit requested - holding for debug...\n");
        Sleep(5000); // 5 seconds to copy output
    }
    return TRUE;
}

bool EnableSeDebug() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return false;
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

DWORD GetPID(const std::wstring& name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;

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

static bool IsDigits(const std::string &s) {
    for (char c : s) if (!isdigit(static_cast<unsigned char>(c))) return false;
    return !s.empty();
}

static std::wstring Utf8ToWide(const std::string &utf8) {
    if (utf8.empty()) return {};
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    if (size_needed <= 0) return {};
    std::wstring w;
    w.resize(size_needed);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &w[0], size_needed);
    return w;
}

int main(int argc, char* argv[]) {
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    SetConsoleTitleA("3leghorse Launcher");
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    printf("[+] 3leghorse Launcher Debug Mode Enabled\n");

    if (argc < 3) {
        printf("[!] Usage: menu_launcher.exe <full_path_to_dll> <process_name>\n");
        printf("[?] Example: menu_launcher.exe C:\\path\\YimMenuCustom.dll GTA5.exe\n");
        system("pause");
        return 1;
    }


    std::string dllPathUtf8 = argv[1];
    std::string targetArg = argv[2];

    // Resolve DLL path using filesystem (handles relative paths)
    std::error_code ec;
    std::filesystem::path dllPath = std::filesystem::u8path(dllPathUtf8);
    if (!dllPath.is_absolute()) dllPath = std::filesystem::absolute(dllPath, ec);
    if (ec) {
        printf("[!] Failed to resolve DLL path: %s (err %d)\n", dllPathUtf8.c_str(), ec.value());
        system("pause");
        return 1;
    }
    if (!std::filesystem::exists(dllPath)) {
        printf("[!] DLL not found: %s\n", dllPath.u8string().c_str());
        system("pause");
        return 1;
    }

    if (!EnableSeDebug()) {
        printf("[!] EnableSeDebug failed (continuing). LastError: %lu\n", GetLastError());
    }

    DWORD pid = 0;
    std::wstring procNameW;
    if (IsDigits(targetArg)) {
        // Treat as PID
        pid = static_cast<DWORD>(std::stoul(targetArg));
    } else {
        procNameW = Utf8ToWide(targetArg);
        pid = GetPID(procNameW);
    }
    if (!pid) {
        // If targetArg looks like an executable path, try to start it
        std::filesystem::path possibleExe = std::filesystem::u8path(targetArg);
        if (!possibleExe.is_absolute()) possibleExe = std::filesystem::absolute(possibleExe, ec);
        if (!ec && std::filesystem::exists(possibleExe) && possibleExe.has_extension()) {
            // Attempt to create the process
            printf("[ ] Target not running; attempting to start: %s\n", possibleExe.u8string().c_str());
            STARTUPINFOW si{};
            PROCESS_INFORMATION pi{};
            si.cb = sizeof(si);
            std::wstring cmdline = Utf8ToWide(possibleExe.u8string());
            // CreateProcess requires modifiable buffer
            std::vector<wchar_t> cmdBuf(cmdline.begin(), cmdline.end());
            cmdBuf.push_back(0);
            if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
                printf("[+] Launched process, PID=%lu\n", pi.dwProcessId);
                pid = pi.dwProcessId;
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            } else {
                printf("[!] Failed to start target. LastError: %lu\n", GetLastError());
                system("pause");
                return 1;
            }
        } else {
            printf("[!] Target process not found (or PID invalid): %s\n", targetArg.c_str());
            system("pause");
            return 1;
        }
    }
    printf("[+] Target PID: %lu\n", pid);

    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!proc) {
        printf("[!] OpenProcess failed. LastError: %lu\n", GetLastError());
        system("pause");
        return 1;
    }

    // Write DLL path (wide) into target process and call LoadLibraryW
    std::wstring dllPathW = Utf8ToWide(dllPath.u8string());
    SIZE_T bytes = (dllPathW.size() + 1) * sizeof(wchar_t);
    LPVOID remoteMem = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        printf("[!] VirtualAllocEx failed. LastError: %lu\n", GetLastError());
        CloseHandle(proc);
        system("pause");
        return 1;
    }

    if (!WriteProcessMemory(proc, remoteMem, dllPathW.c_str(), bytes, nullptr)) {
        printf("[!] WriteProcessMemory failed. LastError: %lu\n", GetLastError());
        VirtualFreeEx(proc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(proc);
        system("pause");
        return 1;
    }

    printf("[ ] Creating remote thread (LoadLibraryW)...\n");
    HANDLE thread = CreateRemoteThread(proc, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibraryW), remoteMem, 0, nullptr);
    if (!thread) {
        printf("[!] CreateRemoteThread failed. LastError: %lu\n", GetLastError());
        VirtualFreeEx(proc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(proc);
        system("pause");
        return 1;
    }

    WaitForSingleObject(thread, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    printf("[+] Remote thread exit code: 0x%08lX\n", exitCode);

    CloseHandle(thread);
    VirtualFreeEx(proc, remoteMem, 0, MEM_RELEASE);
    CloseHandle(proc);

    printf("[!] Injection complete — press any key to exit.\n");
    system("pause");
    return 0;
}
