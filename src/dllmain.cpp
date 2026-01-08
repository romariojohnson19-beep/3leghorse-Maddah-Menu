#include <windows.h>
#include "menu_stub.hpp"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        
        // Test basic Windows functionality - create a test window
        CreateThread(NULL, 0, [](LPVOID) -> DWORD {
            MessageBoxA(NULL, "3leghorse DLL Loaded Successfully!\n\nTesting window creation...", "DLL Test", MB_OK | MB_ICONINFORMATION);
            
            // Create a simple test window to verify we can create windows
            WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
            wc.lpfnWndProc = DefWindowProc;
            wc.hInstance = GetModuleHandle(NULL);
            wc.lpszClassName = "3leghorse_Test";
            
            if (RegisterClassEx(&wc)) {
                HWND testWnd = CreateWindowEx(0, "3leghorse_Test", "3leghorse Test Window", 
                    WS_OVERLAPPEDWINDOW, 100, 100, 300, 200, NULL, NULL, wc.hInstance, NULL);
                if (testWnd) {
                    ShowWindow(testWnd, SW_SHOW);
                    UpdateWindow(testWnd);
                    MessageBoxA(testWnd, "Test window created successfully!\n\nNow testing menu initialization...", "Window Test", MB_OK);
                    DestroyWindow(testWnd);
                } else {
                    MessageBoxA(NULL, "Failed to create test window!", "Window Test Failed", MB_OK | MB_ICONERROR);
                }
                UnregisterClass("3leghorse_Test", wc.hInstance);
            }
            
            return 0;
        }, NULL, 0, NULL);
        
        // Create a simple test thread that draws text on screen
        CreateThread(NULL, 0, [](LPVOID) -> DWORD {
            Sleep(2000); // Wait 2 seconds for game to be ready
            
            // Try to draw text using GDI
            HWND hwnd = FindWindowA(NULL, "Grand Theft Auto V");
            if (!hwnd) {
                hwnd = GetForegroundWindow();
            }
            
            if (hwnd) {
                HDC hdc = GetDC(hwnd);
                if (hdc) {
                    SetTextColor(hdc, RGB(255, 0, 0));
                    SetBkMode(hdc, TRANSPARENT);
                    HFONT hFont = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                    
                    TextOutA(hdc, 50, 50, "3LEGHORSE MENU LOADED!", 22);
                    TextOutA(hdc, 50, 80, "If you see this, GDI works!", 26);
                    
                    SelectObject(hdc, hOldFont);
                    DeleteObject(hFont);
                    ReleaseDC(hwnd, hdc);
                    
                    Sleep(5000); // Show text for 5 seconds
                }
            }
            return 0;
        }, NULL, 0, NULL);
        
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
