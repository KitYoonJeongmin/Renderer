#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>
#include <filesystem>
#include <vector>
#include <cassert>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")


// ImGui
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using Microsoft::WRL::ComPtr;
static HINSTANCE GInstance = nullptr;
static HWND      GWnd = nullptr;
static UINT      GWidth = 1600, GHeight = 900;

static void HR(HRESULT hr) { assert(SUCCEEDED(hr)); }
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/*
 * 장치/스왑체인/백버퍼 관리자
 * 
 * Initialize
 *  * D3D11CreateDeviceAndSwapChain 호출로 Device/Context/SwapChain 생성. 
 *  * 끝나면 CreateBackbuffer(w,h)로 RTV/DSV/뷰포트 준비.
 * 
 * CreateBackbuffer
 * ** 스왑체인에서 0번 버퍼를 꺼내 RTV 생성.
 * ** 깊이 텍스처(D24S8) + DSV 생성.
 * ** mVP(뷰포트) 폭·높이 업데이트.
 * 
 * Resize
 * ** RTV/DSV/Depth 텍스처 **Reset()**로 해제 → ResizeBuffers() → 다시 CreateBackbuffer
 * ** 리사이즈 시 리소스 리크가 가장 흔한 버그인데, 여기서 안전하게 해결.
 * 
 * Begin/EndFrame
 * ** OM(출력 병합 단계)에 RTV/DSV 바인딩, RSSetViewports.
 * ** 화면/깊이버퍼 Clear.
 * ** Present(vsync)로 교체.
 */
// ---------------- Device ----------------
class DeviceResources {
public:
    void Initialize(HWND hwnd, UINT w, UINT h) {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = w; sd.BufferDesc.Height = h;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd; sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT flags = 0;
#if defined(_DEBUG)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        D3D_FEATURE_LEVEL fl;
        HR(D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            nullptr, 0, D3D11_SDK_VERSION, &sd,
            mSwapChain.GetAddressOf(), mDevice.GetAddressOf(), &fl,
            mCtx.GetAddressOf()));

        CreateBackbuffer(w, h);
    }

    void Resize(UINT w, UINT h) {
        if (!mSwapChain) return;
        mRTV.Reset(); mDSV.Reset(); mDepth.Reset();
        HR(mSwapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0));
        CreateBackbuffer(w, h);
    }

    void BeginFrame(const float c[4]) {
        mCtx->OMSetRenderTargets(1, mRTV.GetAddressOf(), mDSV.Get());
        mCtx->RSSetViewports(1, &mVP);
        mCtx->ClearRenderTargetView(mRTV.Get(), c);
        mCtx->ClearDepthStencilView(mDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    }
    void EndFrame(bool vsync) { mSwapChain->Present(vsync ? 1 : 0, 0); }

    ID3D11Device* Dev() { return mDevice.Get(); }
    ID3D11DeviceContext* Ctx() { return mCtx.Get(); }

private:
    void CreateBackbuffer(UINT w, UINT h) {
        ComPtr<ID3D11Texture2D> bb;
        HR(mSwapChain->GetBuffer(0, IID_PPV_ARGS(bb.GetAddressOf())));
        HR(mDevice->CreateRenderTargetView(bb.Get(), nullptr, mRTV.GetAddressOf()));

        D3D11_TEXTURE2D_DESC dsd{};
        dsd.Width = w; dsd.Height = h; dsd.MipLevels = 1; dsd.ArraySize = 1;
        dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsd.SampleDesc.Count = 1; dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        HR(mDevice->CreateTexture2D(&dsd, nullptr, mDepth.GetAddressOf()));
        HR(mDevice->CreateDepthStencilView(mDepth.Get(), nullptr, mDSV.GetAddressOf()));

        mVP = { 0, 0, (float)w, (float)h, 0, 1 };
    }
    ComPtr<ID3D11Device> mDevice;
    ComPtr<ID3D11DeviceContext> mCtx;
    ComPtr<IDXGISwapChain> mSwapChain;
    ComPtr<ID3D11RenderTargetView> mRTV;
    ComPtr<ID3D11Texture2D> mDepth;
    ComPtr<ID3D11DepthStencilView> mDSV;
    D3D11_VIEWPORT mVP{};
};

/*
 * Init
 * ** 장치/경로/입력 레이아웃 정의를 저장.
 * ** 즉시 CompileAll() 호출로 VS/PS 컴파일 + 생성.
 * 
 * CompileAll
 * ** D3DCompileFromFile(path, ..., "VSMain","vs_5_0")로 컴파일 → CreateVertexShader
 * ** 주의: VS 컴파일 결과 blob이 InputLayout 생성에도 필요.
 * ** PS도 동일하게 "PSMain","ps_5_0"
 * 
 * TryHotReload
 * ** std::filesystem::last_write_time으로 파일 변경 감지.
 * ** 변하면 CompileAll() 다시 호출 → 런타임 재컴파일(핫리로드).
 * 
 * Bind
 * ** IASetInputLayout, VSSetShader, PSSetShader 바인딩만 담당.
 */

// ---------------- Shader ----------------
class ShaderProgram {
public:
    void Init(ID3D11Device* dev,
        const std::wstring& vsPath,
        const std::wstring& psPath,
        const D3D11_INPUT_ELEMENT_DESC* layout, UINT n) {
        mDev = dev; mVSPath = vsPath; mPSPath = psPath;
        mLayout.assign(layout, layout + n);
        CompileAll();
    }
    void TryHotReload() {
        namespace fs = std::filesystem;
        auto changed = [&](const std::wstring& p, auto& t) {
            if (!fs::exists(p)) return false;
            auto nt = fs::last_write_time(p);
            if (t != fs::file_time_type{} && nt != t) { t = nt; return true; }
            t = nt; return false;
            };
        if (changed(mVSPath, mVSTime) || changed(mPSPath, mPSTime)) CompileAll();
    }
    void Bind(ID3D11DeviceContext* ctx) {
        ctx->IASetInputLayout(mIL.Get());
        ctx->VSSetShader(mVS.Get(), nullptr, 0);
        ctx->PSSetShader(mPS.Get(), nullptr, 0);
    }
private:
    void CompileAll() {
        UINT f = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
        f |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ComPtr<ID3DBlob> vsb, psb;
        HR(D3DCompileFromFile(mVSPath.c_str(), nullptr, nullptr,
            "VSMain", "vs_5_0", f, 0, vsb.GetAddressOf(), nullptr));
        HR(mDev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, mVS.GetAddressOf()));
        HR(mDev->CreateInputLayout(mLayout.data(), (UINT)mLayout.size(),
            vsb->GetBufferPointer(), vsb->GetBufferSize(), mIL.GetAddressOf()));

        HR(D3DCompileFromFile(mPSPath.c_str(), nullptr, nullptr,
            "PSMain", "ps_5_0", f, 0, psb.GetAddressOf(), nullptr));
        HR(mDev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, mPS.GetAddressOf()));
    }
    ID3D11Device* mDev{};
    std::wstring mVSPath, mPSPath;
    std::filesystem::file_time_type mVSTime{}, mPSTime{};
    std::vector<D3D11_INPUT_ELEMENT_DESC> mLayout;
    ComPtr<ID3D11InputLayout> mIL;
    ComPtr<ID3D11VertexShader> mVS;
    ComPtr<ID3D11PixelShader>  mPS;
};

// ---------------- Triangle ----------------
struct Vertex { float pos[3]; float col[3]; };
struct Triangle {
    ComPtr<ID3D11Buffer> VB;
    void Init(ID3D11Device* d) {
        Vertex v[3] = {
            {{0,0.5f,0},{1,0,0}}, {{0.5f,-0.5f,0},{0,1,0}}, {{-0.5f,-0.5f,0},{0,0,1}}
        };
        D3D11_BUFFER_DESC bd{ sizeof(v),D3D11_USAGE_DEFAULT,D3D11_BIND_VERTEX_BUFFER };
        D3D11_SUBRESOURCE_DATA init{ v };
        HR(d->CreateBuffer(&bd, &init, VB.GetAddressOf()));
    }
    void Bind(ID3D11DeviceContext* c) {
        UINT s = sizeof(Vertex), o = 0;
        ID3D11Buffer* b = VB.Get();
        c->IASetVertexBuffers(0, 1, &b, &s, &o);
        c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }
};

// ---------------- Globals ----------------
static DeviceResources GDev;
static ShaderProgram   GShader;
static Triangle        GTri;
static float           GClear[4] = { 0.1f,0.1f,0.1f,1 };
static bool            GVsync = true;

// ---------------- Init/Loop ----------------
static void InitAll() {
    GDev.Initialize(GWnd, GWidth, GHeight);
    D3D11_INPUT_ELEMENT_DESC l[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0} };
    GShader.Init(GDev.Dev(), L"assets/shaders/simple_vs.hlsl", L"assets/shaders/simple_ps.hlsl", l, 2);
    GTri.Init(GDev.Dev());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(GWnd);
    ImGui_ImplDX11_Init(GDev.Dev(), GDev.Ctx());
}
static void ShutdownAll() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
static void RenderFrame() {
    GShader.TryHotReload();
    GDev.BeginFrame(GClear);
    auto* c = GDev.Ctx();
    GShader.Bind(c); GTri.Bind(c);
    c->Draw(3, 0);

    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
    ImGui::Begin("HUD");
    ImGui::ColorEdit3("Clear", GClear);
    ImGui::Checkbox("VSync", &GVsync);
    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    GDev.EndFrame(GVsync);
}

// ---------------- WinMain ----------------
int APIENTRY wWinMain(HINSTANCE h, HINSTANCE, LPWSTR, int n) {
    GInstance = h;
    WNDCLASSEX wc{ sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WndProc; 
    wc.hInstance = h; 
    wc.lpszClassName = L"MyWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassEx(&wc);
    RECT rc{ 0,0,(LONG)GWidth,(LONG)GHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    GWnd = CreateWindowW(wc.lpszClassName, L"M0 Renderer", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, h, nullptr);

    InitAll();
    MSG msg{}; bool run = true;
    while (run) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { run = false; break; }
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        if (!run) break;
        RenderFrame();
    }
    ShutdownAll();
    return 0;
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, m, w, l))
    {
        return true;
    }

    switch (m) {
    case WM_SIZE:
        if (w != SIZE_MINIMIZED) 
        { 
            GWidth = LOWORD(l); 
            GHeight = HIWORD(l); 
            GDev.Resize(GWidth, GHeight); 
        }
        return 0;
    case WM_DESTROY: 
        PostQuitMessage(0); 
        return 0;
    }
    return DefWindowProc(hWnd, m, w, l);
}
