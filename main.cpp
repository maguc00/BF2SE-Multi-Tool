#include <windows.h>
#include <iostream>

//config reading
#include <unordered_map>
#include <string>

#include "GameStructs.h"
#include "GameFunctions.h"
#include "GameGlobals.h"

#include "minHook/MinHook.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include <tchar.h>

#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "minhook/libMinHook.x86.lib")

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

constexpr int REQUIRED_FRAMEWORK_API_VERSION = 1;

struct FrameworkAPI
{
    int version;
    void (*Log)(const char* msg);
    bool (*Patch)(int address, const char* patch, size_t size);
    std::unordered_map<std::string, std::string>(*LoadConfig)(const std::string& path);
    LightManager* (*GetLightManager)();
    Terrain* (*GetTerrain)();
    GameGlobals* Globals;
    GameFunctions* GameFunctions;
    HWND(*GetWindowHandle)();
    int* (*GetD3D9Device)();
};

struct PluginAPI
{
    int required_framework_version = REQUIRED_FRAMEWORK_API_VERSION;
    const char* name = "Multi-Tool";
    const char* author = "maguc";
    const char* description = "GUI Tool to play around with stuffs";
    const char* version = "1.0";
    bool unload_after_init = false;
};

static FrameworkAPI* frameworkAPI;
static PluginAPI pluginAPI;

LightManager* lightManager;
Terrain* terrain;
HudColor* hudColor;

IDirect3DDevice9* d3d9_device = nullptr;
IDirect3DTexture9* my_texture = nullptr;
int width, height, channels;

ImGuiIO io;
HWND hWnd;

bool showImGuiWindow = false;
bool g_showImGuiDemoWindow = false;
bool replaceTextures = false;

typedef HRESULT(WINAPI* DrawPrimitive_t)(LPDIRECT3DDEVICE9, D3DPRIMITIVETYPE, UINT, UINT);
DrawPrimitive_t g_pOrigDrawPrimitive = nullptr;

typedef HRESULT(WINAPI* DrawIndexedPrimitive_t)(LPDIRECT3DDEVICE9, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
DrawIndexedPrimitive_t g_pOrigDrawIndexedPrimitive = nullptr;

typedef HRESULT(APIENTRY* EndSceneFn)(IDirect3DDevice9*);
EndSceneFn oEndScene = nullptr;


// disabled until a proper way is found to handle the mouse in-game
/*
WNDPROC oWndProc = nullptr;
// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;

LRESULT WINAPI MyWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

    if (showImGuiWindow)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        // Falls ImGui den Input "wünscht", NICHT weiterleiten
        
        if (io.WantCaptureMouse && (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST))
            return true;
        if (io.WantCaptureKeyboard && (msg >= WM_KEYFIRST && msg <= WM_KEYLAST))
            return true;
    }

    switch (msg)
    {
    
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
}
*/

bool InitializeImGui(IDirect3DDevice9* pDevice)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;		// Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;		// Enable Gamepad Controls

    hWnd = frameworkAPI->GetWindowHandle();

    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX9_Init(pDevice);
    
    //disabled until a proper way is found to handle the mouse in - game
    //oWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)MyWndProc);

    return true;
}

HCURSOR cursor = LoadCursor(NULL, IDC_ARROW);

void ToggleImGuiWindow()
{
    showImGuiWindow = !showImGuiWindow;

    if (showImGuiWindow)
    {
        POINT pt = { 100, 100 };
        ClientToScreen(hWnd, &pt);
        SetCursorPos(pt.x, pt.y);

        while (ShowCursor(TRUE) < 0);           // "ShowCursor() doesn't strictly show/hide — it increments/decrements an internal counter.
                                                // The cursor is only visible/invisible if the counter is >= 0 or <= 0."
        
        SetCursor(cursor);                      // To draw an Cursor, since apparently there isn't none in fullscreen mode lol
        ImGui::GetIO().MouseDrawCursor = true;  // technically redundant since my "own" cursor is used and it reflects its position towards Dear ImGUI one which isn't visible.
    }
    else
    {
        while (ShowCursor(FALSE) >= 0);
        SetCursor(NULL);
        ImGui::GetIO().MouseDrawCursor = false;
    }
}

void CreateTexture(IDirect3DDevice9* pDevice)
{
    unsigned char* data = stbi_load("my_image.png", &width, &height, &channels, 4);
    
    if (SUCCEEDED(pDevice->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &my_texture, NULL)))
    {
        D3DLOCKED_RECT rect;
        if (SUCCEEDED(my_texture->LockRect(0, &rect, NULL, 0)))
        {
            // swap R and B channel, otherwise colors are messed up due to different color channels
            for (int i = 0; i < width * height; ++i)
            {
                unsigned char* pixel = data + i * 4;
                std::swap(pixel[0], pixel[2]); // R <-> B
            }

            // Copy pixels row by row
            for (int y = 0; y < height; y++)
            {
                memcpy((BYTE*)rect.pBits + y * rect.Pitch, data + y * width * 4, width * 4);
            }

            my_texture->UnlockRect(0);
        }
    }

    stbi_image_free(data);
}

HRESULT WINAPI HookedDrawIndexedPrimitive(LPDIRECT3DDEVICE9 pDevice, D3DPRIMITIVETYPE Type, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{

    if (replaceTextures)
    {
        D3DCAPS9 caps;
        DWORD maxTextureStages = 0;
        pDevice->GetDeviceCaps(&caps);
        maxTextureStages = caps.MaxTextureBlendStages;

        for (int i = 0; i < maxTextureStages; ++i)
        {
            pDevice->SetTexture(i, my_texture);
        }

        D3DCAPS9 caps2;
        pDevice->GetDeviceCaps(&caps2);
        DWORD maxStages = caps.MaxSimultaneousTextures;

        for (int i = 0; i < maxStages; ++i)
        {
            pDevice->SetTexture(i, my_texture);
        }
    }

    return g_pOrigDrawIndexedPrimitive(pDevice, Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}



HRESULT WINAPI HookedDrawPrimitive(LPDIRECT3DDEVICE9 pDevice, D3DPRIMITIVETYPE Type, UINT StartVertex, UINT PrimitiveCount)
{
    /*
    pDevice->SetTexture(0, my_texture);
    pDevice->SetTexture(1, my_texture);
    pDevice->SetTexture(2, my_texture);
    pDevice->SetTexture(3, my_texture);
    pDevice->SetTexture(4, my_texture);
    pDevice->SetTexture(5, my_texture);
    pDevice->SetTexture(6, my_texture);
    pDevice->SetTexture(7, my_texture);
    pDevice->SetTexture(8, my_texture);
    pDevice->SetTexture(9, my_texture);
    pDevice->SetTexture(10, my_texture);
    */

    if (replaceTextures)
    {
        D3DCAPS9 caps;
        DWORD maxTextureStages = 0;
        pDevice->GetDeviceCaps(&caps);
        maxTextureStages = caps.MaxTextureBlendStages;

        for (int i = 0; i < maxTextureStages; ++i)
        {
            pDevice->SetTexture(i, my_texture);
        }

        D3DCAPS9 caps2;
        pDevice->GetDeviceCaps(&caps2);
        DWORD maxStages = caps.MaxSimultaneousTextures;

        for (int i = 0; i < maxStages; ++i)
        {
            pDevice->SetTexture(i, my_texture);
        }
    }
    return g_pOrigDrawPrimitive(pDevice, Type, StartVertex, PrimitiveCount);
}

bool rainbow = false;
bool rainbow_crosshair = false;
bool rainbow_lightmanager = false;
bool rainbow_terrain = false;

float R = 0.0f;
float G = 0.0f;
float B = 1.0f;
bool R_Peaked = false;
bool G_Peaked = false;
bool B_Peaked = true;

float speed = 0.05;


HRESULT APIENTRY hkEndScene(IDirect3DDevice9* pDevice)
{
    if (rainbow)
    {
        if (!R_Peaked && B_Peaked)
        {
            R += speed;
            B -= speed;

            if (R >= 1.0f)
            {
                R = 1.0f;
                R_Peaked = true;
            }

            if (B <= 0.0f)
            {
                B = 0.0f;
                B_Peaked = false;
            }
        }

        if (!G_Peaked && R_Peaked)
        {
            G += speed;
            R -= speed;

            if (G >= 1.0f)
            {
                G = 1.0f;
                G_Peaked = true;
            }

            if (R <= 0.0f)
            {
                R = 0.0f;
                R_Peaked = false;
            }
        }

        if (!B_Peaked && G_Peaked)
        {
            B += speed;
            G -= speed;

            if (B >= 1.0f)
            {
                B = 1.0f;
                B_Peaked = true;
            }

            if (G <= 0.0f)
            {
                G = 0.0f;
                G_Peaked = false;
            }
        }

        if (rainbow_crosshair)
        {
            frameworkAPI->Globals->HudColor->crosshair_R = R;
            frameworkAPI->Globals->HudColor->crosshair_G = G;
            frameworkAPI->Globals->HudColor->crosshair_B = B;
        }

        if (lightManager == nullptr)
            lightManager = frameworkAPI->GetLightManager();

        if (rainbow_lightmanager && lightManager != nullptr)
        {
            lightManager->skycolor_R = R;
            lightManager->skycolor_G = G;
            lightManager->skycolor_B = B;
            lightManager->treeSunColor_R = R;
            lightManager->treeSunColor_G = G;
            lightManager->treeSunColor_B = B;
            lightManager->treeSkyColor_R = R;
            lightManager->treeSkyColor_G = G;
            lightManager->treeSkyColor_B = B;
            lightManager->ambientcolor_R = R;
            lightManager->ambientcolor_G = G;
            lightManager->ambientcolor_B = B;
            lightManager->treeAmbientColor_R = R;
            lightManager->treeAmbientColor_G = G;
            lightManager->treeAmbientColor_B = B;
            lightManager->staticSunColor_R = R;
            lightManager->staticSunColor_G = G;
            lightManager->staticSunColor_B = B;
            lightManager->staticSkyColor_R = R;
            lightManager->staticSkyColor_G = G;
            lightManager->staticSkyColor_B = B;
            lightManager->staticSpecularColor_R = R;
            lightManager->staticSpecularColor_G = G;
            lightManager->staticSpecularColor_B = B;
            lightManager->singlePointColor_R = R;
            lightManager->singlePointColor_G = G;
            lightManager->singlePointColor_B = B;
            lightManager->effectSunColor_R = R;
            lightManager->effectSunColor_G = G;
            lightManager->effectSunColor_B = B;
            lightManager->effectShadowColor_R = R;
            lightManager->effectShadowColor_G = G;
            lightManager->effectShadowColor_B = B;
        }

        if (terrain == nullptr)
            terrain = frameworkAPI->GetTerrain();

        if (rainbow_terrain && terrain != nullptr)
        {
            terrain->sunColor_R = R;
            terrain->sunColor_G = G;
            terrain->sunColor_B = B;
            terrain->GIColor_R = R;
            terrain->GIColor_G = G;
            terrain->GIColor_B = B;
        }
    }

    if(!showImGuiWindow)
        return oEndScene(pDevice);

    // disabled until a proper way is found to use the mouse in-game
    /*
    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        //if (msg.message == WM_QUIT)
        //    done = true;
    }
    */

    // technically not required in windowed mode, but required in fullscreen mode
    POINT p;
    if (GetCursorPos(&p))
    {
        ScreenToClient(hWnd, &p);

        ImGuiIO& io = ImGui::GetIO();
        io.MousePos = ImVec2((float)p.x, (float)p.y);
    }

    // update mouse button states directly here since ImGUI can't read the Window Messages
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
    {
        ImGui::GetIO().AddMouseButtonEvent(0, true);
    }
    else
    {
        ImGui::GetIO().AddMouseButtonEvent(0, false);
    }

    ///////////////////////////////////////////////////////
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ///////////////////////////////////////////////////////

    ImGui::Begin("Multi-Tool");

    if (ImGui::BeginTabBar("MT-TabBar"))
    {

        if (ImGui::BeginTabItem("General"))
        {
            ImGui::Checkbox("Show Demo Window", &g_showImGuiDemoWindow);

            if (g_showImGuiDemoWindow)
                ImGui::ShowDemoWindow(&g_showImGuiDemoWindow);

            if (ImGui::Button("Exit"))
                ToggleImGuiWindow();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Player Manager"))
        {
            PlayerManager* playerManager = frameworkAPI->Globals->PlayerManager;

            ImGui::Text("Amount of players: %d", playerManager->num_players);

            if (playerManager->num_players > 0)
            {
                for (int i = 255; i >= 256 - playerManager->num_players; --i)
                {
                    Player* player = frameworkAPI->GameFunctions->GetPlayerByID(playerManager, i);

                    std::string label = "Player " + std::to_string(i);
                    std::string label2 = "Health###health" + std::to_string(i);

                    if (ImGui::TreeNode(label.c_str()))
                    {
                        if (player->vehicle->vehicleData->c != nullptr)
                            ImGui::DragFloat(label2.c_str(), &player->vehicle->vehicleData->c->health, 1.0f);

                        ImGui::TreePop();
                    }
                }
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Globals"))
        {
            ImGui::DragFloat("Jump Power", frameworkAPI->Globals->Jump_Power, 0.1f);
            ImGui::Checkbox("Local Player invuln. and no weapon heat", frameworkAPI->Globals->Is_Player_Invulnerable_And_No_Weapon_Heat);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("LightManager"))
        {
            if (lightManager == nullptr)
                lightManager = frameworkAPI->GetLightManager();

            if (lightManager != nullptr)
            {
                ImGui::DragFloat("SkyColor R", &lightManager->skycolor_R, 0.1f);
                ImGui::DragFloat("SkyColor G", &lightManager->skycolor_G, 0.1f);
                ImGui::DragFloat("SkyColor B", &lightManager->skycolor_B, 0.1f);

                ImGui::DragFloat("Tree SunColor R", &lightManager->treeSunColor_R, 0.1f);
                ImGui::DragFloat("Tree SunColor G", &lightManager->treeSunColor_G, 0.1f);
                ImGui::DragFloat("Tree SunColor B", &lightManager->treeSunColor_B, 0.1f);

                ImGui::DragFloat("Tree SkyColor R", &lightManager->treeSkyColor_R, 0.1f);
                ImGui::DragFloat("Tree SkyColor G", &lightManager->treeSkyColor_G, 0.1f);
                ImGui::DragFloat("Tree SkyColor B", &lightManager->treeSkyColor_B, 0.1f);

                ImGui::DragFloat("AmbientColor R", &lightManager->ambientcolor_R, 0.1f);
                ImGui::DragFloat("AmbientColor G", &lightManager->ambientcolor_G, 0.1f);
                ImGui::DragFloat("AmbientColor B", &lightManager->ambientcolor_B, 0.1f);

                ImGui::DragFloat("Tree AmbientColor R", &lightManager->treeAmbientColor_R, 0.1f);
                ImGui::DragFloat("Tree AmbientColor G", &lightManager->treeAmbientColor_G, 0.1f);
                ImGui::DragFloat("Tree AmbientColor B", &lightManager->treeAmbientColor_B, 0.1f);

                ImGui::DragFloat("Static SunColor R", &lightManager->staticSunColor_R, 0.1f);
                ImGui::DragFloat("Static SunColor G", &lightManager->staticSunColor_G, 0.1f);
                ImGui::DragFloat("Static SunColor B", &lightManager->staticSunColor_B, 0.1f);

                ImGui::DragFloat("Static SkyColor R", &lightManager->staticSkyColor_R, 0.1f);
                ImGui::DragFloat("Static SkyColor G", &lightManager->staticSkyColor_G, 0.1f);
                ImGui::DragFloat("Static SkyColor B", &lightManager->staticSkyColor_B, 0.1f);

                ImGui::DragFloat("Static SpecularColor R", &lightManager->staticSpecularColor_R, 0.1f);
                ImGui::DragFloat("Static SpecularColor G", &lightManager->staticSpecularColor_G, 0.1f);
                ImGui::DragFloat("Static SpecularColor B", &lightManager->staticSpecularColor_B, 0.1f);

                ImGui::DragFloat("Single PointColor R", &lightManager->singlePointColor_R, 0.1f);
                ImGui::DragFloat("Single PointColor G", &lightManager->singlePointColor_G, 0.1f);
                ImGui::DragFloat("Single PointColor B", &lightManager->singlePointColor_B, 0.1f);

                ImGui::DragFloat("Effect SunColor R", &lightManager->effectSunColor_R, 0.1f);
                ImGui::DragFloat("Effect SunColor G", &lightManager->effectSunColor_G, 0.1f);
                ImGui::DragFloat("Effect SunColor B", &lightManager->effectSunColor_B, 0.1f);

                ImGui::DragFloat("Effect ShadowColor R", &lightManager->effectShadowColor_R, 0.1f);
                ImGui::DragFloat("Effect ShadowColor G", &lightManager->effectShadowColor_G, 0.1f);
                ImGui::DragFloat("Effect ShadowColor B", &lightManager->effectShadowColor_B, 0.1f);
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Terrain"))
        {
            if (terrain == nullptr)
                terrain = frameworkAPI->GetTerrain();

            if (terrain != nullptr)
            {
                ImGui::DragFloat("SunColor R", &terrain->sunColor_R, 0.1f);
                ImGui::DragFloat("SunColor G", &terrain->sunColor_G, 0.1f);
                ImGui::DragFloat("SunColor B", &terrain->sunColor_B, 0.1f);

                ImGui::DragFloat("GIColor R", &terrain->GIColor_R, 0.1f);
                ImGui::DragFloat("GIColor G", &terrain->GIColor_G, 0.1f);
                ImGui::DragFloat("GIColor B", &terrain->GIColor_B, 0.1f);
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Crosshair"))
        {
            if (hudColor == nullptr)
                hudColor = frameworkAPI->Globals->HudColor;

            if (hudColor != nullptr)
            {
                ImGui::DragFloat("Crosshair R", &hudColor->crosshair_R, 0.1f);
                ImGui::DragFloat("Crosshair G", &hudColor->crosshair_G, 0.1f);
                ImGui::DragFloat("Crosshair B", &hudColor->crosshair_B, 0.1f);
                ImGui::DragFloat("Crosshair A", &hudColor->crosshair_A, 0.1f);
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Fun"))
        {
            if (ImGui::Button("Kill All"))
            {
                PlayerManager* playerManager = frameworkAPI->Globals->PlayerManager;

                if (playerManager->num_players > 0)
                {
                    for (int i = 255; i >= 256 - playerManager->num_players; --i)
                    {
                        Player* player = frameworkAPI->GameFunctions->GetPlayerByID(playerManager, i);
                        
                        if (player->vehicle->vehicleData->c != nullptr)
                            player->vehicle->vehicleData->c->health = 0.0f;
                    }
                }
            }

            ImGui::Checkbox("Replace All Textures", &replaceTextures);
            ImGui::Checkbox("Rainbow", &rainbow);
            ImGui::Checkbox("Rainbow Crosshair", &rainbow_crosshair);
            ImGui::Checkbox("Rainbow Light", &rainbow_lightmanager);
            ImGui::Checkbox("Rainbow Terrain", &rainbow_terrain);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();

    ///////////////////////////////////////////////////////
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    ///////////////////////////////////////////////////////
    
    return oEndScene(pDevice);
}

DWORD WINAPI KeyboardListener(LPVOID param)
{
    std::cout << "[+] Starting Keyboard listener..." << std::endl;
    while (true)
    {
        if (GetAsyncKeyState(VK_F11))
        {
            if (d3d9_device == nullptr)
            {
                
                d3d9_device = reinterpret_cast<IDirect3DDevice9*>(frameworkAPI->GetD3D9Device());
                
                void** vtable = *reinterpret_cast<void***>(d3d9_device);
                
                InitializeImGui(d3d9_device);
                CreateTexture(d3d9_device);

                MH_Initialize();

                void* endSceneAddr = vtable[42];
                MH_CreateHook(endSceneAddr, &hkEndScene, reinterpret_cast<void**>(&oEndScene));
                MH_EnableHook(endSceneAddr);

                void* drawIndexedPrimitiveAddr = vtable[82];
                MH_CreateHook(drawIndexedPrimitiveAddr, &HookedDrawIndexedPrimitive, reinterpret_cast<void**>(&g_pOrigDrawIndexedPrimitive));
                MH_EnableHook(drawIndexedPrimitiveAddr);

                void* drawPrimitiveAddr = vtable[81];
                MH_CreateHook(drawPrimitiveAddr, &HookedDrawPrimitive, reinterpret_cast<void**>(&g_pOrigDrawPrimitive));
                MH_EnableHook(drawPrimitiveAddr);

                ToggleImGuiWindow();
            }
            else
            {
                ToggleImGuiWindow();
            }
        }

        Sleep(100);
    }

    return 0;
}

extern "C" __declspec(dllexport)
PluginAPI* PluginInit(FrameworkAPI* api)
{
    frameworkAPI = api;

    CreateThread(NULL, 0, KeyboardListener, NULL, 0, NULL);

    return &pluginAPI;
}