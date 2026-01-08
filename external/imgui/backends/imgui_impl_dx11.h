#pragma once// Dear ImGui: Renderer Backend for DirectX11

// Use with a Platform Backend (e.g. Win32)

#include "imgui.h"

#pragma once

IMGUI_IMPL_API bool     ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context);#include "imgui.h"

IMGUI_IMPL_API void     ImGui_ImplDX11_Shutdown();

IMGUI_IMPL_API void     ImGui_ImplDX11_NewFrame();struct ID3D11Device;

IMGUI_IMPL_API void     ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data);struct ID3D11DeviceContext;



// Use if you want to reset your rendering state without losing ImGui state.IMGUI_IMPL_API bool ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context);

IMGUI_IMPL_API void     ImGui_ImplDX11_InvalidateDeviceObjects();IMGUI_IMPL_API void ImGui_ImplDX11_Shutdown();

IMGUI_IMPL_API bool     ImGui_ImplDX11_CreateDeviceObjects();IMGUI_IMPL_API void ImGui_ImplDX11_NewFrame();
IMGUI_IMPL_API void ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data);

// Optional helpers for device reset scenarios
IMGUI_IMPL_API bool ImGui_ImplDX11_CreateDeviceObjects();
IMGUI_IMPL_API void ImGui_ImplDX11_DestroyDeviceObjects();
