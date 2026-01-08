// Minimal ImGui public header (vendor shim)
#pragma once

#include <cstdint>
#include <cstddef>

// Define types in global namespace for compatibility with existing code
struct ImVec2 { float x; float y; ImVec2() : x(0), y(0) {} ImVec2(float _x, float _y) : x(_x), y(_y) {} };
struct ImVec4 { float x,y,z,w; ImVec4() : x(0),y(0),z(0),w(0) {} ImVec4(float a,float b,float c,float d) : x(a),y(b),z(c),w(d) {} };

enum ImGuiCol { ImGuiCol_Text = 0, ImGuiCol_WindowBg = 1, ImGuiCol_Button = 2 };

struct ImGuiStyle { ImVec4 Colors[32]; };

struct ImGuiIO { ImVec2 DisplaySize; };

using ImGuiWindowFlags = int;

struct ImDrawData { /* minimal placeholder */ };

// Also provide ImGui namespace with the expected API
namespace ImGui {
	inline void CreateContext() {}
	inline void DestroyContext() {}
	inline ImGuiStyle& GetStyle() { static ImGuiStyle s; return s; }
	inline ImGuiIO& GetIO() { static ImGuiIO io; io.DisplaySize = ImVec2(800,600); return io; }
	inline double GetTime() { return 0.0; }
	inline void NewFrame() {}
	inline void Render() {}
	inline ImDrawData* GetDrawData() { return nullptr; }
	inline void SetNextWindowBgAlpha(float) {}
	inline void Begin(const char*, void*, ImGuiWindowFlags) {}
	inline void End() {}
	inline void SetCursorPos(const ImVec2&) {}
	inline void Text(const char*, ...) {}
	inline void TextColored(const ImVec4&, const char*) {}
	inline void Separator() {}
	inline bool ColorEdit4(const char*, float[4]) { return false; }
	inline bool InputInt(const char*, int*) { return false; }
	inline bool Button(const char*) { return false; }
	inline void PushStyleColor(ImGuiCol, const ImVec4&) {}
	inline void PopStyleColor() {}
}

// Common flags used in renderer
#define ImGuiWindowFlags_NoTitleBar (1<<0)
#define ImGuiWindowFlags_NoInputs (1<<1)
#define ImGuiWindowFlags_AlwaysAutoResize (1<<2)
#define ImGuiWindowFlags_NoDecoration (1<<3)
#define ImGuiWindowFlags_NoBackground (1<<4)

