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

// Minimal x64 manual map (no TLS callbacks). Assumes same-arch target.
bool ManualMap64(HANDLE proc, const std::vector<BYTE>& image, LPVOID& remoteBaseOut) {
    remoteBaseOut = nullptr;
    if (image.size() < sizeof(IMAGE_DOS_HEADER)) {
        std::cout << "[!] ManualMap: image too small\n";
        return false;
    }

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        std::cout << "[!] ManualMap: bad DOS signature\n";
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        std::cout << "[!] ManualMap: unsupported PE (expecting x64)\n";
        return false;
    }

    SIZE_T imageSize   = nt->OptionalHeader.SizeOfImage;
    SIZE_T headersSize = nt->OptionalHeader.SizeOfHeaders;

    LPVOID remoteBase = VirtualAllocEx(proc, reinterpret_cast<LPVOID>(nt->OptionalHeader.ImageBase), imageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!remoteBase) {
        remoteBase = VirtualAllocEx(proc, nullptr, imageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    }
    if (!remoteBase) {
        std::cout << "[!] ManualMap: VirtualAllocEx failed. Error: " << GetLastError() << "\n";
        return false;
    }

    if (!WriteProcessMemory(proc, remoteBase, image.data(), headersSize, nullptr)) {
        std::cout << "[!] ManualMap: failed to write headers. Error: " << GetLastError() << "\n";
        VirtualFreeEx(proc, remoteBase, 0, MEM_RELEASE);
        return false;
    }

    auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        if (section->SizeOfRawData == 0) continue;
        LPCVOID localAddr = image.data() + section->PointerToRawData;
        LPVOID remoteAddr = static_cast<BYTE*>(remoteBase) + section->VirtualAddress;
        SIZE_T size = section->SizeOfRawData;
        if (!WriteProcessMemory(proc, remoteAddr, localAddr, size, nullptr)) {
            std::cout << "[!] ManualMap: failed to write section " << section->Name << " Error: " << GetLastError() << "\n";
            VirtualFreeEx(proc, remoteBase, 0, MEM_RELEASE);
            return false;
        }
    }

    ULONGLONG delta = reinterpret_cast<ULONGLONG>(remoteBase) - nt->OptionalHeader.ImageBase;

    if (delta != 0) {
        const auto& relocDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (relocDir.Size) {
            SIZE_T offset = 0;
            while (offset < relocDir.Size) {
                auto* block = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(image.data() + relocDir.VirtualAddress + offset);
                if (block->SizeOfBlock == 0) break;
                size_t entryCount = (block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                auto* entry = reinterpret_cast<const WORD*>(reinterpret_cast<const BYTE*>(block) + sizeof(IMAGE_BASE_RELOCATION));
                for (size_t i = 0; i < entryCount; ++i) {
                    WORD typeOffset = entry[i];
                    WORD type = typeOffset >> 12;
                    WORD off  = typeOffset & 0x0FFF;
                    if (type == IMAGE_REL_BASED_DIR64) {
                        ULONGLONG* localPtr = reinterpret_cast<ULONGLONG*>(const_cast<BYTE*>(image.data()) + block->VirtualAddress + off);
                        ULONGLONG patched = *localPtr + delta;
                        LPVOID remotePtr = static_cast<BYTE*>(remoteBase) + block->VirtualAddress + off;
                        WriteProcessMemory(proc, remotePtr, &patched, sizeof(patched), nullptr);
                    }
                }
                offset += block->SizeOfBlock;
            }
        }
    }

    // Import resolution
    const auto& importDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir.Size) {
        auto* desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(image.data() + importDir.VirtualAddress);
        while (desc->Name) {
            const char* modName = reinterpret_cast<const char*>(image.data() + desc->Name);
            HMODULE mod = LoadLibraryA(modName);
            if (!mod) {
                std::cout << "[!] ManualMap: failed to load import module " << modName << "\n";
                VirtualFreeEx(proc, remoteBase, 0, MEM_RELEASE);
                return false;
            }

            auto* thunk = reinterpret_cast<const IMAGE_THUNK_DATA64*>(image.data() + desc->OriginalFirstThunk);
            auto* firstThunk = reinterpret_cast<ULONGLONG*>(const_cast<BYTE*>(image.data()) + desc->FirstThunk);
            SIZE_T idx = 0;
            while (thunk && thunk->u1.AddressOfData) {
                FARPROC fn = nullptr;
                if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) {
                    fn = GetProcAddress(mod, reinterpret_cast<LPCSTR>(thunk->u1.Ordinal & 0xFFFF));
                } else {
                    auto* importByName = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(image.data() + thunk->u1.AddressOfData);
                    fn = GetProcAddress(mod, importByName->Name);
                }
                if (!fn) {
                    std::cout << "[!] ManualMap: unresolved import in module " << modName << "\n";
                    VirtualFreeEx(proc, remoteBase, 0, MEM_RELEASE);
                    return false;
                }
                ULONGLONG remoteThunkVal = reinterpret_cast<ULONGLONG>(fn);
                LPVOID remoteThunkPtr = static_cast<BYTE*>(remoteBase) + desc->FirstThunk + idx * sizeof(ULONGLONG);
                WriteProcessMemory(proc, remoteThunkPtr, &remoteThunkVal, sizeof(remoteThunkVal), nullptr);
                ++idx;
                ++thunk;
            }
            ++desc;
        }
    }

    // Set final protections (simple heuristic)
    section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        DWORD protect = PAGE_READONLY;
        bool executable = (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        bool readable   = (section->Characteristics & IMAGE_SCN_MEM_READ) != 0;
        bool writable   = (section->Characteristics & IMAGE_SCN_MEM_WRITE) != 0;
        if (executable) {
            protect = writable ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
        } else {
            protect = writable ? PAGE_READWRITE : PAGE_READONLY;
        }
        DWORD oldProt;
        VirtualProtectEx(proc, static_cast<BYTE*>(remoteBase) + section->VirtualAddress, section->Misc.VirtualSize, protect, &oldProt);
    }

    LPTHREAD_START_ROUTINE entry = reinterpret_cast<LPTHREAD_START_ROUTINE>(static_cast<BYTE*>(remoteBase) + nt->OptionalHeader.AddressOfEntryPoint);
    HANDLE hThread = CreateRemoteThread(proc, nullptr, 0, entry, remoteBase, 0, nullptr);
    if (!hThread) {
        std::cout << "[!] ManualMap: CreateRemoteThread failed. Error: " << GetLastError() << "\n";
        VirtualFreeEx(proc, remoteBase, 0, MEM_RELEASE);
        return false;
    }
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    remoteBaseOut = remoteBase;
    return true;
}

// Get alternative process names for known executables
std::vector<std::string> GetAlternativeProcessNames(const std::string& exeName) {
    std::vector<std::string> alternatives;
    
    // GTA V: PlayGTAV.exe -> GTA5.exe
    if (exeName == "PlayGTAV.exe" || exeName == "PlayGTAV") {
        alternatives.push_back("GTA5.exe");
        alternatives.push_back("GTA5");
    }
    
    // Add more mappings here as needed
    // Example: if (exeName == "SomeGame.exe") alternatives.push_back("SomeGameProcess.exe");
    
    return alternatives;
}

// Function to run a command and wait
bool RunCommand(const std::string& cmd, const std::string& cwd = "") {
    std::string params = "-c " + cmd;
    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = "open";
    sei.lpFile = "C:\\msys64\\usr\\bin\\bash.exe";
    sei.lpParameters = params.c_str();
    sei.lpDirectory = cwd.empty() ? nullptr : cwd.c_str();
    sei.nShow = SW_HIDE;

    if (!ShellExecuteExA(&sei)) {
        return false;
    }

    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(sei.hProcess, &exitCode);
        CloseHandle(sei.hProcess);
        return exitCode == 0;
    }
    return false;
}

// Function to build the DLL
bool BuildDLL() {
    std::cout << "[+] DLL not found, attempting to build...\n";

    // Assume launcher is in tools/launcher/build, so root is ..\..\..
    fs::path launcherDir = fs::current_path();
    fs::path rootDir = launcherDir.parent_path().parent_path().parent_path();

    fs::path buildDir = rootDir / "build";
    fs::path cmakeLists = rootDir / "CMakeLists.txt";

    if (!fs::exists(cmakeLists)) {
        std::cout << "[!] CMakeLists.txt not found in " << rootDir << "\n";
        return false;
    }

    // Create build dir if needed
    if (!fs::exists(buildDir)) {
        fs::create_directories(buildDir);
    }

    // Run cmake configure
    std::string cmakeCmd = "cmake -B \"" + buildDir.string() + "\" -S \"" + rootDir.string() + "\"";
    std::cout << "[+] Running: " << cmakeCmd << "\n";
    if (!RunCommand(cmakeCmd)) {
        std::cout << "[!] CMake configure failed\n";
        return false;
    }

    // Run cmake build
    std::string buildCmd = "cmake --build \"" + buildDir.string() + "\" --config Release";
    std::cout << "[+] Running: " << buildCmd << "\n";
    if (!RunCommand(buildCmd)) {
        std::cout << "[!] CMake build failed\n";
        return false;
    }

    std::cout << "[+] Build completed successfully\n";
    return true;
}

int main(int argc, char* argv[]) {
    SetConsoleTitleA("3leghorse Launcher");

    // If no DLL provided, try sensible defaults (current dir, then repo /build)
    std::string dllPathUtf8;
    if (argc < 2) {
        std::vector<fs::path> candidates = {
            fs::current_path() / "3leghorse.dll",
            fs::current_path() / "YimMenuCustom.dll",
            fs::current_path().parent_path().parent_path() / "build" / "3leghorse.dll"
        };
        for (const auto& c : candidates) {
            if (fs::exists(c)) {
                dllPathUtf8 = c.string();
                std::cout << "[+] No DLL arg given, using found DLL: " << dllPathUtf8 << "\n";
                break;
            }
        }
        if (dllPathUtf8.empty()) {
            // Try to build the DLL
            if (BuildDLL()) {
                // Check candidates again after build
                for (const auto& c : candidates) {
                    if (fs::exists(c)) {
                        dllPathUtf8 = c.string();
                        std::cout << "[+] Using built DLL: " << dllPathUtf8 << "\n";
                        break;
                    }
                }
            }
            if (dllPathUtf8.empty()) {
                std::cout << "Usage: launcher.exe <dll_path> [process_name_or_exe]\n";
                std::cout << "Example: launcher.exe 3leghorse.dll PlayGTAV.exe\n";
                std::cout << "Press Enter to exit...\n";
                std::cin.get();
                return 1;
            }
        }
    } else {
        dllPathUtf8 = argv[1];
    }

    fs::path dllPath = fs::path(dllPathUtf8);  // Handles UTF-8 correctly without u8path

    std::cout << "[+] Resolving DLL path: " << dllPath << "\n";

    if (!fs::exists(dllPath)) {
        std::cout << "[!] DLL not found: " << dllPath << "\n";
        // Try to build the DLL
        if (BuildDLL()) {
            if (fs::exists(dllPath)) {
                std::cout << "[+] Using built DLL: " << dllPath << "\n";
            } else {
                std::cout << "[!] DLL still not found after build\n";
                std::cout << "Press Enter to exit...\n";
                std::cin.get();
                return 1;
            }
        } else {
            std::cout << "[!] Build failed\n";
            std::cout << "Press Enter to exit...\n";
            std::cin.get();
            return 1;
        }
    }

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
    std::string processName = targetArg;

    // Extract filename if full path provided
    if (fs::exists(targetPath) && targetPath.extension() == ".exe") {
        processName = targetPath.filename().string();
    }

    // First, try to find if the process is already running
    std::vector<std::string> processNamesToTry = { processName };
    
    // Add alternative process names
    auto alternatives = GetAlternativeProcessNames(processName);
    processNamesToTry.insert(processNamesToTry.end(), alternatives.begin(), alternatives.end());
    
    // Also try without .exe extension for all names
    std::vector<std::string> namesWithoutExt;
    for (const auto& name : processNamesToTry) {
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".exe") {
            namesWithoutExt.push_back(name.substr(0, name.size() - 4));
        }
    }
    processNamesToTry.insert(processNamesToTry.end(), namesWithoutExt.begin(), namesWithoutExt.end());
    
    // Remove duplicates
    std::sort(processNamesToTry.begin(), processNamesToTry.end());
    processNamesToTry.erase(std::unique(processNamesToTry.begin(), processNamesToTry.end()), processNamesToTry.end());
    
    for (const auto& name : processNamesToTry) {
        std::cout << "[+] Trying process name: " << name << "\n";
        std::wstring wTarget = Utf8ToWide(name);
        pid = GetPID(wTarget);
        if (pid != 0) {
            std::cout << "[+] Found process: " << name << " (PID: " << pid << ")\n";
            processName = name; // Update processName to the found name
            break;
        }
    }

    if (pid == 0) {
        std::cout << "[!] Could not find running process: " << processName << "\n";

        // If it's a full exe path, try launching it
        if (fs::exists(targetPath) && targetPath.extension() == ".exe") {
            std::cout << "[+] Starting executable: " << targetArg << "\n";
            ShellExecuteA(nullptr, "open", targetArg.c_str(), nullptr, nullptr, SW_SHOW);
            std::this_thread::sleep_for(std::chrono::seconds(10)); // Wait for load
            
            std::cout << "[+] Searching again after launch...\n";
            
            // Try all process names again after launching
            for (const auto& name : processNamesToTry) {
                std::cout << "[+] Trying process name: " << name << "\n";
                std::wstring wTarget = Utf8ToWide(name);
                pid = GetPID(wTarget);
                if (pid != 0) {
                    std::cout << "[+] Found process after launch: " << name << " (PID: " << pid << ")\n";
                    processName = name; // Update processName to the found name
                    break;
                }
            }
        }

        if (pid == 0) {
            std::cout << "[!] Still could not find process after launch attempt\n";
            std::cout << "Press Enter to exit...\n";
            std::cin.get();
            return 1;
        }
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

    bool injected = false;
    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, alloc, 0, nullptr);
    if (thread) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        std::cout << "[+] Injected successfully via LoadLibrary!\n";
        injected = true;
    } else {
        std::cout << "[!] LoadLibrary injection failed. Error: " << GetLastError() << "\n";
    }

    VirtualFreeEx(proc, alloc, 0, MEM_RELEASE);

    if (!injected) {
        std::vector<BYTE> image;
        if (!ReadDLL(dllPathStr, image)) {
            std::cout << "[!] Manual map: failed to read DLL file\n";
        } else {
            LPVOID remoteBase = nullptr;
            std::cout << "[+] Attempting manual map fallback...\n";
            if (ManualMap64(proc, image, remoteBase)) {
                injected = true;
                std::cout << "[+] Manual map succeeded at base " << remoteBase << "\n";
            } else {
                std::cout << "[!] Manual map failed.\n";
            }
        }
    }

    CloseHandle(proc);

    if (injected) {
        std::cout << "\n[+] Injection complete. Press Enter to exit...\n";
    } else {
        std::cout << "\n[!] Injection failed. Press Enter to exit...\n";
    }
    std::cin.get();
    return injected ? 0 : 1;
}
