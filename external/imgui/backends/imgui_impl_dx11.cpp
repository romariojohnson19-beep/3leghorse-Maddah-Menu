// Dear ImGui backend implementation for DirectX11// dear imgui: Renderer Backend for DirectX11

// Latest canonical from ocornut/imgui (v1.90+ stable)// This needs to be used along with a Platform Backend (e.g. Win32)



#include "imgui.h"// Implemented features:

#include "imgui_impl_dx11.h"//  [X] Renderer: User texture binding. Use 'ID3D11ShaderResourceView*' as texture identifier. Read the FAQ about ImTextureID/ImTextureRef!

#include <d3d11.h>//  [X] Renderer: Large meshes support (64k+ vertices) even with 16-bit indices (ImGuiBackendFlags_RendererHasVtxOffset).

#include <stdio.h>//  [X] Renderer: Texture updates support for dynamic font atlas (ImGuiBackendFlags_RendererHasTextures).

//  [X] Renderer: Expose selected render state for draw callbacks to use. Access in '(ImGui_ImplXXXX_RenderState*)GetPlatformIO().Renderer_RenderState'.

#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.

// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.

#pragma comment(lib, "d3d11.lib")// Learn about Dear ImGui:

// - FAQ                  https://dearimgui.com/faq

// Data// - Getting Started      https://dearimgui.com/getting-started

static ID3D11Device*            g_pd3dDevice = nullptr;// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).

static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;// - Introduction, links and more at the top of imgui.cpp

static ID3D11SamplerState*      g_pFontSampler = nullptr;

static ID3D11ShaderResourceView* g_pFontTextureView = nullptr;// CHANGELOG

static ID3D11RasterizerState*   g_pRasterizerState = nullptr;// (minor and older changes stripped away, please see git history for details)

static ID3D11BlendState*        g_pBlendState = nullptr;//  2025-09-18: Call platform_io.ClearRendererHandlers() on shutdown.

static ID3D11DepthStencilState* g_pDepthStencilState = nullptr;//  2025-06-11: DirectX11: Added support for ImGuiBackendFlags_RendererHasTextures, for dynamic font atlas.

static int                      g_VertexBufferSize = 5000, g_IndexBufferSize = 10000;//  2025-05-07: DirectX11: Honor draw_data->FramebufferScale to allow for custom backends and experiment using it (consistently with other renderer backends, even though in normal condition it is not set under Windows).

//  2025-01-06: DirectX11: Expose VertexConstantBuffer in ImGui_ImplDX11_RenderState. Reset projection matrix in ImDrawCallback_ResetRenderState handler.

struct VertexConstantBuffer//  2024-10-07: DirectX11: Changed default texture sampler to Clamp instead of Repeat/Wrap.

{//  2024-10-07: DirectX11: Expose selected render state in ImGui_ImplDX11_RenderState, which you can access in 'void* platform_io.Renderer_RenderState' during draw callbacks.

    float   mvp[4][4];//  2022-10-11: Using 'nullptr' instead of 'NULL' as per our switch to C++11.

};//  2021-06-29: Reorganized backend to pull data from a single structure to facilitate usage with multiple-contexts (all g_XXXX access changed to bd->XXXX).

//  2021-05-19: DirectX11: Replaced direct access to ImDrawCmd::TextureId with a call to ImDrawCmd::GetTexID(). (will become a requirement)

static void ImGui_ImplDX11_SetupRenderState(ImDrawData* draw_data)//  2021-02-18: DirectX11: Change blending equation to preserve alpha in output buffer.

{//  2019-08-01: DirectX11: Fixed code querying the Geometry Shader state (would generally error with Debug layer enabled).

    // Setup viewport//  2019-07-21: DirectX11: Backup, clear and restore Geometry Shader is any is bound when calling ImGui_ImplDX11_RenderDrawData. Clearing Hull/Domain/Compute shaders without backup/restore.

    D3D11_VIEWPORT vp;//  2019-05-29: DirectX11: Added support for large mesh (64K+ vertices), enable ImGuiBackendFlags_RendererHasVtxOffset flag.

    memset(&vp, 0, sizeof(D3D11_VIEWPORT));//  2019-04-30: DirectX11: Added support for special ImDrawCallback_ResetRenderState callback to reset render state.

    vp.Width = draw_data->DisplaySize.x;//  2018-12-03: Misc: Added #pragma comment statement to automatically link with d3dcompiler.lib when using D3DCompile().

    vp.Height = draw_data->DisplaySize.y;//  2018-11-30: Misc: Setting up io.BackendRendererName so it can be displayed in the About Window.

    vp.MinDepth = 0.0f;//  2018-08-01: DirectX11: Querying for IDXGIFactory instead of IDXGIFactory1 to increase compatibility.

    vp.MaxDepth = 1.0f;//  2018-07-13: DirectX11: Fixed unreleased resources in Init and Shutdown functions.

    vp.TopLeftX = vp.TopLeftY = 0;//  2018-06-08: Misc: Extracted imgui_impl_dx11.cpp/.h away from the old combined DX11+Win32 example.

    g_pd3dDeviceContext->RSSetViewports(1, &vp);//  2018-06-08: DirectX11: Use draw_data->DisplayPos and draw_data->DisplaySize to setup projection matrix and clipping rectangle.

//  2018-02-16: Misc: Obsoleted the io.RenderDrawListsFn callback and exposed ImGui_ImplDX11_RenderDrawData() in the .h file so you can call it yourself.

    // Setup shader and vertex buffers//  2018-02-06: Misc: Removed call to ImGui::Shutdown() which is not available from 1.60 WIP, user needs to call CreateContext/DestroyContext themselves.

    unsigned int stride = sizeof(ImDrawVert);//  2016-05-07: DirectX11: Disabling depth-write.

    unsigned int offset = 0;

    g_pd3dDeviceContext->IASetInputLayout(g_pInputLayout);#include "imgui.h"

    g_pd3dDeviceContext->IASetVertexBuffers(0, 1, &g_pVB, &stride, &offset);#ifndef IMGUI_DISABLE

    g_pd3dDeviceContext->IASetIndexBuffer(g_pIB, sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);#include "imgui_impl_dx11.h"

    g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_pd3dDeviceContext->VSSetShader(g_pVertexShader, nullptr, 0);// DirectX

    g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, &g_pVertexConstantBuffer);#include <stdio.h>

    g_pd3dDeviceContext->PSSetShader(g_pPixelShader, nullptr, 0);#include <d3d11.h>

    g_pd3dDeviceContext->PSSetShaderResources(0, 1, &g_pFontTextureView);#include <d3dcompiler.h>

    g_pd3dDeviceContext->PSSetSamplers(0, 1, &g_pFontSampler);#ifdef _MSC_VER

    g_pd3dDeviceContext->GSSetShader(nullptr, nullptr, 0);#pragma comment(lib, "d3dcompiler") // Automatically link with d3dcompiler.lib as we are using D3DCompile() below.

    g_pd3dDeviceContext->OMSetBlendState(g_pBlendState, nullptr, 0xffffffff);#endif

    g_pd3dDeviceContext->OMSetDepthStencilState(g_pDepthStencilState, 0);

    g_pd3dDeviceContext->RSSetState(g_pRasterizerState);// Clang/GCC warnings with -Weverything

}#if defined(__clang__)

#pragma clang diagnostic ignored "-Wold-style-cast"         // warning: use of old-style cast                            // yes, they are more terse.

// Render function#pragma clang diagnostic ignored "-Wsign-conversion"        // warning: implicit conversion changes signedness

void ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data)#endif

{

    // Avoid rendering when minimized// DirectX11 data

    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)struct ImGui_ImplDX11_Texture

        return;{

    ID3D11Texture2D*            pTexture;

    ID3D11DeviceContext* ctx = g_pd3dDeviceContext;    ID3D11ShaderResourceView*   pTextureView;

};

    // Create and grow vertex/index buffers if needed

    if (!g_pVB || g_VertexBufferSize < draw_data->TotalVtxCount)struct ImGui_ImplDX11_Data

    {{

        if (g_pVB) { g_pVB->Release(); g_pVB = nullptr; }    ID3D11Device*               pd3dDevice;

        g_VertexBufferSize = draw_data->TotalVtxCount + 5000;    ID3D11DeviceContext*        pd3dDeviceContext;

        D3D11_BUFFER_DESC desc;    IDXGIFactory*               pFactory;

        memset(&desc, 0, sizeof(D3D11_BUFFER_DESC));    ID3D11Buffer*               pVB;

        desc.Usage = D3D11_USAGE_DYNAMIC;    ID3D11Buffer*               pIB;

        desc.ByteWidth = g_VertexBufferSize * sizeof(ImDrawVert);    ID3D11VertexShader*         pVertexShader;

        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;    ID3D11InputLayout*          pInputLayout;

        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;    ID3D11Buffer*               pVertexConstantBuffer;

        desc.MiscFlags = 0;    ID3D11PixelShader*          pPixelShader;

        if (g_pd3dDevice->CreateBuffer(&desc, nullptr, &g_pVB) < 0)    ID3D11SamplerState*         pTexSamplerLinear;

            return;    ID3D11RasterizerState*      pRasterizerState;

    }    ID3D11BlendState*           pBlendState;

    if (!g_pIB || g_IndexBufferSize < draw_data->TotalIdxCount)    ID3D11DepthStencilState*    pDepthStencilState;

    {    int                         VertexBufferSize;

        if (g_pIB) { g_pIB->Release(); g_pIB = nullptr; }    int                         IndexBufferSize;

        g_IndexBufferSize = draw_data->TotalIdxCount + 10000;

        D3D11_BUFFER_DESC desc;    ImGui_ImplDX11_Data()       { memset((void*)this, 0, sizeof(*this)); VertexBufferSize = 5000; IndexBufferSize = 10000; }

        memset(&desc, 0, sizeof(D3D11_BUFFER_DESC));};

        desc.Usage = D3D11_USAGE_DYNAMIC;

        desc.ByteWidth = g_IndexBufferSize * sizeof(ImDrawIdx);struct VERTEX_CONSTANT_BUFFER_DX11

        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;{

        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;    float   mvp[4][4];

        if (g_pd3dDevice->CreateBuffer(&desc, nullptr, &g_pIB) < 0)};

            return;

    }struct ImGui_ImplDX11_RenderState

{

    // Upload vertex/index data into a single contiguous GPU buffer    ID3D11Device*               Device;

    D3D11_MAPPED_SUBRESOURCE vtx_resource, idx_resource;    ID3D11DeviceContext*        DeviceContext;

    if (ctx->Map(g_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource) != S_OK)    ID3D11SamplerState*         SamplerDefault;

        return;    ID3D11Buffer*               VertexConstantBuffer;

    if (ctx->Map(g_pIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource) != S_OK)};

        return;

    ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource.pData;// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts

    ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource.pData;// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.

    for (int n = 0; n < draw_data->CmdListsCount; n++)static ImGui_ImplDX11_Data* ImGui_ImplDX11_GetBackendData()

    {{

        const ImDrawList* cmd_list = draw_data->CmdLists[n];    return ImGui::GetCurrentContext() ? (ImGui_ImplDX11_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;

        memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));}

        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));

        vtx_dst += cmd_list->VtxBuffer.Size;// Functions

        idx_dst += cmd_list->IdxBuffer.Size;static void ImGui_ImplDX11_SetupRenderState(const ImDrawData* draw_data, ID3D11DeviceContext* device_ctx)

    }{

    ctx->Unmap(g_pVB, 0);    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();

    ctx->Unmap(g_pIB, 0);

    // Setup viewport

    // Setup orthographic projection matrix    D3D11_VIEWPORT vp = {};

    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos + draw_data->DisplaySize (bottom right)    vp.Width = draw_data->DisplaySize.x * draw_data->FramebufferScale.x;

    {    vp.Height = draw_data->DisplaySize.y * draw_data->FramebufferScale.y;

        float L = draw_data->DisplayPos.x;    vp.MinDepth = 0.0f;

        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;    vp.MaxDepth = 1.0f;

        float T = draw_data->DisplayPos.y;    vp.TopLeftX = vp.TopLeftY = 0;

        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;    device_ctx->RSSetViewports(1, &vp);

        float mvp[4][4] =

        {    // Setup orthographic projection matrix into our constant buffer

            { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.

            { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },    D3D11_MAPPED_SUBRESOURCE mapped_resource;

            { 0.0f,         0.0f,           0.5f,       0.0f },    if (device_ctx->Map(bd->pVertexConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) == S_OK)

            { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },    {

        };        VERTEX_CONSTANT_BUFFER_DX11* constant_buffer = (VERTEX_CONSTANT_BUFFER_DX11*)mapped_resource.pData;

        ctx->UpdateSubresource(g_pVertexConstantBuffer, 0, nullptr, mvp, 0, 0);        float L = draw_data->DisplayPos.x;

    }        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;

        float T = draw_data->DisplayPos.y;

    // Setup render state        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    ImGui_ImplDX11_SetupRenderState(draw_data);        float mvp[4][4] =

        {

    // Render command lists            { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },

    int vtx_offset = 0;            { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },

    int idx_offset = 0;            { 0.0f,         0.0f,           0.5f,       0.0f },

    ImVec2 clip_off = draw_data->DisplayPos;            { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },

    for (int n = 0; n < draw_data->CmdListsCount; n++)        };

    {        memcpy(&constant_buffer->mvp, mvp, sizeof(mvp));

        const ImDrawList* cmd_list = draw_data->CmdLists[n];        device_ctx->Unmap(bd->pVertexConstantBuffer, 0);

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)    }

        {

            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];    // Setup shader and vertex buffers

            if (pcmd->UserCallback)    unsigned int stride = sizeof(ImDrawVert);

            {    unsigned int offset = 0;

                // User callback, registered via ImDrawList::AddCallback()    device_ctx->IASetInputLayout(bd->pInputLayout);

                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to reset render state.)    device_ctx->IASetVertexBuffers(0, 1, &bd->pVB, &stride, &offset);

                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)    device_ctx->IASetIndexBuffer(bd->pIB, sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);

                    ImGui_ImplDX11_SetupRenderState(draw_data);    device_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                else    device_ctx->VSSetShader(bd->pVertexShader, nullptr, 0);

                    pcmd->UserCallback(cmd_list, pcmd);    device_ctx->VSSetConstantBuffers(0, 1, &bd->pVertexConstantBuffer);

            }    device_ctx->PSSetShader(bd->pPixelShader, nullptr, 0);

            else    device_ctx->PSSetSamplers(0, 1, &bd->pTexSamplerLinear);

            {    device_ctx->GSSetShader(nullptr, nullptr, 0);

                // Project scissor/clipping rectangles into framebuffer space    device_ctx->HSSetShader(nullptr, nullptr, 0); // In theory we should backup and restore this as well.. very infrequently used..

                ImVec4 clip_rect;    device_ctx->DSSetShader(nullptr, nullptr, 0); // In theory we should backup and restore this as well.. very infrequently used..

                clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * draw_data->FramebufferScale.x;    device_ctx->CSSetShader(nullptr, nullptr, 0); // In theory we should backup and restore this as well.. very infrequently used..

                clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * draw_data->FramebufferScale.y;

                clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * draw_data->FramebufferScale.x;    // Setup render state

                clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * draw_data->FramebufferScale.y;    const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };

    device_ctx->OMSetBlendState(bd->pBlendState, blend_factor, 0xffffffff);

                if (clip_rect.x < vp_width && clip_rect.y < vp_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)    device_ctx->OMSetDepthStencilState(bd->pDepthStencilState, 0);

                {    device_ctx->RSSetState(bd->pRasterizerState);

                    // Apply scissor/clipping rectangle}

                    const D3D11_RECT r = { (LONG)clip_rect.x, (LONG)clip_rect.y, (LONG)clip_rect.z, (LONG)clip_rect.w };

                    ctx->RSSetScissorRects(1, &r);// Render function

void ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data)

                    // Bind texture{

                    ID3D11ShaderResourceView* texture_srv = (ID3D11ShaderResourceView*)pcmd->GetTexID();    // Avoid rendering when minimized

                    ctx->PSSetShaderResources(0, 1, &texture_srv);    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)

        return;

                    ctx->DrawIndexed(pcmd->ElemCount, idx_offset, vtx_offset);

                }    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();

            }    ID3D11DeviceContext* device = bd->pd3dDeviceContext;

            idx_offset += pcmd->ElemCount;

        }    // Catch up with texture updates. Most of the times, the list will have 1 element with an OK status, aka nothing to do.

        vtx_offset += cmd_list->VtxBuffer.Size;    // (This almost always points to ImGui::GetPlatformIO().Textures[] but is part of ImDrawData to allow overriding or disabling texture updates).

    }    if (draw_data->Textures != nullptr)

}        for (ImTextureData* tex : *draw_data->Textures)

            if (tex->Status != ImTextureStatus_OK)

// Init                ImGui_ImplDX11_UpdateTexture(tex);

bool ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context)

{    // Create and grow vertex/index buffers if needed

    ImGuiIO& io = ImGui::GetIO();    if (!bd->pVB || bd->VertexBufferSize < draw_data->TotalVtxCount)

    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");    {

        if (bd->pVB) { bd->pVB->Release(); bd->pVB = nullptr; }

    // Setup backend data        bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;

    g_pd3dDevice = device;        D3D11_BUFFER_DESC desc = {};

    g_pd3dDeviceContext = device_context;        desc.Usage = D3D11_USAGE_DYNAMIC;

    g_pd3dDevice->AddRef();        desc.ByteWidth = bd->VertexBufferSize * sizeof(ImDrawVert);

    g_pd3dDeviceContext->AddRef();        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ImGui_ImplDX11_CreateDeviceObjects();        desc.MiscFlags = 0;

        if (bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pVB) < 0)

    return true;            return;

}    }

    if (!bd->pIB || bd->IndexBufferSize < draw_data->TotalIdxCount)

// Shutdown    {

void ImGui_ImplDX11_Shutdown()        if (bd->pIB) { bd->pIB->Release(); bd->pIB = nullptr; }

{        bd->IndexBufferSize = draw_data->TotalIdxCount + 10000;

    ImGui_ImplDX11_InvalidateDeviceObjects();        D3D11_BUFFER_DESC desc = {};

    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }        desc.Usage = D3D11_USAGE_DYNAMIC;

    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }        desc.ByteWidth = bd->IndexBufferSize * sizeof(ImDrawIdx);

}        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

// New Frame        if (bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pIB) < 0)

void ImGui_ImplDX11_NewFrame()            return;

{    }

    if (!g_pFontSampler)

        ImGui_ImplDX11_CreateDeviceObjects();    // Upload vertex/index data into a single contiguous GPU buffer

}    D3D11_MAPPED_SUBRESOURCE vtx_resource, idx_resource;

    if (device->Map(bd->pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource) != S_OK)

// Invalidate Device Objects        return;

void ImGui_ImplDX11_InvalidateDeviceObjects()    if (device->Map(bd->pIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource) != S_OK)

{        return;

    if (!g_pd3dDevice) return;    ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource.pData;

    ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource.pData;

    if (g_pFontSampler) { g_pFontSampler->Release(); g_pFontSampler = nullptr; }    for (const ImDrawList* draw_list : draw_data->CmdLists)

    if (g_pFontTextureView) { g_pFontTextureView->Release(); g_pFontTextureView = nullptr; ImGui::GetIO().Fonts->SetTexID(0); } // We copied g_pFontTextureView into io.Fonts->TexID, so let's clear that as well.    {

    if (g_pIB) { g_pIB->Release(); g_pIB = nullptr; }        memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));

    if (g_pVB) { g_pVB->Release(); g_pVB = nullptr; }        memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));

        vtx_dst += draw_list->VtxBuffer.Size;

    if (g_pBlendState) { g_pBlendState->Release(); g_pBlendState = nullptr; }        idx_dst += draw_list->IdxBuffer.Size;

    if (g_pDepthStencilState) { g_pDepthStencilState->Release(); g_pDepthStencilState = nullptr; }    }

    if (g_pRasterizerState) { g_pRasterizerState->Release(); g_pRasterizerState = nullptr; }    device->Unmap(bd->pVB, 0);

    if (g_pPixelShader) { g_pPixelShader->Release(); g_pPixelShader = nullptr; }    device->Unmap(bd->pIB, 0);

    if (g_pVertexConstantBuffer) { g_pVertexConstantBuffer->Release(); g_pVertexConstantBuffer = nullptr; }

    if (g_pInputLayout) { g_pInputLayout->Release(); g_pInputLayout = nullptr; }    // Backup DX state that will be modified to restore it afterwards (unfortunately this is very ugly looking and verbose. Close your eyes!)

    if (g_pVertexShader) { g_pVertexShader->Release(); g_pVertexShader = nullptr; }    struct BACKUP_DX11_STATE

}    {

        UINT                        ScissorRectsCount, ViewportsCount;

// Create Device Objects (fonts, shaders, buffers)        D3D11_RECT                  ScissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];

bool ImGui_ImplDX11_CreateDeviceObjects()        D3D11_VIEWPORT              Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];

{        ID3D11RasterizerState*      RS;

    if (!g_pd3dDevice)        ID3D11BlendState*           BlendState;

        return false;        FLOAT                       BlendFactor[4];

    if (g_pFontSampler)        UINT                        SampleMask;

        ImGui_ImplDX11_InvalidateDeviceObjects();        UINT                        StencilRef;

        ID3D11DepthStencilState*    DepthStencilState;

    // Build shaders (embedded or compile at runtime)        ID3D11ShaderResourceView*   PSShaderResource;

    // Use embedded if you want to avoid D3DCompile dependency        ID3D11SamplerState*         PSSampler;

    // For simplicity, here's runtime compile (requires d3dcompiler.lib linked)        ID3D11PixelShader*          PS;

    const char* vertex_shader =         ID3D11VertexShader*         VS;

        "cbuffer vertexBuffer : register(b0) \        ID3D11GeometryShader*       GS;

        {\        UINT                        PSInstancesCount, VSInstancesCount, GSInstancesCount;

            float4x4 ProjectionMatrix; \        ID3D11ClassInstance         *PSInstances[256], *VSInstances[256], *GSInstances[256];   // 256 is max according to PSSetShader documentation

        };\        D3D11_PRIMITIVE_TOPOLOGY    PrimitiveTopology;

        struct VS_INPUT\        ID3D11Buffer*               IndexBuffer, *VertexBuffer, *VSConstantBuffer;

        {\        UINT                        IndexBufferOffset, VertexBufferStride, VertexBufferOffset;

            float2 pos : POSITION;\        DXGI_FORMAT                 IndexBufferFormat;

            float2 uv  : TEXCOORD0;\        ID3D11InputLayout*          InputLayout;

            byte4 col : COLOR0;\    };

        };\    BACKUP_DX11_STATE old = {};

        struct PS_INPUT\    old.ScissorRectsCount = old.ViewportsCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;

        {\    device->RSGetScissorRects(&old.ScissorRectsCount, old.ScissorRects);

            float4 pos : SV_POSITION;\    device->RSGetViewports(&old.ViewportsCount, old.Viewports);

            float4 col : COLOR0;\    device->RSGetState(&old.RS);

            float2 uv  : TEXCOORD0;\    device->OMGetBlendState(&old.BlendState, old.BlendFactor, &old.SampleMask);

        };\    device->OMGetDepthStencilState(&old.DepthStencilState, &old.StencilRef);

        PS_INPUT main(VS_INPUT input)\    device->PSGetShaderResources(0, 1, &old.PSShaderResource);

        {\    device->PSGetSamplers(0, 1, &old.PSSampler);

            PS_INPUT output;\    old.PSInstancesCount = old.VSInstancesCount = old.GSInstancesCount = 256;

            output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\    device->PSGetShader(&old.PS, old.PSInstances, &old.PSInstancesCount);

            output.col = input.col;\    device->VSGetShader(&old.VS, old.VSInstances, &old.VSInstancesCount);

            output.uv  = input.uv;\    device->VSGetConstantBuffers(0, 1, &old.VSConstantBuffer);

            return output;\    device->GSGetShader(&old.GS, old.GSInstances, &old.GSInstancesCount);

        }";

    device->IAGetPrimitiveTopology(&old.PrimitiveTopology);

    const char* pixel_shader =     device->IAGetIndexBuffer(&old.IndexBuffer, &old.IndexBufferFormat, &old.IndexBufferOffset);

        "struct PS_INPUT\    device->IAGetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset);

        {\    device->IAGetInputLayout(&old.InputLayout);

            float4 pos : SV_POSITION;\

            float4 col : COLOR0;\    // Setup desired DX state

            float2 uv  : TEXCOORD0;\    ImGui_ImplDX11_SetupRenderState(draw_data, device);

        };\

        sampler sampler0;\    // Setup render state structure (for callbacks and custom texture bindings)

        Texture2D texture0;\    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

        float4 main(PS_INPUT input) : SV_Target\    ImGui_ImplDX11_RenderState render_state;

        {\    render_state.Device = bd->pd3dDevice;

            float4 out_col = input.col * texture0.Sample(sampler0, input.uv);\    render_state.DeviceContext = bd->pd3dDeviceContext;

            return out_col;\    render_state.SamplerDefault = bd->pTexSamplerLinear;

        }";    render_state.VertexConstantBuffer = bd->pVertexConstantBuffer;

    platform_io.Renderer_RenderState = &render_state;

    ID3DBlob* vertex_shader_blob = nullptr;

    ID3DBlob* pixel_shader_blob = nullptr;    // Render command lists

    ID3DBlob* error_blob = nullptr;    // (Because we merged all buffers into a single one, we maintain our own offset into them)

    int global_idx_offset = 0;

    if (FAILED(D3DCompile(vertex_shader, strlen(vertex_shader), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vertex_shader_blob, &error_blob)))    int global_vtx_offset = 0;

    {    ImVec2 clip_off = draw_data->DisplayPos;

        if (error_blob) error_blob->Release();    ImVec2 clip_scale = draw_data->FramebufferScale;

        return false;    for (const ImDrawList* draw_list : draw_data->CmdLists)

    }    {

    if (FAILED(D3DCompile(pixel_shader, strlen(pixel_shader), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &pixel_shader_blob, &error_blob)))        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)

    {        {

        vertex_shader_blob->Release();            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];

        if (error_blob) error_blob->Release();            if (pcmd->UserCallback != nullptr)

        return false;            {

    }                // User callback, registered via ImDrawList::AddCallback()

                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)

    if (FAILED(g_pd3dDevice->CreateVertexShader(vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), nullptr, &g_pVertexShader)))                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)

    {                    ImGui_ImplDX11_SetupRenderState(draw_data, device);

        vertex_shader_blob->Release();                else

        pixel_shader_blob->Release();                    pcmd->UserCallback(draw_list, pcmd);

        return false;            }

    }            else

    if (FAILED(g_pd3dDevice->CreatePixelShader(pixel_shader_blob->GetBufferPointer(), pixel_shader_blob->GetBufferSize(), nullptr, &g_pPixelShader)))            {

    {                // Project scissor/clipping rectangles into framebuffer space

        vertex_shader_blob->Release();                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);

        pixel_shader_blob->Release();                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

        return false;                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)

    }                    continue;



    D3D11_INPUT_ELEMENT_DESC local_layout[] = {                // Apply scissor/clipping rectangle

        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },                const D3D11_RECT r = { (LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y };

        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },                device->RSSetScissorRects(1, &r);

        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)IM_OFFSETOF(ImDrawVert, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },

    };                // Bind texture, Draw

    if (FAILED(g_pd3dDevice->CreateInputLayout(local_layout, 3, vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), &g_pInputLayout)))                ID3D11ShaderResourceView* texture_srv = (ID3D11ShaderResourceView*)pcmd->GetTexID();

    {                device->PSSetShaderResources(0, 1, &texture_srv);

        vertex_shader_blob->Release();                device->DrawIndexed(pcmd->ElemCount, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset);

        pixel_shader_blob->Release();            }

        return false;        }

    }        global_idx_offset += draw_list->IdxBuffer.Size;

        global_vtx_offset += draw_list->VtxBuffer.Size;

    vertex_shader_blob->Release();    }

    pixel_shader_blob->Release();    platform_io.Renderer_RenderState = nullptr;



    // Constant buffer    // Restore modified DX state

    D3D11_BUFFER_DESC desc;    device->RSSetScissorRects(old.ScissorRectsCount, old.ScissorRects);

    ZeroMemory(&desc, sizeof(desc));    device->RSSetViewports(old.ViewportsCount, old.Viewports);

    desc.Usage = D3D11_USAGE_DYNAMIC;    device->RSSetState(old.RS); if (old.RS) old.RS->Release();

    desc.ByteWidth = sizeof(VertexConstantBuffer);    device->OMSetBlendState(old.BlendState, old.BlendFactor, old.SampleMask); if (old.BlendState) old.BlendState->Release();

    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;    device->OMSetDepthStencilState(old.DepthStencilState, old.StencilRef); if (old.DepthStencilState) old.DepthStencilState->Release();

    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;    device->PSSetShaderResources(0, 1, &old.PSShaderResource); if (old.PSShaderResource) old.PSShaderResource->Release();

    g_pd3dDevice->CreateBuffer(&desc, nullptr, &g_pVertexConstantBuffer);    device->PSSetSamplers(0, 1, &old.PSSampler); if (old.PSSampler) old.PSSampler->Release();

    device->PSSetShader(old.PS, old.PSInstances, old.PSInstancesCount); if (old.PS) old.PS->Release();

    // Blend state    for (UINT i = 0; i < old.PSInstancesCount; i++) if (old.PSInstances[i]) old.PSInstances[i]->Release();

    D3D11_BLEND_DESC blend_desc = {};    device->VSSetShader(old.VS, old.VSInstances, old.VSInstancesCount); if (old.VS) old.VS->Release();

    blend_desc.RenderTarget[0].BlendEnable = TRUE;    device->VSSetConstantBuffers(0, 1, &old.VSConstantBuffer); if (old.VSConstantBuffer) old.VSConstantBuffer->Release();

    blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;    device->GSSetShader(old.GS, old.GSInstances, old.GSInstancesCount); if (old.GS) old.GS->Release();

    blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;    for (UINT i = 0; i < old.VSInstancesCount; i++) if (old.VSInstances[i]) old.VSInstances[i]->Release();

    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;    device->IASetPrimitiveTopology(old.PrimitiveTopology);

    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;    device->IASetIndexBuffer(old.IndexBuffer, old.IndexBufferFormat, old.IndexBufferOffset); if (old.IndexBuffer) old.IndexBuffer->Release();

    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;    device->IASetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset); if (old.VertexBuffer) old.VertexBuffer->Release();

    blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;    device->IASetInputLayout(old.InputLayout); if (old.InputLayout) old.InputLayout->Release();

    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;}

    g_pd3dDevice->CreateBlendState(&blend_desc, &g_pBlendState);

static void ImGui_ImplDX11_DestroyTexture(ImTextureData* tex)

    // Rasterizer state{

    D3D11_RASTERIZER_DESC rs_desc = {};    if (ImGui_ImplDX11_Texture* backend_tex = (ImGui_ImplDX11_Texture*)tex->BackendUserData)

    rs_desc.FillMode = D3D11_FILL_SOLID;    {

    rs_desc.CullMode = D3D11_CULL_NONE;        IM_ASSERT(backend_tex->pTextureView == (ID3D11ShaderResourceView*)(intptr_t)tex->TexID);

    rs_desc.ScissorEnable = TRUE;        backend_tex->pTextureView->Release();

    rs_desc.DepthClipEnable = TRUE;        backend_tex->pTexture->Release();

    g_pd3dDevice->CreateRasterizerState(&rs_desc, &g_pRasterizerState);        IM_DELETE(backend_tex);



    // Depth stencil state        // Clear identifiers and mark as destroyed (in order to allow e.g. calling InvalidateDeviceObjects while running)

    D3D11_DEPTH_STENCIL_DESC ds_desc = {};        tex->SetTexID(ImTextureID_Invalid);

    ds_desc.DepthEnable = FALSE;        tex->BackendUserData = nullptr;

    ds_desc.StencilEnable = FALSE;    }

    g_pd3dDevice->CreateDepthStencilState(&ds_desc, &g_pDepthStencilState);    tex->SetStatus(ImTextureStatus_Destroyed);

}

    // Sampler

    D3D11_SAMPLER_DESC sampler_desc = {};void ImGui_ImplDX11_UpdateTexture(ImTextureData* tex)

    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;{

    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();

    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;    if (tex->Status == ImTextureStatus_WantCreate)

    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;    {

    sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;        // Create and upload new texture to graphics system

    sampler_desc.MinLOD = 0;        //IMGUI_DEBUG_LOG("UpdateTexture #%03d: WantCreate %dx%d\n", tex->UniqueID, tex->Width, tex->Height);

    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;        IM_ASSERT(tex->TexID == ImTextureID_Invalid && tex->BackendUserData == nullptr);

    g_pd3dDevice->CreateSamplerState(&sampler_desc, &g_pFontSampler);        IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);

        unsigned int* pixels = (unsigned int*)tex->GetPixels();

    // Create font texture        ImGui_ImplDX11_Texture* backend_tex = IM_NEW(ImGui_ImplDX11_Texture)();

    unsigned char* pixels;

    int width, height;        // Create texture

    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);        D3D11_TEXTURE2D_DESC desc;

        ZeroMemory(&desc, sizeof(desc));

    D3D11_TEXTURE2D_DESC tex_desc = {};        desc.Width = (UINT)tex->Width;

    tex_desc.Width = width;        desc.Height = (UINT)tex->Height;

    tex_desc.Height = height;        desc.MipLevels = 1;

    tex_desc.MipLevels = 1;        desc.ArraySize = 1;

    tex_desc.ArraySize = 1;        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;        desc.SampleDesc.Count = 1;

    tex_desc.SampleDesc.Count = 1;        desc.Usage = D3D11_USAGE_DEFAULT;

    tex_desc.Usage = D3D11_USAGE_DEFAULT;        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;        desc.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA subResource;

    D3D11_SUBRESOURCE_DATA tex_data = {};        subResource.pSysMem = pixels;

    tex_data.pSysMem = pixels;        subResource.SysMemPitch = desc.Width * 4;

    tex_data.SysMemPitch = width * 4;        subResource.SysMemSlicePitch = 0;

        bd->pd3dDevice->CreateTexture2D(&desc, &subResource, &backend_tex->pTexture);

    ID3D11Texture2D* tex = nullptr;        IM_ASSERT(backend_tex->pTexture != nullptr && "Backend failed to create texture!");

    g_pd3dDevice->CreateTexture2D(&tex_desc, &tex_data, &tex);

        // Create texture view

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;

    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;        ZeroMemory(&srvDesc, sizeof(srvDesc));

    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    srv_desc.Texture2D.MipLevels = 1;        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

    g_pd3dDevice->CreateShaderResourceView(tex, &srv_desc, &g_pFontTextureView);        srvDesc.Texture2D.MipLevels = desc.MipLevels;

        srvDesc.Texture2D.MostDetailedMip = 0;

    tex->Release();        bd->pd3dDevice->CreateShaderResourceView(backend_tex->pTexture, &srvDesc, &backend_tex->pTextureView);

        IM_ASSERT(backend_tex->pTextureView != nullptr && "Backend failed to create texture!");

    ImGui::GetIO().Fonts->SetTexID(g_pFontTextureView);

        // Store identifiers

    return true;        tex->SetTexID((ImTextureID)(intptr_t)backend_tex->pTextureView);

}        tex->SetStatus(ImTextureStatus_OK);
        tex->BackendUserData = backend_tex;
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
    {
        // Update selected blocks. We only ever write to textures regions which have never been used before!
        // This backend choose to use tex->Updates[] but you can use tex->UpdateRect to upload a single region.
        ImGui_ImplDX11_Texture* backend_tex = (ImGui_ImplDX11_Texture*)tex->BackendUserData;
        IM_ASSERT(backend_tex->pTextureView == (ID3D11ShaderResourceView*)(intptr_t)tex->TexID);
        for (ImTextureRect& r : tex->Updates)
        {
            D3D11_BOX box = { (UINT)r.x, (UINT)r.y, (UINT)0, (UINT)(r.x + r.w), (UINT)(r.y + r .h), (UINT)1 };
            bd->pd3dDeviceContext->UpdateSubresource(backend_tex->pTexture, 0, &box, tex->GetPixelsAt(r.x, r.y), (UINT)tex->GetPitch(), 0);
        }
        tex->SetStatus(ImTextureStatus_OK);
    }
    if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0)
        ImGui_ImplDX11_DestroyTexture(tex);
}

bool    ImGui_ImplDX11_CreateDeviceObjects()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd->pd3dDevice)
        return false;
    ImGui_ImplDX11_InvalidateDeviceObjects();

    // By using D3DCompile() from <d3dcompiler.h> / d3dcompiler.lib, we introduce a dependency to a given version of d3dcompiler_XX.dll (see D3DCOMPILER_DLL_A)
    // If you would like to use this DX11 sample code but remove this dependency you can:
    //  1) compile once, save the compiled shader blobs into a file or source code and pass them to CreateVertexShader()/CreatePixelShader() [preferred solution]
    //  2) use code to detect any version of the DLL and grab a pointer to D3DCompile from the DLL.
    // See https://github.com/ocornut/imgui/pull/638 for sources and details.

    // Create the vertex shader
    {
        static const char* vertexShader =
            "cbuffer vertexBuffer : register(b0) \
            {\
              float4x4 ProjectionMatrix; \
            };\
            struct VS_INPUT\
            {\
              float2 pos : POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            struct PS_INPUT\
            {\
              float4 pos : SV_POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            PS_INPUT main(VS_INPUT input)\
            {\
              PS_INPUT output;\
              output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
              output.col = input.col;\
              output.uv  = input.uv;\
              return output;\
            }";

        ID3DBlob* vertexShaderBlob;
        if (FAILED(D3DCompile(vertexShader, strlen(vertexShader), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &vertexShaderBlob, nullptr)))
            return false; // NB: Pass ID3DBlob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
        if (bd->pd3dDevice->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), nullptr, &bd->pVertexShader) != S_OK)
        {
            vertexShaderBlob->Release();
            return false;
        }

        // Create the input layout
        D3D11_INPUT_ELEMENT_DESC local_layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(ImDrawVert, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(ImDrawVert, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)offsetof(ImDrawVert, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (bd->pd3dDevice->CreateInputLayout(local_layout, 3, vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), &bd->pInputLayout) != S_OK)
        {
            vertexShaderBlob->Release();
            return false;
        }
        vertexShaderBlob->Release();

        // Create the constant buffer
        {
            D3D11_BUFFER_DESC desc = {};
            desc.ByteWidth = sizeof(VERTEX_CONSTANT_BUFFER_DX11);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags = 0;
            bd->pd3dDevice->CreateBuffer(&desc, nullptr, &bd->pVertexConstantBuffer);
        }
    }

    // Create the pixel shader
    {
        static const char* pixelShader =
            "struct PS_INPUT\
            {\
            float4 pos : SV_POSITION;\
            float4 col : COLOR0;\
            float2 uv  : TEXCOORD0;\
            };\
            sampler sampler0;\
            Texture2D texture0;\
            \
            float4 main(PS_INPUT input) : SV_Target\
            {\
            float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
            return out_col; \
            }";

        ID3DBlob* pixelShaderBlob;
        if (FAILED(D3DCompile(pixelShader, strlen(pixelShader), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &pixelShaderBlob, nullptr)))
            return false; // NB: Pass ID3DBlob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
        if (bd->pd3dDevice->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, &bd->pPixelShader) != S_OK)
        {
            pixelShaderBlob->Release();
            return false;
        }
        pixelShaderBlob->Release();
    }

    // Create the blending setup
    {
        D3D11_BLEND_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.AlphaToCoverageEnable = false;
        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        bd->pd3dDevice->CreateBlendState(&desc, &bd->pBlendState);
    }

    // Create the rasterizer state
    {
        D3D11_RASTERIZER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.ScissorEnable = true;
        desc.DepthClipEnable = true;
        bd->pd3dDevice->CreateRasterizerState(&desc, &bd->pRasterizerState);
    }

    // Create depth-stencil State
    {
        D3D11_DEPTH_STENCIL_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.DepthEnable = false;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        desc.StencilEnable = false;
        desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.BackFace = desc.FrontFace;
        bd->pd3dDevice->CreateDepthStencilState(&desc, &bd->pDepthStencilState);
    }

    // Create texture sampler
    // (Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling)
    {
        D3D11_SAMPLER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.MipLODBias = 0.f;
        desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        desc.MinLOD = 0.f;
        desc.MaxLOD = 0.f;
        bd->pd3dDevice->CreateSamplerState(&desc, &bd->pTexSamplerLinear);
    }

    return true;
}

void    ImGui_ImplDX11_InvalidateDeviceObjects()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd->pd3dDevice)
        return;

    // Destroy all textures
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
        if (tex->RefCount == 1)
            ImGui_ImplDX11_DestroyTexture(tex);

    if (bd->pTexSamplerLinear)      { bd->pTexSamplerLinear->Release(); bd->pTexSamplerLinear = nullptr; }
    if (bd->pIB)                    { bd->pIB->Release(); bd->pIB = nullptr; }
    if (bd->pVB)                    { bd->pVB->Release(); bd->pVB = nullptr; }
    if (bd->pBlendState)            { bd->pBlendState->Release(); bd->pBlendState = nullptr; }
    if (bd->pDepthStencilState)     { bd->pDepthStencilState->Release(); bd->pDepthStencilState = nullptr; }
    if (bd->pRasterizerState)       { bd->pRasterizerState->Release(); bd->pRasterizerState = nullptr; }
    if (bd->pPixelShader)           { bd->pPixelShader->Release(); bd->pPixelShader = nullptr; }
    if (bd->pVertexConstantBuffer)  { bd->pVertexConstantBuffer->Release(); bd->pVertexConstantBuffer = nullptr; }
    if (bd->pInputLayout)           { bd->pInputLayout->Release(); bd->pInputLayout = nullptr; }
    if (bd->pVertexShader)          { bd->pVertexShader->Release(); bd->pVertexShader = nullptr; }
}

bool    ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    ImGui_ImplDX11_Data* bd = IM_NEW(ImGui_ImplDX11_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_dx11";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;   // We can honor ImGuiPlatformIO::Textures[] requests during render.

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_TextureMaxWidth = platform_io.Renderer_TextureMaxHeight = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;

    // Get factory from device
    IDXGIDevice* pDXGIDevice = nullptr;
    IDXGIAdapter* pDXGIAdapter = nullptr;
    IDXGIFactory* pFactory = nullptr;

    if (device->QueryInterface(IID_PPV_ARGS(&pDXGIDevice)) == S_OK)
        if (pDXGIDevice->GetParent(IID_PPV_ARGS(&pDXGIAdapter)) == S_OK)
            if (pDXGIAdapter->GetParent(IID_PPV_ARGS(&pFactory)) == S_OK)
            {
                bd->pd3dDevice = device;
                bd->pd3dDeviceContext = device_context;
                bd->pFactory = pFactory;
            }
    if (pDXGIDevice) pDXGIDevice->Release();
    if (pDXGIAdapter) pDXGIAdapter->Release();
    bd->pd3dDevice->AddRef();
    bd->pd3dDeviceContext->AddRef();

    return true;
}

void ImGui_ImplDX11_Shutdown()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    ImGui_ImplDX11_InvalidateDeviceObjects();
    if (bd->pFactory)             { bd->pFactory->Release(); }
    if (bd->pd3dDevice)           { bd->pd3dDevice->Release(); }
    if (bd->pd3dDeviceContext)    { bd->pd3dDeviceContext->Release(); }

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
    platform_io.ClearRendererHandlers();
    IM_DELETE(bd);
}

void ImGui_ImplDX11_NewFrame()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplDX11_Init()?");

    if (!bd->pVertexShader)
        if (!ImGui_ImplDX11_CreateDeviceObjects())
            IM_ASSERT(0 && "ImGui_ImplDX11_CreateDeviceObjects() failed!");
}

//-----------------------------------------------------------------------------

#endif // #ifndef IMGUI_DISABLE
