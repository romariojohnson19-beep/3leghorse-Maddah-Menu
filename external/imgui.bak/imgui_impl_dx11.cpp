// imgui_impl_dx11.cpp - trimmed DX11 backend
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <stdint.h>

static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;

bool ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context)
{
	g_pd3dDevice = device;
	g_pd3dDeviceContext = device_context;
	return true;
}

void ImGui_ImplDX11_Shutdown()
{
	g_pd3dDevice = NULL;
	g_pd3dDeviceContext = NULL;
}

void ImGui_ImplDX11_NewFrame()
{
	// Normally backend prepares textures, buffers, etc.
}

void ImGui_ImplDX11_RenderDrawData(void* draw_data)
{
	// In a real backend we translate ImDrawData into D3D11 draw calls.
	// For our vendored, minimal backend, we leave it as a no-op so the
	// ImGui API is functional while keeping the implementation compact.
	(void)draw_data;
}
