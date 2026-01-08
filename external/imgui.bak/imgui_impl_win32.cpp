// imgui_impl_win32.cpp - trimmed Win32 backend
#include "imgui.h"
#include "imgui_impl_win32.h"
#include <windows.h>

static HWND g_hWnd = NULL;

bool ImGui_ImplWin32_Init(void* hwnd)
{
	g_hWnd = (HWND)hwnd;
	return true;
}

void ImGui_ImplWin32_Shutdown()
{
	g_hWnd = NULL;
}

void ImGui_ImplWin32_NewFrame()
{
	// Backend could update IO (mouse/keyboard); we keep simple.
}
