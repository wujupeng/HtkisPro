#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include "resource.h"
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#ifdef _WIN32
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#endif
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include "Server.h"
#include "UserManager.h"
#include "Protocol.h"
#include "Localization.h"
#include "Logger.h"
#include "ConfigManager.h"
#include "StatsCollector.h"
#include "AuthFailureTracker.h"
#include "DnsCache.h"
#include "TlsContext.h"
#include "RelayManager.h"
#include "NatTraversalManager.h"
#include "BenchmarkEngine.h"
#include "StabilityMetrics.h"
#include "BufferPool.h"
#ifdef _WIN32
#include "CrashHandler.h"
#endif
#include <iostream>
#include <vector>
#include <ctime>
#include <algorithm>

// ===== Global State =====
static GLFWwindow* g_window = nullptr;
static bool g_shouldExit = false;
static bool g_useD3D11 = false;

#ifdef _WIN32
// ===== D3D11 State =====
static HWND g_d3d11Hwnd = nullptr;
static ID3D11Device* g_d3dDevice = nullptr;
static ID3D11DeviceContext* g_d3dContext = nullptr;
static IDXGISwapChain* g_swapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static bool g_d3d11WindowVisible = true;

// ===== System Tray =====
#define WM_TRAYICON (WM_USER + 1)
static NOTIFYICONDATAW g_nid = {};
static HWND g_hwndTray = nullptr;
static HICON g_hIcon = nullptr;
static ULONG_PTR g_gdiplusToken = 0;

static std::wstring UTF8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

static std::wstring GetExeDir() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) dir = dir.substr(0, pos);
    return dir;
}

// Load Gdiplus::Bitmap from embedded resource, fallback to file
static Gdiplus::Bitmap* LoadBitmapFromResourceOrFile(const std::wstring& fallbackPath) {
    HRSRC hRes = FindResourceA(NULL, MAKEINTRESOURCEA(IDR_PNG_LOGO), RT_RCDATA);
    if (hRes) {
        HGLOBAL hData = LoadResource(NULL, hRes);
        if (hData) {
            DWORD size = SizeofResource(NULL, hRes);
            void* pData = LockResource(hData);
            if (pData && size > 0) {
                HGLOBAL hCopy = GlobalAlloc(GMEM_MOVEABLE, size);
                if (hCopy) {
                    void* pCopy = GlobalLock(hCopy);
                    memcpy(pCopy, pData, size);
                    GlobalUnlock(hCopy);
                    IStream* pStream = NULL;
                    if (CreateStreamOnHGlobal(hCopy, TRUE, &pStream) == S_OK) {
                        Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromStream(pStream);
                        pStream->Release();
                        if (bitmap && bitmap->GetLastStatus() == Gdiplus::Ok) {
                            return bitmap;
                        }
                        delete bitmap;
                    }
                    GlobalFree(hCopy);
                }
            }
        }
    }
    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(fallbackPath.c_str());
    if (bitmap && bitmap->GetLastStatus() == Gdiplus::Ok) {
        return bitmap;
    }
    delete bitmap;
    return nullptr;
}

static HICON LoadIconFromPNG(const std::wstring& path, int targetSize) {
    Gdiplus::Bitmap* original = LoadBitmapFromResourceOrFile(path);
    if (!original) return nullptr;
    Gdiplus::Bitmap* resized = new Gdiplus::Bitmap(targetSize, targetSize, PixelFormat32bppARGB);
    Gdiplus::Graphics g(resized);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.DrawImage(original, 0, 0, targetSize, targetSize);
    HICON hIcon = nullptr;
    resized->GetHICON(&hIcon);
    delete original;
    delete resized;
    return hIcon;
}

static bool SetGLFWWindowIcon(const std::wstring& path, GLFWwindow* window) {
    Gdiplus::Bitmap* bitmap = LoadBitmapFromResourceOrFile(path);
    if (!bitmap) return false;
    int w = (int)bitmap->GetWidth();
    int h = (int)bitmap->GetHeight();
    Gdiplus::Rect rect(0, 0, w, h);
    Gdiplus::BitmapData bd;
    if (bitmap->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) != Gdiplus::Ok) {
        delete bitmap;
        return false;
    }
    std::vector<uint8_t> rgba(w * h * 4);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t* src = (uint8_t*)bd.Scan0 + y * bd.Stride + x * 4;
            uint8_t* dst = rgba.data() + (y * w + x) * 4;
            dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = src[3];
        }
    }
    bitmap->UnlockBits(&bd);
    delete bitmap;
    GLFWimage image;
    image.width = w; image.height = h; image.pixels = rgba.data();
    glfwSetWindowIcon(window, 1, &image);
    return true;
}

static void ShowMainWindow() {
    if (g_useD3D11 && g_d3d11Hwnd) {
        ShowWindow(g_d3d11Hwnd, SW_SHOW);
        SetForegroundWindow(g_d3d11Hwnd);
        g_d3d11WindowVisible = true;
    } else if (g_window) {
        glfwShowWindow(g_window);
        glfwFocusWindow(g_window);
    }
}

static LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_TRAYICON) {
        auto& loc = Localization::getInstance();
        if (LOWORD(lp) == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            std::wstring wShow = UTF8ToWide(loc.get("SHOW_WINDOW"));
            std::wstring wExit = UTF8ToWide(loc.get("EXIT"));
            AppendMenuW(hMenu, MF_STRING, 1, wShow.c_str());
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, 2, wExit.c_str());
            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
            if (cmd == 1) {
                ShowMainWindow();
            } else if (cmd == 2) {
                g_shouldExit = true;
                if (g_useD3D11 && g_d3d11Hwnd) PostMessage(g_d3d11Hwnd, WM_NULL, 0, 0);
                else if (g_window) glfwPostEmptyEvent();
            }
        } else if (LOWORD(lp) == WM_LBUTTONDBLCLK) {
            ShowMainWindow();
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void InitTray(HICON hIcon) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"HtkisProTrayClass";
    RegisterClassW(&wc);
    g_hwndTray = CreateWindowW(L"HtkisProTrayClass", L"", 0, 0, 0, 0, 0,
        HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwndTray;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = hIcon;
    wcscpy_s(g_nid.szTip, L"HtkisPro");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void CleanupTray() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    if (g_hwndTray) DestroyWindow(g_hwndTray);
    if (g_hIcon) DestroyIcon(g_hIcon);
}

static void ProcessTrayMessages() {
    MSG msg;
    while (PeekMessageW(&msg, g_hwndTray, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// ===== D3D11 Backend =====
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (pBackBuffer) {
        g_d3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

static LRESULT CALLBACK D3D11WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (g_d3dDevice && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_swapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        g_d3d11WindowVisible = false;
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_MINIMIZE) {
            ShowWindow(hWnd, SW_HIDE);
            g_d3d11WindowVisible = false;
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
#endif // _WIN32

// ===== GLFW Callbacks =====
static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void window_close_callback(GLFWwindow* window) {
    glfwSetWindowShouldClose(window, GLFW_FALSE);
    glfwHideWindow(window);
}

// ===== UI Helpers =====
void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();

    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
    builder.AddRanges(io.Fonts->GetGlyphRangesThai());
    builder.AddRanges(io.Fonts->GetGlyphRangesVietnamese());
    builder.BuildRanges(&ranges);

#ifdef _WIN32
    // Load primary font (Chinese + Latin), then merge Thai and Vietnamese fonts
    const char* primary_fonts[] = {
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\simsun.ttc",
    };
    bool loaded = false;
    for (const char* fp : primary_fonts) {
        FILE* f = fopen(fp, "rb");
        if (f) { fclose(f); io.Fonts->AddFontFromFileTTF(fp, 18.0f, nullptr, ranges.Data); loaded = true; break; }
    }
    if (!loaded) { io.Fonts->AddFontDefault(); return; }

    // Merge Thai font (tahoma supports Thai on Windows)
    ImFontConfig mergeCfg; mergeCfg.MergeMode = true;
    static ImVector<ImWchar> thaiRanges;
    ImFontGlyphRangesBuilder thaiBuilder;
    thaiBuilder.AddRanges(io.Fonts->GetGlyphRangesThai());
    thaiBuilder.BuildRanges(&thaiRanges);
    const char* thai_fonts[] = { "C:\\Windows\\Fonts\\tahoma.ttf", "C:\\Windows\\Fonts\\thonburi.ttc" };
    for (const char* fp : thai_fonts) {
        FILE* f = fopen(fp, "rb");
        if (f) { fclose(f); io.Fonts->AddFontFromFileTTF(fp, 18.0f, &mergeCfg, thaiRanges.Data); break; }
    }

    // Merge Vietnamese font
    static ImVector<ImWchar> vietRanges;
    ImFontGlyphRangesBuilder vietBuilder;
    vietBuilder.AddRanges(io.Fonts->GetGlyphRangesVietnamese());
    vietBuilder.BuildRanges(&vietRanges);
    const char* viet_fonts[] = { "C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\tahoma.ttf" };
    for (const char* fp : viet_fonts) {
        FILE* f = fopen(fp, "rb");
        if (f) { fclose(f); io.Fonts->AddFontFromFileTTF(fp, 18.0f, &mergeCfg, vietRanges.Data); break; }
    }
#else
    const char* font_candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/Supplemental/Thonburi.ttc",
    };
    bool loaded = false;
    for (const char* font_path : font_candidates) {
        FILE* f = fopen(font_path, "rb");
        if (f) { fclose(f); io.Fonts->AddFontFromFileTTF(font_path, 18.0f, NULL, ranges.Data); loaded = true; break; }
    }
    if (!loaded) io.Fonts->AddFontDefault();
#endif
}


void ApplyCustomStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ChildRounding = 6.0f;
    style.WindowPadding = ImVec2(16, 16);
    style.FramePadding = ImVec2(8, 5);
    style.ItemSpacing = ImVec2(10, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    colors[ImGuiCol_WindowBg]           = ImVec4(0.12f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.15f, 0.16f, 0.21f, 1.00f);
    colors[ImGuiCol_Border]             = ImVec4(0.25f, 0.27f, 0.33f, 0.50f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.20f, 0.21f, 0.27f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.26f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_FrameBgActive]      = ImVec4(0.30f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_TitleBg]            = ImVec4(0.10f, 0.11f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.14f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_MenuBarBg]          = ImVec4(0.14f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]        = ImVec4(0.10f, 0.11f, 0.15f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]      = ImVec4(0.30f, 0.32f, 0.40f, 0.60f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.40f, 0.50f, 0.70f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.45f, 0.47f, 0.58f, 0.80f);
    colors[ImGuiCol_CheckMark]          = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]         = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]   = ImVec4(0.55f, 0.80f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]             = ImVec4(0.25f, 0.45f, 0.75f, 0.80f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.30f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.20f, 0.38f, 0.65f, 1.00f);
    colors[ImGuiCol_Header]             = ImVec4(0.22f, 0.38f, 0.60f, 0.60f);
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.28f, 0.48f, 0.72f, 0.80f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.22f, 0.40f, 0.65f, 1.00f);
    colors[ImGuiCol_Separator]          = ImVec4(0.28f, 0.30f, 0.37f, 0.60f);
    colors[ImGuiCol_TableHeaderBg]      = ImVec4(0.16f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]  = ImVec4(0.30f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_TableBorderLight]   = ImVec4(0.22f, 0.24f, 0.30f, 0.60f);
    colors[ImGuiCol_TableRowBg]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]      = ImVec4(0.06f, 0.06f, 0.10f, 0.40f);
    colors[ImGuiCol_Text]               = ImVec4(0.88f, 0.90f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled]       = ImVec4(0.50f, 0.52f, 0.58f, 1.00f);
    colors[ImGuiCol_PopupBg]            = ImVec4(0.14f, 0.15f, 0.20f, 0.96f);
}

void StatusIndicator(bool running) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float r = 6.0f;
    ImU32 color = running ? IM_COL32(80, 220, 100, 255) : IM_COL32(220, 60, 60, 255);
    draw_list->AddCircleFilled(ImVec2(p.x + r, p.y + r), r, color);
    ImGui::Dummy(ImVec2(r * 2 + 4, r * 2));
}

// ===== Shared UI State =====
static int s_port = LanProxy::DEFAULT_SERVER_PORT;
static bool s_show_about = false;
static char s_username_buf[128] = {};
static char s_password_buf[128] = {};
static int s_days = 30;
static std::vector<std::pair<std::string, UserInfo>> s_users;
static std::vector<LogEntry> s_logEntries;
static bool s_autoScroll = true;

static void RenderTlsPanel();

// ===== Config Panel =====
static void RenderConfigPanel() {
    auto cfg = Config::ConfigManager::getInstance().getSnapshot();

    if (ImGui::BeginTabBar("##ConfigTabs")) {
        // IP ACL
        if (ImGui::BeginTabItem("IP ACL")) {
            ImGui::Spacing();
            const char* modes[] = {"Disabled", "Whitelist", "Blacklist"};
            int modeIdx = static_cast<int>(cfg->ipAcl.mode);
            if (ImGui::Combo("Mode", &modeIdx, modes, 3)) {
                Config::ConfigManager::getInstance().updateIpAcl(
                    [modeIdx](Config::IpAclConfig& c) { c.mode = static_cast<Config::AclMode>(modeIdx); });
            }
            ImGui::Spacing();
            ImGui::Text("Rules (%d):", (int)cfg->ipAcl.rules.size());
            static char cidrBuf[64] = {};
            ImGui::InputText("##CidrInput", cidrBuf, sizeof(cidrBuf));
            ImGui::SameLine();
            if (ImGui::Button("Add")) {
                if (strlen(cidrBuf) > 0) {
                    Config::ConfigManager::getInstance().updateIpAcl(
                        [&](Config::IpAclConfig& c) {
                            Config::CidrRule rule;
                            rule.network = cidrBuf;
                            rule.isIpv6 = strchr(cidrBuf, ':') != nullptr;
                            c.rules.push_back(rule);
                        });
                    cidrBuf[0] = '\0';
                }
            }
            ImGui::Separator();
            for (size_t i = 0; i < cfg->ipAcl.rules.size(); i++) {
                ImGui::Text("%s", cfg->ipAcl.rules[i].network.c_str());
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
                std::string label = "Del##" + std::to_string(i);
                if (ImGui::SmallButton(label.c_str())) {
                    Config::ConfigManager::getInstance().updateIpAcl(
                        [i](Config::IpAclConfig& c) { c.rules.erase(c.rules.begin() + i); });
                }
            }
            ImGui::EndTabItem();
        }

        // Rate Limit
        if (ImGui::BeginTabItem("Rate Limit")) {
            ImGui::Spacing();
            static uint64_t globalUp = 0, globalDown = 0;
            if (globalUp == 0) { globalUp = cfg->rateLimit.globalUpLimit; globalDown = cfg->rateLimit.globalDownLimit; }
            ImGui::Text("Global Upload Limit (bytes/s, 0=unlimited):");
            ImGui::InputScalar("##GlobalUp", ImGuiDataType_U64, &globalUp);
            ImGui::Text("Global Download Limit (bytes/s, 0=unlimited):");
            ImGui::InputScalar("##GlobalDown", ImGuiDataType_U64, &globalDown);
            if (ImGui::Button("Apply")) {
                Config::ConfigManager::getInstance().updateRateLimit(
                    [&](Config::RateLimitConfig& c) {
                        c.globalUpLimit = globalUp;
                        c.globalDownLimit = globalDown;
                    });
            }
            ImGui::EndTabItem();
        }

        // Connection Pool / DNS
        if (ImGui::BeginTabItem("Pool/DNS")) {
            ImGui::Spacing();
            ImGui::Text("Connection Pool");
            ImGui::Separator();
            static bool poolEnabled = false;
            static int maxConn = 256, idleTtl = 60;
            if (!poolEnabled) { poolEnabled = cfg->connectionPool.enabled; maxConn = cfg->connectionPool.maxConnections; idleTtl = cfg->connectionPool.idleTtlSeconds; }
            ImGui::Checkbox("Enabled", &poolEnabled);
            ImGui::InputInt("Max Connections", &maxConn);
            ImGui::InputInt("Idle TTL (s)", &idleTtl);
            if (ImGui::Button("Apply Pool")) {
                Config::ConfigManager::getInstance().updateConnectionPool(
                    [&](Config::ConnectionPoolConfig& c) {
                        c.enabled = poolEnabled;
                        c.maxConnections = static_cast<uint32_t>((std::max)(1, maxConn));
                        c.idleTtlSeconds = static_cast<uint32_t>((std::max)(1, idleTtl));
                    });
            }
            ImGui::Spacing();
            ImGui::Text("DNS Cache");
            ImGui::Separator();
            static bool dnsEnabled = false;
            static int dnsTtl = 300, dnsMax = 4096;
            if (!dnsEnabled) { dnsEnabled = cfg->dnsCache.enabled; dnsTtl = cfg->dnsCache.fixedTtlSeconds; dnsMax = cfg->dnsCache.maxEntries; }
            ImGui::Checkbox("Enabled##DNS", &dnsEnabled);
            ImGui::InputInt("TTL (s)##DNS", &dnsTtl);
            ImGui::InputInt("Max Entries##DNS", &dnsMax);
            if (ImGui::Button("Apply DNS")) {
                Config::ConfigManager::getInstance().updateDnsCache(
                    [&](Config::DnsCacheConfig& c) {
                        c.enabled = dnsEnabled;
                        c.fixedTtlSeconds = static_cast<uint32_t>((std::max)(1, dnsTtl));
                        c.maxEntries = static_cast<uint32_t>((std::max)(1, dnsMax));
                    });
            }
            ImGui::EndTabItem();
        }

        // Timeout
        if (ImGui::BeginTabItem("Timeout")) {
            ImGui::Spacing();
            static int firstPkt = 10, connectT = 10, idleT = 300, retryCnt = 3;
            static int initBackoff = 200, maxBackoff = 2000;
            if (firstPkt == 10) {
                firstPkt = cfg->timeout.firstPacketSeconds;
                connectT = cfg->timeout.connectSeconds;
                idleT = cfg->timeout.idleSeconds;
                retryCnt = cfg->timeout.retryCount;
                initBackoff = cfg->timeout.retryInitialBackoffMs;
                maxBackoff = cfg->timeout.retryMaxBackoffMs;
            }
            ImGui::InputInt("First Packet (s)", &firstPkt);
            ImGui::InputInt("Connect (s)", &connectT);
            ImGui::InputInt("Idle (s)", &idleT);
            ImGui::InputInt("Retry Count", &retryCnt);
            ImGui::InputInt("Initial Backoff (ms)", &initBackoff);
            ImGui::InputInt("Max Backoff (ms)", &maxBackoff);
            if (ImGui::Button("Apply")) {
                Config::ConfigManager::getInstance().updateTimeout(
                    [&](Config::TimeoutConfig& c) {
                        c.firstPacketSeconds = static_cast<uint32_t>((std::max)(1, firstPkt));
                        c.connectSeconds = static_cast<uint32_t>((std::max)(1, connectT));
                        c.idleSeconds = static_cast<uint32_t>((std::max)(1, idleT));
                        c.retryCount = static_cast<uint32_t>((std::max)(0, retryCnt));
                        c.retryInitialBackoffMs = static_cast<uint32_t>((std::max)(1, initBackoff));
                        c.retryMaxBackoffMs = static_cast<uint32_t>((std::max)(1, maxBackoff));
                    });
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("TLS")) {
            RenderTlsPanel();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

// ===== Stats Panel =====
static void RenderStatsPanel() {
    auto snap = StatsCollector::getInstance().getSnapshot();

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "Global Statistics");
    ImGui::Separator();
    ImGui::Text("Active Sessions: %u", snap.activeSessionCount);
    ImGui::Text("Total Upload: %llu bytes", (unsigned long long)snap.globalBytesUp);
    ImGui::Text("Total Download: %llu bytes", (unsigned long long)snap.globalBytesDown);
    ImGui::Text("Upload Rate: %llu bytes/s", (unsigned long long)snap.globalRateUp);
    ImGui::Text("Download Rate: %llu bytes/s", (unsigned long long)snap.globalRateDown);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "Active Sessions");
    ImGui::Separator();
    if (ImGui::BeginTable("##Sessions", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("User");
        ImGui::TableSetupColumn("Protocol");
        ImGui::TableSetupColumn("Target");
        ImGui::TableSetupColumn("Up");
        ImGui::TableSetupColumn("Down");
        ImGui::TableHeadersRow();
        for (const auto& s : snap.activeSessions) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", s.username.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s", s.protocol.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%s", s.target.c_str());
            ImGui::TableSetColumnIndex(3); ImGui::Text("%llu", (unsigned long long)s.bytesUp);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%llu", (unsigned long long)s.bytesDown);
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "Banned IPs");
    ImGui::Separator();
    auto blocked = AuthFailureTracker::getInstance().getBlocked();
    if (blocked.empty()) {
        ImGui::TextDisabled("No banned IPs");
    } else {
        for (const auto& [ip, remaining] : blocked) {
            ImGui::Text("%s (remaining: %ds)", ip.c_str(), remaining);
        }
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "DNS Cache");
    ImGui::Separator();
    ImGui::Text("Cache Size: %zu", DnsCache::getInstance().size());
    ImGui::Text("Cache Hits: %llu", (unsigned long long)DnsCache::getInstance().getHits());
    ImGui::Text("Cache Misses: %llu", (unsigned long long)DnsCache::getInstance().getMisses());
    if (ImGui::Button("Clear DNS Cache")) {
        DnsCache::getInstance().clear();
    }
}

// ===== v2.1 GUI Panels =====


static void RenderShadowsocksPanel() {
    auto cfg = Config::ConfigManager::getInstance().getSnapshot();
    static bool s_ss_enabled = cfg->shadowsocks.enabled;
    static char s_ss_password[128] = {};
    static int s_ss_method = 0;
    static int s_ss_port = 8388;

    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "Shadowsocks Configuration");
    ImGui::Spacing();
    ImGui::Checkbox("Enabled", &s_ss_enabled);
    ImGui::Spacing();

    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Port", &s_ss_port);
    if (s_ss_port < 1) s_ss_port = 1;
    if (s_ss_port > 65535) s_ss_port = 65535;

    ImGui::SetNextItemWidth(200);
    ImGui::InputText("Password", s_ss_password, sizeof(s_ss_password), ImGuiInputTextFlags_Password);

    const char* methods[] = {"AES-256-GCM", "AES-128-GCM", "ChaCha20-IETF-Poly1305"};
    ImGui::SetNextItemWidth(180);
    ImGui::Combo("Method", &s_ss_method, methods, 3);

    ImGui::Spacing();
    if (ImGui::Button("Apply", ImVec2(120, 28))) {
        bool en = s_ss_enabled; int port = s_ss_port; const char* pwd = s_ss_password; int meth = s_ss_method;
        Config::ConfigManager::getInstance().updateShadowsocks(
            [en, port, pwd, meth](Config::ShadowsocksConfig& c) {
                c.enabled = en;
                Config::SsPortConfig pc;
                pc.port = static_cast<uint16_t>(port);
                pc.passwordHash = pwd;
                pc.method = static_cast<Config::SsMethod>(meth);
                c.ports.clear();
                c.ports.push_back(pc);
            });
        Logger::getInstance().info("[GUI] Shadowsocks config applied");
    }
}

static void RenderVmessPanel() {
    auto cfg = Config::ConfigManager::getInstance().getSnapshot();
    static bool s_vmess_enabled = cfg->vmess.enabled;
    static int s_vmess_port = cfg->vmess.port;
    static int s_vmess_alterId = cfg->vmess.alterId;
    static char s_vmess_uuid[64] = {};

    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "VMess Configuration");
    ImGui::Spacing();
    ImGui::Checkbox("Enabled", &s_vmess_enabled);
    ImGui::Spacing();

    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Port", &s_vmess_port);
    if (s_vmess_port < 1) s_vmess_port = 1;
    if (s_vmess_port > 65535) s_vmess_port = 65535;

    ImGui::SetNextItemWidth(300);
    ImGui::InputText("UUID", s_vmess_uuid, sizeof(s_vmess_uuid));

    const char* alterIds[] = {"0", "16"};
    ImGui::SetNextItemWidth(80);
    ImGui::Combo("alterId", &s_vmess_alterId, alterIds, 2);

    ImGui::Spacing();
    if (ImGui::Button("Apply", ImVec2(120, 28))) {
        if (strlen(s_vmess_uuid) > 0 && !Config::isValidUuid(s_vmess_uuid)) {
            Logger::getInstance().warn("[GUI] Invalid UUID format");
        } else {
            bool en = s_vmess_enabled; int port = s_vmess_port; const char* uuid = s_vmess_uuid; int aid = s_vmess_alterId;
            Config::ConfigManager::getInstance().updateVmess(
                [en, port, uuid, aid](Config::VmessConfig& c) {
                    c.enabled = en;
                    c.port = static_cast<uint16_t>(port);
                    c.alterId = static_cast<uint8_t>(aid * 16);
                    if (strlen(uuid) > 0) {
                        c.uuids.clear();
                        c.uuids.push_back(uuid);
                    }
                });
            Logger::getInstance().info("[GUI] VMess config applied");
        }
    }
}

static void RenderVlessPanel() {
    auto cfg = Config::ConfigManager::getInstance().getSnapshot();
    static bool s_vless_enabled = cfg->vless.enabled;
    static int s_vless_port = cfg->vless.port;
    static int s_vless_transport = 0;
    static char s_vless_uuid[64] = {};

    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "VLESS Configuration");
    ImGui::Spacing();
    ImGui::Checkbox("Enabled", &s_vless_enabled);
    ImGui::Spacing();

    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Port", &s_vless_port);
    if (s_vless_port < 1) s_vless_port = 1;
    if (s_vless_port > 65535) s_vless_port = 65535;

    ImGui::SetNextItemWidth(300);
    ImGui::InputText("UUID", s_vless_uuid, sizeof(s_vless_uuid));

    const char* transports[] = {"TCP", "TLS", "WebSocket"};
    ImGui::SetNextItemWidth(120);
    ImGui::Combo("Transport", &s_vless_transport, transports, 3);

    if (s_vless_transport == 0) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "[Warning] TCP transport without TLS/WS is insecure!");
    }

    ImGui::Spacing();
    if (ImGui::Button("Apply", ImVec2(120, 28))) {
        if (strlen(s_vless_uuid) > 0 && !Config::isValidUuid(s_vless_uuid)) {
            Logger::getInstance().warn("[GUI] Invalid UUID format");
        } else {
            bool en = s_vless_enabled; int port = s_vless_port; const char* uuid = s_vless_uuid; int tp = s_vless_transport;
            Config::ConfigManager::getInstance().updateVless(
                [en, port, uuid, tp](Config::VlessConfig& c) {
                    c.enabled = en;
                    c.port = static_cast<uint16_t>(port);
                    c.transport = static_cast<Config::VlessTransport>(tp);
                    if (strlen(uuid) > 0) {
                        c.uuids.clear();
                        c.uuids.push_back(uuid);
                    }
                });
            Logger::getInstance().info("[GUI] VLESS config applied");
        }
    }
}

static void RenderTrojanPanel() {
    auto cfg = Config::ConfigManager::getInstance().getSnapshot();
    static bool s_trojan_enabled = cfg->trojan.enabled;
    static int s_trojan_port = cfg->trojan.port;
    static char s_trojan_password[128] = {};
    static char s_trojan_cert[256] = {};
    static char s_trojan_key[256] = {};

    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "Trojan Configuration");
    ImGui::Spacing();

    bool tlsReady = TlsContext::getInstance().isReady();
    if (!tlsReady) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "[TLS not configured] Trojan requires TLS certificate");
        ImGui::Spacing();
    }

    ImGui::Checkbox("Enabled", &s_trojan_enabled);
    if (!tlsReady) {
        s_trojan_enabled = false;
    }
    ImGui::Spacing();

    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Port", &s_trojan_port);
    if (s_trojan_port < 1) s_trojan_port = 1;
    if (s_trojan_port > 65535) s_trojan_port = 65535;

    ImGui::SetNextItemWidth(200);
    ImGui::InputText("Password", s_trojan_password, sizeof(s_trojan_password), ImGuiInputTextFlags_Password);

    ImGui::SetNextItemWidth(300);
    ImGui::InputText("TLS Cert Path", s_trojan_cert, sizeof(s_trojan_cert));
    ImGui::SetNextItemWidth(300);
    ImGui::InputText("TLS Key Path", s_trojan_key, sizeof(s_trojan_key));

    ImGui::Spacing();
    if (ImGui::Button("Apply", ImVec2(120, 28))) {
        bool en = s_trojan_enabled; int port = s_trojan_port; const char* pwd = s_trojan_password;
        const char* cert = s_trojan_cert; const char* key = s_trojan_key;
        Config::ConfigManager::getInstance().updateTrojan(
            [en, port, pwd, cert, key](Config::TrojanConfig& c) {
                c.enabled = en;
                c.port = static_cast<uint16_t>(port);
                c.tlsCertPath = cert;
                c.tlsKeyPath = key;
                if (strlen(pwd) > 0) {
                    c.passwordHashes.clear();
                    c.passwordHashes.push_back(pwd);
                }
            });
        Logger::getInstance().info("[GUI] Trojan config applied");
    }
}

static void RenderRelayNatPanel() {
    auto cfg = Config::ConfigManager::getInstance().getSnapshot();

    if (ImGui::BeginTabBar("##RelayNatTabs")) {
        if (ImGui::BeginTabItem("Relay")) {
            ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "Relay Nodes");
            ImGui::Spacing();

            auto statuses = RelayManager::getInstance().getAllStatus();
            if (statuses.empty()) {
                ImGui::TextDisabled("No relay nodes configured");
            } else {
                if (ImGui::BeginTable("##RelayNodes", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Node ID");
                    ImGui::TableSetupColumn("State");
                    ImGui::TableSetupColumn("Status");
                    ImGui::TableHeadersRow();
                    for (const auto& s : statuses) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", s.nodeId.c_str());
                        ImGui::TableSetColumnIndex(1);
                        const char* stateStr = "Unknown";
                        switch (s.state) {
                            case RelayTunnelClient::State::DISCONNECTED:   stateStr = "Disconnected"; break;
                            case RelayTunnelClient::State::CONNECTING:     stateStr = "Connecting"; break;
                            case RelayTunnelClient::State::AUTHENTICATING: stateStr = "Authing"; break;
                            case RelayTunnelClient::State::ACTIVE:         stateStr = "Active"; break;
                            case RelayTunnelClient::State::RECONNECTING:   stateStr = "Reconnecting"; break;
                        }
                        if (s.state == RelayTunnelClient::State::ACTIVE) {
                            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", stateStr);
                        } else {
                            ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.3f, 1.0f), "%s", stateStr);
                        }
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("up=%llu down=%llu", (unsigned long long)s.bytesUp, (unsigned long long)s.bytesDown);
                    }
                    ImGui::EndTable();
                }
            }

            ImGui::Spacing();
            static char s_relay_host[128] = {};
            static int s_relay_port = 443;
            static char s_relay_node_id[64] = {};
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("Server Host", s_relay_host, sizeof(s_relay_host));
            ImGui::SetNextItemWidth(80);
            ImGui::InputInt("Server Port", &s_relay_port);
            ImGui::SetNextItemWidth(120);
            ImGui::InputText("Node ID", s_relay_node_id, sizeof(s_relay_node_id));
            ImGui::Spacing();
            if (ImGui::Button("Add Node", ImVec2(100, 25))) {
                Config::RelayNodeConfig node;
                node.nodeId = s_relay_node_id;
                node.serverHost = s_relay_host;
                node.serverPort = static_cast<uint16_t>(s_relay_port);
                RelayManager::getInstance().addNode(node);
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove Node", ImVec2(120, 25))) {
                RelayManager::getInstance().removeNode(s_relay_node_id);
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("NAT")) {
            ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "NAT Traversal");
            ImGui::Spacing();

            static bool s_nat_enabled = cfg->nat.enabled;
            static char s_stun_server[128] = {};
            static int s_stun_port = 3478;
            static int s_keepalive = 20;

            ImGui::Checkbox("Enabled", &s_nat_enabled);
            ImGui::Spacing();
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("STUN Server", s_stun_server, sizeof(s_stun_server));
            ImGui::SetNextItemWidth(80);
            ImGui::InputInt("STUN Port", &s_stun_port);
            ImGui::SetNextItemWidth(80);
            ImGui::InputInt("Keepalive (s)", &s_keepalive);

            ImGui::Spacing();
            if (ImGui::Button("Probe Now", ImVec2(100, 25))) {
                auto info = NatTraversalManager::getInstance().probeNow();
                if (info) {
                    Logger::getInstance().info("[GUI] NAT probe: type=" +
                        Config::natTypeToString(info->type) + " " + info->publicIp + ":" + std::to_string(info->publicPort));
                }
            }

            ImGui::Spacing();
            if (NatTraversalManager::getInstance().isEnabled()) {
                auto state = NatTraversalManager::getInstance().getCurrentState();
                ImGui::Text("NAT Type: %s", Config::natTypeToString(state.type).c_str());
                ImGui::Text("Public Address: %s:%u", state.publicIp.c_str(), state.publicPort);
                if (state.type == Config::NatType::SYMMETRIC) {
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "[Warning] Symmetric NAT requires relay mode");
                }
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

static void RenderBenchmarkPanel() {
    auto cfg = Config::ConfigManager::getInstance().getSnapshot();
    static int s_concurrency = 100;
    static int s_duration = 60;
    static int s_protocol_idx = 0;
    static char s_target_host[128] = "127.0.0.1";
    static int s_target_port = 80;
    static int s_connect_rate = 100;
    static int s_mem_interval = 5;
    static double s_progress = 0.0;
    static std::string s_status;
    static bool s_running = false;

    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "Benchmark Engine");
    ImGui::Spacing();

    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Concurrency", &s_concurrency);
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Duration (s)", &s_duration);
    const char* protocols[] = {"HTTP_CONNECT", "SOCKS5_TCP", "SHADOWSOCKS", "VMESS", "VLESS", "TROJAN"};
    ImGui::SetNextItemWidth(150);
    ImGui::Combo("Protocol", &s_protocol_idx, protocols, 6);
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("Target Host", s_target_host, sizeof(s_target_host));
    ImGui::SetNextItemWidth(80);
    ImGui::InputInt("Target Port", &s_target_port);
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Connect Rate", &s_connect_rate);
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Mem Monitor Interval", &s_mem_interval);

    ImGui::Spacing();
    if (!s_running) {
        if (ImGui::Button("Start Benchmark", ImVec2(150, 30))) {
            s_running = true;
            s_progress = 0.0;
            Config::BenchmarkConfig bc;
            bc.concurrency = s_concurrency;
            bc.durationSeconds = s_duration;
            bc.protocol = protocols[s_protocol_idx];
            bc.targetHost = s_target_host;
            bc.targetPort = static_cast<uint16_t>(s_target_port);
            bc.connectRate = s_connect_rate;
            bc.memoryMonitorInterval = s_mem_interval;

            BenchmarkEngine::getInstance().run(bc,
                [](double pct, const std::string& status) {
                    s_progress = pct;
                    s_status = status;
                },
                [](const BenchmarkReport& report) {
                    s_running = false;
                    Logger::getInstance().info("[BENCH] Throughput=" + std::to_string(report.throughputMbps) +
                        " MB/s p99=" + std::to_string(report.p99HandshakeLatencyMs) + "ms");
                },
                [](const std::string& err) {
                    s_running = false;
                    Logger::getInstance().error("[BENCH] Error: " + err);
                });
        }
    } else {
        if (ImGui::Button("Cancel", ImVec2(100, 30))) {
            BenchmarkEngine::getInstance().cancel();
            s_running = false;
        }
        ImGui::SameLine();
        ImGui::ProgressBar(static_cast<float>(s_progress / 100.0), ImVec2(200, 20));
        ImGui::SameLine();
        ImGui::Text("%s", s_status.c_str());
    }
}

static void RenderTlsPanel() {
    auto cfg = Config::ConfigManager::getInstance().getSnapshot();
    static char s_cert_path[256] = {};
    static char s_key_path[256] = {};
    static int s_min_tls = 2;

    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "TLS Certificate");
    ImGui::Spacing();

    bool tlsReady = TlsContext::getInstance().isReady();
    if (tlsReady) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "[TLS Ready]");
    } else {
        ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.3f, 1.0f), "[TLS Not Configured]");
    }
    ImGui::Spacing();

    ImGui::SetNextItemWidth(300);
    ImGui::InputText("Cert Path", s_cert_path, sizeof(s_cert_path));
    ImGui::SetNextItemWidth(300);
    ImGui::InputText("Key Path", s_key_path, sizeof(s_key_path));

    const char* tlsVersions[] = {"TLS 1.0", "TLS 1.1", "TLS 1.2", "TLS 1.3"};
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("Min TLS Version", &s_min_tls, tlsVersions, 4);

    ImGui::Spacing();
    if (ImGui::Button("Load", ImVec2(80, 25))) {
        std::string err;
        if (TlsContext::getInstance().init(s_cert_path, s_key_path,
                                           static_cast<Config::TlsVersion>(s_min_tls), err)) {
            Logger::getInstance().info("[GUI] TLS certificate loaded successfully");
        } else {
            Logger::getInstance().error("[GUI] TLS load failed: " + err);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload", ImVec2(80, 25))) {
        std::string err;
        if (TlsContext::getInstance().reload(err)) {
            Logger::getInstance().info("[GUI] TLS certificate reloaded");
        } else {
            Logger::getInstance().error("[GUI] TLS reload failed: " + err);
        }
    }
}

static void RenderCryptoStatusPanel() {
    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "Crypto Protocol Status");
    ImGui::Spacing();

    auto snap = StatsCollector::getInstance().getSnapshot();
    if (snap.protocolStats.empty()) {
        ImGui::TextDisabled("No crypto protocol activity");
        return;
    }

    if (ImGui::BeginTable("##CryptoStats", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Protocol");
        ImGui::TableSetupColumn("Active Sessions");
        ImGui::TableSetupColumn("Bytes Up");
        ImGui::TableSetupColumn("Bytes Down");
        ImGui::TableHeadersRow();
        for (const auto& [type, stat] : snap.protocolStats) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", protocolTypeToString(type));
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", stat.activeSessions);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%llu", (unsigned long long)stat.bytesUp);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%llu", (unsigned long long)stat.bytesDown);
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "Stability Metrics");
    ImGui::Spacing();
    auto stability = StabilityMetrics::getInstance().getSnapshot();
    ImGui::Text("Connect Failure Rate: %.1f%%", stability.connectFailureRate);
    ImGui::Text("Auth Failure Rate: %.1f%%", stability.authFailureRate);
    ImGui::Text("Abnormal Disconnect Rate: %.1f%%", stability.abnormalDisconnectRate);
    if (stability.alertLevel == "WARNING") {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "[ALERT] %s", stability.alertLevel.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "[OK] %s", stability.alertLevel.c_str());
    }
}

// ===== Shared UI Render =====
static void RenderImGuiUI(int display_w, int display_h) {
    auto& loc = Localization::getInstance();

    // ===== Menu Bar =====
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu(loc.get("LANGUAGE").c_str())) {
            for (const auto& lang : loc.getAvailableLanguages()) {
                if (ImGui::MenuItem(lang.name.c_str(), NULL, loc.getLanguage() == lang.code)) {
                    loc.setLanguage(lang.code);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(loc.get("ABOUT").c_str())) {
            s_show_about = true;
        }
        ImGui::EndMainMenuBar();
    }

    // ===== About Modal =====
    if (s_show_about) {
        ImGui::OpenPopup(loc.get("ABOUT").c_str());
        s_show_about = false;
    }
    if (ImGui::BeginPopupModal(loc.get("ABOUT").c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("HtkisPro v1.1");
        ImGui::Separator();
        ImGui::Text("%s: Hunt", loc.get("DEVELOPER").c_str());
        ImGui::Text("%s: admin@cii.sh.cn", loc.get("EMAIL").c_str());
        ImGui::Text("Build: D3D11 fallback + static CRT");
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ===== Main Window =====
    {
        ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight()));
        ImGui::SetNextWindowSize(ImVec2((float)display_w, (float)display_h - ImGui::GetFrameHeight()));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::Begin("##Main", NULL, flags);

        auto& app = ServerApp::getInstance();

        // ===== Left Panel =====
        float panelWidth = 280.0f;
        ImGui::BeginChild("##LeftPanel", ImVec2(panelWidth, 0), true);
        ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "%s", loc.get("SERVICE_STATUS").c_str());
        ImGui::Spacing();
        StatusIndicator(app.isRunning());
        ImGui::SameLine();
        if (app.isRunning()) {
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", loc.get("STATUS_RUNNING").c_str());
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", loc.get("STATUS_STOPPED").c_str());
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "%s", loc.get("PROXY_CONFIG").c_str());
        ImGui::Spacing();
        if (!app.isRunning()) {
            ImGui::SetNextItemWidth(-1);
            ImGui::InputInt("##port", &s_port);
            if (s_port < 1) s_port = 1;
            if (s_port > 65535) s_port = 65535;
        } else {
            ImGui::Text("%s: %d", loc.get("SERVER_PORT").c_str(), app.getPort());
        }
        ImGui::Spacing();
        if (app.isRunning()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
            if (ImGui::Button(loc.get("STOP_SERVER").c_str(), ImVec2(-1, 36))) app.stop();
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.3f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.35f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.25f, 1.0f));
            if (ImGui::Button(loc.get("START_SERVER").c_str(), ImVec2(-1, 36))) app.start(s_port);
            ImGui::PopStyleColor(3);
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "%s", loc.get("PROTOCOL_INFO").c_str());
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.55f, 0.6f, 0.7f, 1.0f), "HTTP CONNECT");
        ImGui::TextColored(ImVec4(0.55f, 0.6f, 0.7f, 1.0f), "SOCKS5");
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::TextWrapped("%s", loc.get("USAGE_HINT").c_str());
        ImGui::EndChild();

        // ===== Right Panel =====
        ImGui::SameLine();
        ImGui::BeginChild("##RightPanel", ImVec2(0, 0), false);

        if (ImGui::BeginTabBar("##RightTabs", ImGuiTabBarFlags_FittingPolicyResizeDown)) {
            if (ImGui::BeginTabItem("Users")) {
        float rightHeight = ImGui::GetContentRegionAvail().y;
        float userPanelHeight = rightHeight * 0.45f;
        float logPanelHeight = rightHeight - userPanelHeight;

        // ---- User Management ----
        ImGui::BeginChild("##UserPanel", ImVec2(0, userPanelHeight), true);
        ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "%s", loc.get("USER_MANAGEMENT").c_str());
        ImGui::Spacing();
        float formWidth = ImGui::GetContentRegionAvail().x;
        float fieldWidth = (formWidth - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;
        ImGui::SetNextItemWidth(fieldWidth);
        ImGui::InputText(loc.get("USERNAME").c_str(), s_username_buf, IM_ARRAYSIZE(s_username_buf));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(fieldWidth);
        ImGui::InputText(loc.get("PASSWORD").c_str(), s_password_buf, IM_ARRAYSIZE(s_password_buf), ImGuiInputTextFlags_Password);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(fieldWidth);
        ImGui::InputInt(loc.get("DAYS").c_str(), &s_days);
        if (s_days < 1) s_days = 1;
        ImGui::Spacing();
        float btnWidth = 120.0f;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.7f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.82f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.38f, 0.6f, 1.0f));
        if (ImGui::Button(loc.get("ADD_USER").c_str(), ImVec2(btnWidth, 28))) {
            if (strlen(s_username_buf) > 0 && strlen(s_password_buf) > 0) {
                UserManager::getInstance().addUser(s_username_buf, s_password_buf, s_days);
                UserManager::getInstance().save();
                s_username_buf[0] = '\0';
                s_password_buf[0] = '\0';
                s_users = UserManager::getInstance().getAllUsers();
            }
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        if (ImGui::Button(loc.get("REFRESH_LIST").c_str(), ImVec2(btnWidth, 28))) {
            s_users = UserManager::getInstance().getAllUsers();
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        if (ImGui::BeginTable("##Users", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn(loc.get("USERNAME").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.3f);
            ImGui::TableSetupColumn(loc.get("ACTIVE").c_str(), ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn(loc.get("EXPIRE_TIME").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableSetupColumn(loc.get("ACTIONS").c_str(), ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableHeadersRow();
            for (const auto& user : s_users) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", user.first.c_str());
                ImGui::TableSetColumnIndex(1);
                if (user.second.active) {
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "O");
                    ImGui::SameLine();
                    ImGui::Text("%s", loc.get("YES").c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "X");
                    ImGui::SameLine();
                    ImGui::Text("%s", loc.get("NO").c_str());
                }
                ImGui::TableSetColumnIndex(2);
                char timeBuf[64] = {};
                struct tm tm_buf;
#ifdef _WIN32
                localtime_s(&tm_buf, &user.second.expire_time);
#else
                localtime_r(&user.second.expire_time, &tm_buf);
#endif
                strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M", &tm_buf);
                ImGui::Text("%s", timeBuf);
                ImGui::TableSetColumnIndex(3);
                std::string toggleLabel = std::string(user.second.active ? loc.get("DEACTIVATE") : loc.get("ACTIVATE")) + "##" + user.first;
                if (ImGui::SmallButton(toggleLabel.c_str())) {
                    UserManager::getInstance().setStatus(user.first, !user.second.active);
                    UserManager::getInstance().save();
                    s_users = UserManager::getInstance().getAllUsers();
                    break;
                }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.2f, 0.2f, 1.0f));
                std::string delLabel = std::string(loc.get("DELETE").c_str()) + "##" + user.first;
                if (ImGui::SmallButton(delLabel.c_str())) {
                    UserManager::getInstance().removeUser(user.first);
                    UserManager::getInstance().save();
                    s_users = UserManager::getInstance().getAllUsers();
                    break;
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Log")) {

        // ---- Log Panel ----
        ImGui::BeginChild("##LogPanel", ImVec2(0, 0), true);
        ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.85f, 1.0f), "%s", loc.get("LOG_PANEL").c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
        if (ImGui::SmallButton(loc.get("CLEAR").c_str())) {
            Logger::getInstance().clear();
            s_logEntries.clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox(loc.get("AUTO_SCROLL").c_str(), &s_autoScroll);
        ImGui::Separator();
        if (Logger::getInstance().hasNewEntries()) {
            s_logEntries = Logger::getInstance().getEntries();
            Logger::getInstance().markRead();
        }
        ImGui::BeginChild("##LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& entry : s_logEntries) {
            char timeBuf[32] = {};
            struct tm tm_buf;
#ifdef _WIN32
            localtime_s(&tm_buf, &entry.time);
#else
            localtime_r(&entry.time, &tm_buf);
#endif
            strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm_buf);
            ImVec4 color;
            const char* prefix;
            switch (entry.level) {
            case LogLevel::Info:  color = ImVec4(0.6f, 0.8f, 1.0f, 1.0f); prefix = "INFO";  break;
            case LogLevel::Warn:  color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); prefix = "WARN";  break;
            case LogLevel::Error: color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); prefix = "ERR ";  break;
            default:              color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); prefix = "????";  break;
            }
            ImGui::TextDisabled("%s", timeBuf);
            ImGui::SameLine();
            ImGui::TextColored(color, "[%s]", prefix);
            ImGui::SameLine();
            ImGui::Text("%s", entry.message.c_str());
        }
        if (s_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::EndChild();

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Config")) {
                RenderConfigPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Stats")) {
                RenderStatsPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Shadowsocks")) {
                RenderShadowsocksPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("VMess")) {
                RenderVmessPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("VLESS")) {
                RenderVlessPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Trojan")) {
                RenderTrojanPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Relay/NAT")) {
                RenderRelayNatPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Benchmark")) {
                RenderBenchmarkPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("TLS")) {
                RenderTlsPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Crypto Stats")) {
                RenderCryptoStatusPanel();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild(); // RightPanel
        ImGui::End(); // Main
    }
}

// ===== Common Init =====
static void CommonInit() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = NULL;
    ApplyCustomStyle();
    LoadFonts();
    UserManager::getInstance().load("users.json");
}

static void CommonCleanup() {
    ServerApp::getInstance().stop();
    UserManager::getInstance().save();
#ifdef _WIN32
    CleanupTray();
    Gdiplus::GdiplusShutdown(g_gdiplusToken);
#endif
    ImGui::DestroyContext();
}

#ifdef _WIN32
// ===== D3D11 Path =====
static int RunD3D11Path() {
    g_useD3D11 = true;

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    std::wstring exeDir = GetExeDir();
    std::wstring pngPath = exeDir + L"\\HtkisPro.png";
    DWORD attrs = GetFileAttributesW(pngPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) pngPath = L"HtkisPro.png";
    g_hIcon = LoadIconFromPNG(pngPath, 32);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = D3D11WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"HtkisProD3D11";
    wc.hIcon = g_hIcon ? g_hIcon : LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    RECT rc = {0, 0, 960, 720};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_d3d11Hwnd = CreateWindowW(L"HtkisProD3D11", L"HtkisPro", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!g_d3d11Hwnd) return 1;

    ShowWindow(g_d3d11Hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_d3d11Hwnd);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_d3d11Hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fls, 6, D3D11_SDK_VERSION, &sd, &g_swapChain, &g_d3dDevice, nullptr, &g_d3dContext);
    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, fls, 6, D3D11_SDK_VERSION, &sd, &g_swapChain, &g_d3dDevice, nullptr, &g_d3dContext);
        if (FAILED(hr)) return 1;
    }
    CreateRenderTarget();

    InitTray(g_hIcon ? g_hIcon : LoadIcon(NULL, IDI_APPLICATION));

    CommonInit();
    ImGui_ImplWin32_Init(g_d3d11Hwnd);
    ImGui_ImplDX11_Init(g_d3dDevice, g_d3dContext);

    // ===== Main Loop =====
    while (!g_shouldExit) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_shouldExit = true;
        }
        ProcessTrayMessages();
        if (g_shouldExit) break;

        if (!g_d3d11WindowVisible) {
            WaitMessage();
            MSG msg2;
            while (PeekMessage(&msg2, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg2);
                DispatchMessage(&msg2);
                if (msg2.message == WM_QUIT) g_shouldExit = true;
            }
            ProcessTrayMessages();
            continue;
        }

        SetWindowTextW(g_d3d11Hwnd, L"HtkisPro");

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RECT rect;
        GetClientRect(g_d3d11Hwnd, &rect);
        int display_w = rect.right - rect.left;
        int display_h = rect.bottom - rect.top;
        RenderImGuiUI(display_w, display_h);

        ImGui::Render();
        const float clear_color[4] = { 0.10f, 0.11f, 0.14f, 1.00f };
        g_d3dContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_d3dContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swapChain->Present(1, 0);
        Sleep(16);
    }

    // ===== Cleanup =====
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    CommonCleanup();
    CleanupRenderTarget();
    if (g_swapChain) { g_swapChain->Release(); g_swapChain = nullptr; }
    if (g_d3dContext) { g_d3dContext->Release(); g_d3dContext = nullptr; }
    if (g_d3dDevice) { g_d3dDevice->Release(); g_d3dDevice = nullptr; }
    if (g_d3d11Hwnd) { DestroyWindow(g_d3d11Hwnd); g_d3d11Hwnd = nullptr; }
    UnregisterClassW(L"HtkisProD3D11", GetModuleHandleW(NULL));
    return 0;
}
#endif // _WIN32

// ===== GLFW Path =====
static int RunGLFWPath() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(960, 720, "HtkisPro", NULL, NULL);
    if (!window) return 1;
    g_window = window;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetWindowCloseCallback(window, window_close_callback);

#ifdef _WIN32
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    std::wstring exeDir = GetExeDir();
    std::wstring pngPath = exeDir + L"\\HtkisPro.png";
    DWORD attrs = GetFileAttributesW(pngPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) pngPath = L"HtkisPro.png";
    SetGLFWWindowIcon(pngPath, window);
    g_hIcon = LoadIconFromPNG(pngPath, 32);
    InitTray(g_hIcon ? g_hIcon : LoadIcon(NULL, IDI_APPLICATION));
#endif

    CommonInit();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // ===== Main Loop =====
    while (!g_shouldExit) {
        glfwWaitEventsTimeout(1.0 / 60.0);
#ifdef _WIN32
        ProcessTrayMessages();
#endif
        if (g_shouldExit) break;

        if (!glfwGetWindowAttrib(window, GLFW_VISIBLE)) {
#ifdef _WIN32
            Sleep(50);
#else
            usleep(50000);
#endif
            continue;
        }

        glfwSetWindowTitle(window, "HtkisPro");
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        RenderImGuiUI(display_w, display_h);

        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.11f, 0.14f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // ===== Cleanup =====
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    CommonCleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

int main(int, char**) {
#ifdef _WIN32
    CrashHandler::getInstance().init();
    Config::ConfigManager::getInstance().loadFromFile("config.json");
    // Try GLFW/OpenGL first
    int result = RunGLFWPath();
    if (result == 0 || g_shouldExit) return result;
    // Fallback to D3D11
    g_shouldExit = false;
    return RunD3D11Path();
#else
    return RunGLFWPath();
#endif
}
