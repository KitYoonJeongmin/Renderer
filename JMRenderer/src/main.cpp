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
#include <cmath>

#include "grid/GridMesh.h"
#include "utils/camera/Camera.h"
#include "utils/BMTexture.h"

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

//struct SceneCB {
//    DirectX::XMFLOAT4X4 WVP;     // World*View*Proj (Transpose 해서 보냄)
//    DirectX::XMFLOAT3   LightDir;// 정규화된 방향(예: -0.5, -1, -0.3)
//    float _pad0 = 0.0f;        // 16바이트 정렬
//};

/// <summary>
/// 씬
/// </summary>
struct SceneCB {
    DirectX::XMMATRIX WVP; // World*View*Proj (Transpose 해서 보냄)

    // 정규화된 방향(예: -0.5, -1, -0.3)
    DirectX::XMFLOAT3   LightDir;  
    float _pad0 = 0.0f; // 16바이트 정렬

    DirectX::XMFLOAT3   FogColor;  
    float FogDensity = 0.06f;


    DirectX::XMFLOAT3   CamPos;    
    float _pad1 = 0.0f;
};

static ComPtr<ID3D11Buffer> GSceneCB;

/// <summary>
/// 와이어 프레임
/// </summary>
static bool GWireframe = false;
static ComPtr<ID3D11RasterizerState> GRS_Solid;
static ComPtr<ID3D11RasterizerState> GRS_Wire;

// 알베도 텍스처 3장
static ComPtr<ID3D11ShaderResourceView> GTexGrass, GTexRock, GTexSnow;
static ComPtr<ID3D11SamplerState>       GAlbedoSamp;

// 스플랫 임계값(필요시 ImGui에서 조정)
static float GH_GrassMax = 0.35f;   // 이 높이까지는 잔디 가중치↑
static float GH_SnowMin = 0.75f;   // 이 높이부터는 눈 가중치↑
static float GS_SlopeLo = 0.45f;   // 이 경사부터 바위 가중치 시작
static float GS_SlopeHi = 0.85f;   // 이 경사에서 바위 가중치=1
static float GUvScale = 8.0f;    // 타일링
static float GBlendBand = 0.08f;   // 높이 전이 부드러움


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

// ── Heightmap 리소스 ───────────────────────────────────────────
static ComPtr<ID3D11ShaderResourceView> GHeightSRV;
static ComPtr<ID3D11SamplerState>       GHeightSamp;

// ── Terrain 상수버퍼(b1) ──────────────────────────────────────
struct TerrainCBCPU {
    float HeightScale; 
    float padA[3];     // 16바이트 정렬
    DirectX::XMFLOAT2 GridSize;           // (worldX, worldZ)
    DirectX::XMFLOAT2 TexelSize;          // (1/texW, 1/texH)
};
static ComPtr<ID3D11Buffer> GTerrainCB;

// ── Mat 상수버퍼(b2) ──────────────────────────────────────
struct MatCBCPU 
{ 
    DirectX::XMFLOAT4 thresholds; 
    float band; 
    float _pad[3]; 
    float uvScale; 
    float _pad2[3]; 
};
static ComPtr<ID3D11Buffer> GMatCB;
static UINT GhmW = 0, GhmH = 0;

// ── UI/그리드 파라미터 (그리드 생성에 쓰는 값과 일치) ─────────
static int   GGridRows = 64, GGridCols = 64;
static float GGridSizeX = 10.0f, GGridSizeZ = 10.0f;
static float GHeightScale = 1.5f;   // ImGui에서 조절할 값
static DirectX::XMFLOAT3 GFogColor = { 0.6f, 0.7f, 0.8f };
static float             GFogDensity = 0.06f;

// ---------------- Globals ----------------
static DeviceResources GDev;
static ShaderProgram   GShader;
static GridMesh        GGrid;
static CameraFPS GCam;
static float           GClear[4] = { 0.1f,0.1f,0.1f,1 };
static bool            GVsync = true;

// ---------------- Init/Loop ----------------
static void InitAll() {
    GDev.Initialize(GWnd, GWidth, GHeight);

    /*
     * BuildInputLayoutDesc 생성
     * GridMesh로 전달
     */
    std::vector<D3D11_INPUT_ELEMENT_DESC> layout;
    GridMesh::BuildInputLayoutDesc(layout);

    /*
    * tODO: 파일 Path를 한 파이렝 모으도록 개선
    */
    GShader.Init(
        GDev.Dev(),
        L"assets/shaders/grid/terrain_vs.hlsl",   // ← 테레인용 VS
        L"assets/shaders/grid/terrain_ps.hlsl",   // ← 테레인용 PS
        layout.data(),
        (UINT)layout.size()
    );

    // 카메라 세팅
    GCam.SetPosition({ 0.f, 2.f, -5.f });
    GCam.SetMoveSpeed(8.f);
    GCam.SetTurnSpeed(2.0f);

    D3D11_BUFFER_DESC cbd{};
    UINT sz = (UINT)sizeof(SceneCB);
    cbd.ByteWidth = (sz + 15u) & ~15u; // 16바이트 정렬
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HR(GDev.Dev()->CreateBuffer(&cbd, nullptr, GSceneCB.GetAddressOf()));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(GWnd);
    ImGui_ImplDX11_Init(GDev.Dev(), GDev.Ctx());

    //////////////////////////////////////
    ////////// Heightmap ////////////////
    // ── (A) CPU에서 256×256 높이맵 생성 (sin/cos 패턴) ─────────────
    const UINT texW = 256, texH = 256;
    std::vector<uint8_t> hm(texW * texH);
    for (UINT y = 0; y < texH; ++y) {
        for (UINT x = 0; x < texW; ++x) {
            float u = (float)x / (texW - 1);
            float v = (float)y / (texH - 1);
            float h = 0.5f + 0.5f * (sinf(u * 8.0f) * cosf(v * 6.0f)); // 0~1
            hm[y * texW + x] = (uint8_t)std::round(h * 255.0f);
        }
    }

    // ── (B) Texture2D + SRV ───────────────────────────────────────
    D3D11_TEXTURE2D_DESC td{};
    td.Width = texW; td.Height = texH; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8_UNORM;
    td.SampleDesc.Count = 1;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA srd{};
    srd.pSysMem = hm.data();
    srd.SysMemPitch = texW * sizeof(uint8_t);

    ComPtr<ID3D11Texture2D> tex;
    HR(GDev.Dev()->CreateTexture2D(&td, &srd, tex.GetAddressOf()));

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MostDetailedMip = 0;
    srvd.Texture2D.MipLevels = 1;
    HR(GDev.Dev()->CreateShaderResourceView(tex.Get(), &srvd, GHeightSRV.GetAddressOf()));

    // ── (C) SamplerState ──────────────────────────────────────────
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    HR(GDev.Dev()->CreateSamplerState(&sd, GHeightSamp.GetAddressOf()));

    // ── (D) Terrain CB(b1) 생성 ───────────────────────────────────
    //D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth = ((UINT)sizeof(TerrainCBCPU) + 15u) & ~15u;
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HR(GDev.Dev()->CreateBuffer(&cbd, nullptr, GTerrainCB.GetAddressOf())); 

    // 그리드 생성
    GGrid.Init(GDev.Dev(), GGridRows, GGridCols, GGridSizeX, GGridSizeZ);

    // 와이어프레임
    D3D11_RASTERIZER_DESC rs{}; 
    rs.FillMode = D3D11_FILL_SOLID; 
    rs.CullMode = D3D11_CULL_BACK; 
    rs.DepthClipEnable = TRUE;
    HR(GDev.Dev()->CreateRasterizerState(&rs, GRS_Solid.GetAddressOf()));
    
    rs.FillMode = D3D11_FILL_WIREFRAME;
    HR(GDev.Dev()->CreateRasterizerState(&rs, GRS_Wire.GetAddressOf()));

    // 텍스쳐

    D3D11_BUFFER_DESC bd{}; 
    bd.ByteWidth = ((UINT)sizeof(MatCBCPU) + 15) & ~15; 
    bd.Usage = D3D11_USAGE_DYNAMIC; 
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; 
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HR(GDev.Dev()->CreateBuffer(&bd, nullptr, GMatCB.GetAddressOf()));

    
    BMP::LoadR8(GDev.Dev(), L"assets\\heightmaps\\hm.bmp", GHeightSRV, &GhmW, &GhmH);
    
    BMP::LoadRGBA8(GDev.Dev(), L"assets\\textures\\grass.bmp", GTexGrass);
    BMP::LoadRGBA8(GDev.Dev(), L"assets\\textures\\rock.bmp", GTexRock);
    BMP::LoadRGBA8(GDev.Dev(), L"assets\\textures\\snow.bmp", GTexSnow);

    D3D11_SAMPLER_DESC asd{};
    asd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    asd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    asd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    asd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    asd.MaxLOD = D3D11_FLOAT32_MAX;
    HR(GDev.Dev()->CreateSamplerState(&asd, GAlbedoSamp.GetAddressOf()));
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
    ID3D11Buffer* cb = GSceneCB.Get();

    // dt 계산 (고정 프레임이 아니면)
    static LARGE_INTEGER freq{}, prev{};
    if (freq.QuadPart == 0) { QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&prev); }
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    float dt = float(now.QuadPart - prev.QuadPart) / float(freq.QuadPart);
    prev = now;

    // 카메라 업데이트
    GCam.UpdateFromKeyboardWin32(dt);

    // 행렬 계산
    float aspect = (float)GWidth / (float)GHeight;
    DirectX::XMMATRIX World = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX View = GCam.View();
    DirectX::XMMATRIX Proj = GCam.Proj(aspect);

    // 상수버퍼 업로드
    
    SceneCB scb{};
    scb.WVP = DirectX::XMMatrixTranspose(World * View * Proj);
    scb.LightDir = { -0.4f, -1.0f, -0.3f };
    scb.FogColor = GFogColor;
    scb.FogDensity = GFogDensity;
    scb.CamPos = GCam.Position();

    D3D11_MAPPED_SUBRESOURCE m{};
    HR(c->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m));
    memcpy(m.pData, &scb, sizeof(SceneCB));
    c->Unmap(cb, 0);

    // ── Terrain CB(b1) 채우기 ───────────────────────────────
    TerrainCBCPU tcb{};
    tcb.HeightScale = GHeightScale;
    tcb.GridSize = { GGridSizeX, GGridSizeZ };
    //tcb.TexelSize = { 1.0f / 256.0f, 1.0f / 256.0f }; // 우리가 만든 높이맵 크기
    tcb.TexelSize = { 1.0f / float(GhmW), 1.0f / float(GhmH) };

    D3D11_MAPPED_SUBRESOURCE mt{};
    HR(c->Map(GTerrainCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mt));
    memcpy(mt.pData, &tcb, sizeof(TerrainCBCPU));
    c->Unmap(GTerrainCB.Get(), 0);

    // VS/PS에 바인딩
    ID3D11Buffer* cb1 = GTerrainCB.Get();
    c->VSSetConstantBuffers(1, 1, &cb1);
    c->PSSetConstantBuffers(1, 1, &cb1);
    
    // Heightmap 리소스를 바인딩 
    ID3D11ShaderResourceView* srv = GHeightSRV.Get();
    ID3D11SamplerState* s = GHeightSamp.Get();
    c->VSSetShaderResources(0, 1, &srv);
    c->VSSetSamplers(0, 1, &s);

    MatCBCPU mat{};
    mat.thresholds = { GH_GrassMax, GH_SnowMin, GS_SlopeLo, GS_SlopeHi };
    mat.band = GBlendBand;
    mat.uvScale = GUvScale;

    D3D11_MAPPED_SUBRESOURCE mat_map{};
    HR(c->Map(GMatCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mat_map));
    memcpy(mat_map.pData, &mat, sizeof(mat));
    c->Unmap(GMatCB.Get(), 0);

    ID3D11Buffer* b2 = GMatCB.Get();
    c->PSSetConstantBuffers(2, 1, &b2);


    c->RSSetState(GWireframe ? GRS_Wire.Get() : GRS_Solid.Get());


    GShader.Bind(c);
    GGrid.Bind(c);
    c->DrawIndexed(GGrid.IndexCount(), 0, 0);
    c->VSSetConstantBuffers(0, 1, &cb);
    c->PSSetConstantBuffers(0, 1, &cb);

    ID3D11ShaderResourceView* srvs[3] = { GTexGrass.Get(), GTexRock.Get(), GTexSnow.Get() };
    ID3D11SamplerState* samp = GAlbedoSamp.Get();
    c->PSSetShaderResources(0, 3, srvs); // t0,t1,t2
    c->PSSetSamplers(0, 1, &samp);       // s0


    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
    ImGui::Begin("HUD");
    ImGui::ColorEdit3("Clear", GClear);
    ImGui::Checkbox("VSync", &GVsync);
    ImGui::SliderFloat("HeightScale", &GHeightScale, 0.0f, 10.0f, "%.2f");

    ImGui::SliderFloat("UV Scale", &GUvScale, 1.0f, 32.0f, "%.1f");
    ImGui::SliderFloat("H GrassMax", &GH_GrassMax, 0.0f, 1.0f);
    ImGui::SliderFloat("H SnowMin", &GH_SnowMin, 0.0f, 1.0f);
    ImGui::SliderFloat("Slope Lo", &GS_SlopeLo, 0.0f, 1.0f);
    ImGui::SliderFloat("Slope Hi", &GS_SlopeHi, 0.0f, 1.0f);
    ImGui::SliderFloat("Blend Band", &GBlendBand, 0.01f, 0.2f, "%.2f");

    ImGui::ColorEdit3("FogColor", &GFogColor.x);
    ImGui::SliderFloat("FogDensity", &GFogDensity, 0.0f, 0.2f, "%.3f");

    ImGui::Checkbox("Wireframe", &GWireframe);

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
    /*
    * 우선순위
    * 1. ImGui
    * 2. GCam
    */
    if (ImGui_ImplWin32_WndProcHandler(hWnd, m, w, l))
    {
        return true;
    }
    if (ImGui::GetCurrentContext() != nullptr && GCam.HandleWin32MouseMsg(hWnd, m, w, l, ImGui::GetIO().WantCaptureMouse))
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
