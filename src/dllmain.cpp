#include <windows.h>
#include "menu_stub.hpp"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        
        // Test basic Windows functionality
        MessageBoxA(NULL, "3leghorse DLL Loaded Successfully!\n\nIf you see this, DLL injection worked.\n\nNow check GTA V for the red menu text.", "DLL Test", MB_OK | MB_ICONINFORMATION);
        
        OutputDebugStringA("[3leghorse] DLL_PROCESS_ATTACH - Starting initialization\n");
        menu_stub::init();
        OutputDebugStringA("[3leghorse] DLL_PROCESS_ATTACH - Initialization complete\n");
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringA("[3leghorse] DLL_PROCESS_DETACH - Shutting down\n");
        menu_stub::shutdown();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}
