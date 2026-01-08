#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <thread>
#include <shellapi.h>

namespace fs = std::filesystem;

// Convert UTF-8 std::string to wide string (std::wstring)
static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), size);
    return wide;
}

bool EnableSeDebug() {
    HANDLE token;
    TOKEN_PRIVILEGES tp;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
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

bool ReadDLL(const std::string& path, std::vector<BYTE>& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.seekg(0, std::ios::end);
    out.resize(file.tellg());
    file.seekg(0);
    file.read(reinterpret_cast<char*>(out.data()), out.size());
    return true;
}

int main(int argc, char* argv[]) {
    SetConsoleTitleA("3leghorse Launcher");

    if (argc < 2) {
        std::cout << "Usage: launcher.exe <dll_path> [process_name_or_exe]\n";
        std::cout << "Example: launcher.exe 3leghorse.dll PlayGTAV.exe\n";
        std::cout << "Press Enter to exit...\n";
        std::cin.get();
        return 1;
    }

    std::string dllPathUtf8 = argv[1];
    fs::path dllPath = fs::path(dllPathUtf8);  // Handles UTF-8 correctly without u8path

    std::string targetArg = (argc >= 3) ? argv[2] : "PlayGTAV.exe";

    EnableSeDebug();

    std::cout << "[+] Resolving DLL path: " << dllPath << "\n";

    if (!fs::exists(dllPath)) {
        std::cout << "[!] DLL not found: " << dllPath << "\n";
        std::cout << "Press Enter to exit...\n";
        std::cin.get();
        return 1;
    }

    std::cout << "[+] Looking for target: " << targetArg << "\n";

    DWORD pid = 0;
    fs::path targetPath = targetArg;

    // If target is a full exe path and it exists, launch it
    if (fs::exists(targetPath) && targetPath.extension() == ".exe") {
        std::cout << "[+] Starting executable: " << targetArg << "\n";
        ShellExecuteA(nullptr, "open", targetArg.c_str(), nullptr, nullptr, SW_SHOW);
        std::this_thread::sleep_for(std::chrono::seconds(10)); // Wait for load
    }

    // Find PID
    std::wstring wTarget = Utf8ToWide(targetArg);
    pid = GetPID(wTarget);

    if (pid == 0) {
        // Try without .exe extension
        std::string nameNoExt = targetArg;
        if (nameNoExt.size() >= 4 && nameNoExt.substr(nameNoExt.size() - 4) == ".exe")
            nameNoExt.erase(nameNoExt.size() - 4);
        pid = GetPID(Utf8ToWide(nameNoExt));
    }

    if (pid == 0) {
        std::cout << "[!] Could not find running process: " << targetArg << "\n";
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

    // LoadLibrary injection attempt
    std::string dllPathStr = dllPath.string(); // Normal UTF-8 string
    size_t pathSize = dllPathStr.size() + 1;
    LPVOID alloc = VirtualAllocEx(proc, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!alloc) {
        std::cout << "[!] VirtualAllocEx failed. Error: " << GetLastError() << "\n";
        CloseHandle(proc);
        std::cin.get();
        return 1;
    }

    if (!WriteProcessMemory(proc, alloc, dllPathStr.c_str(), pathSize, nullptr)) {
        std::cout << "[!] WriteProcessMemory failed. Error: " << GetLastError() << "\n";
        VirtualFreeEx(proc, alloc, 0, MEM_RELEASE);
        CloseHandle(proc);
        std::cin.get();
        return 1;
    }

    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, alloc, 0, nullptr);
    if (thread) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        std::cout << "[+] Injected successfully via LoadLibrary!\n";
    } else {
        std::cout << "[!] LoadLibrary injection failed. Error: " << GetLastError() << "\n";
        std::cout << "[!] Manual map fallback not implemented yet.\n";
    }

    VirtualFreeEx(proc, alloc, 0, MEM_RELEASE);
    CloseHandle(proc);

    std::cout << "\n[+] Injection complete. Press Enter to exit...\n";
    std::cin.get();
    return 0;
}
