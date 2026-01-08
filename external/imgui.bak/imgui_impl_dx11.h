#pragma once
#include <d3d11.h>

bool ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context);
void ImGui_ImplDX11_Shutdown();
void ImGui_ImplDX11_NewFrame();
void ImGui_ImplDX11_RenderDrawData(void* draw_data);
// Canonical backend helpers for device object lifecycle. Use these to create
// font textures and other GPU resources after init, and to invalidate them
// before device reset/shutdown.
bool ImGui_ImplDX11_CreateDeviceObjects();
void ImGui_ImplDX11_InvalidateDeviceObjects();
