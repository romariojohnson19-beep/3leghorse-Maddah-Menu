#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

// Helper: Convert UTF-8 std::string to wide string
static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    std::wstring wide(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), wide.data(), size);
    return wide;
}

bool EnableSeDebug() {
    HANDLE token;
    TOKEN_PRIVILEGES tp;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token))
        return false;
    LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(token, FALSE, &tp, 0, nullptr, nullptr);
    CloseHandle(token);
    return true;
}

DWORD GetPID(const std::wstring& name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry = { sizeof(entry) };
    if (Process32FirstW(snap, &entry)) {
        do {
            if (name == entry.szExeFile) {
                CloseHandle(snap);
                return entry.th32ProcessID;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return 0;
}

// Very basic manual map (only for demonstration - not production ready)
bool ManualMap(HANDLE proc, const std::vector<BYTE>& data) {
    std::cout << "[!] Manual map not implemented in this version.\n";
    std::cout << "[!] Using fallback LoadLibrary injection.\n";
    return false;
}

static bool ReadDLL(const std::string& path, std::vector<BYTE>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    std::streamsize n = f.tellg();
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(n));
    if (n > 0) f.read(reinterpret_cast<char*>(out.data()), n);
    return true;
}

int main(int argc, char* argv[]) {
    SetConsoleTitleA("3leghorse Launcher");

    if (argc < 2) {
        std::cout << "Usage: launcher.exe <dll_path> [process_name or exe_path]\n";
        std::cout << "Example: launcher.exe 3leghorse.dll GTA5.exe\n";
        std::cout << "Press Enter to exit...\n";
        std::cin.get();
        return 1;
    }

    std::string dllPath = argv[1];

    std::string targetArg = (argc >= 3) ? argv[2] : "GTA5.exe";

    EnableSeDebug();

    std::cout << "[+] Looking for target: " << targetArg << "\n";

    DWORD pid = 0;
    fs::path targetPath = targetArg;

    if (fs::exists(targetPath) && fs::is_regular_file(targetPath)) {
        // Try to start executable
        std::cout << "[+] Starting executable: " << targetArg << "\n";
        ShellExecuteA(nullptr, "open", targetArg.c_str(), nullptr, nullptr, SW_SHOW);
        std::this_thread::sleep_for(std::chrono::seconds(8)); // wait a bit
    }

    // Try to find running process
    std::wstring wTarget = Utf8ToWide(targetArg);
    pid = GetPID(wTarget);
    if (pid == 0 && targetArg.find(".exe") != std::string::npos) {
        // Try stripping .exe for name
        std::wstring wName = Utf8ToWide(targetArg.substr(0, targetArg.find_last_of('.')));
        pid = GetPID(wName);
    }

    if (pid == 0) {
        std::cout << "[!] Could not find running process '" << targetArg << "'\n";
        std::cout << "Press Enter to exit...\n";
        std::cin.get();
        return 1;
    }

    std::cout << "[+] Found target PID: " << pid << "\n";

    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc) {
        std::cout << "[!] OpenProcess failed. Error: " << GetLastError() << "\n";
        std::cout << "Press Enter to exit...\n";
        std::cin.get();
        return 1;
    }

    // Try LoadLibrary first
    size_t pathSize = dllPath.size() + 1;
    LPVOID alloc = VirtualAllocEx(proc, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!alloc) {
        std::cout << "[!] VirtualAllocEx failed. Error: " << GetLastError() << "\n";
        CloseHandle(proc);
        std::cout << "Press Enter to exit...\n";
        std::cin.get();
        return 1;
    }

    if (!WriteProcessMemory(proc, alloc, dllPath.c_str(), pathSize, nullptr)) {
        std::cout << "[!] WriteProcessMemory failed. Error: " << GetLastError() << "\n";
        VirtualFreeEx(proc, alloc, 0, MEM_RELEASE);
        CloseHandle(proc);
        std::cout << "Press Enter to exit...\n";
        std::cin.get();
        return 1;
    }

    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, alloc, 0, nullptr);
    if (thread) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        std::cout << "[+] Injected successfully via LoadLibrary!\n";
    } else {
        std::cout << "[!] CreateRemoteThread failed. Error: " << GetLastError() << "\n";
        std::cout << "[!] Trying manual map fallback...\n";
        std::vector<BYTE> dllData;
        if (!ReadDLL(dllPath, dllData)) {
            std::cout << "[!] Could not read DLL file\n";
        } else if (ManualMap(proc, dllData)) {
            std::cout << "[+] Manual map success!\n";
        } else {
            std::cout << "[!] Manual map also failed\n";
        }
    }

    VirtualFreeEx(proc, alloc, 0, MEM_RELEASE);
    CloseHandle(proc);

    std::cout << "\nPress Enter to exit...\n";
    std::cin.get();
    return 0;
}
