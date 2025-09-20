#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// 포맷: POSITION, NORMAL, TEXCOORD
struct VertexPNT {
    XMFLOAT3 pos;
    XMFLOAT3 nrm;
    XMFLOAT2 uv;
};

class GridMesh {
public:
    // rows x cols 정점 (기본 64x64), 월드 XZ 평면에 sizeX x sizeZ 크기
    bool Init(ID3D11Device* d, int rows = 64, int cols = 64,
        float sizeX = 10.0f, float sizeZ = 10.0f);

    void Bind(ID3D11DeviceContext* c) const;
    UINT IndexCount() const { return mIndexCount; }

    // 셰이더 입력 레이아웃(IA) 기술 서술자 생성 헬퍼
    static void BuildInputLayoutDesc(std::vector<D3D11_INPUT_ELEMENT_DESC>& out) {
        out = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
              (UINT)offsetof(VertexPNT, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
              (UINT)offsetof(VertexPNT, nrm), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,
              (UINT)offsetof(VertexPNT, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
    }

private:
    ComPtr<ID3D11Buffer> mVB;
    ComPtr<ID3D11Buffer> mIB;
    UINT mIndexCount = 0;
};
