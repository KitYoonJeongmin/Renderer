#include "GridMesh.h"
#include <cassert>

#ifndef HR
#define HR(x) do { HRESULT __hr=(x); assert(SUCCEEDED(__hr)); } while(0)
#endif

bool GridMesh::Init(ID3D11Device* d, int rows, int cols, float sizeX, float sizeZ)
{
    const int vx = cols;  // x 방향 정점 수
    const int vz = rows;  // z 방향 정점 수
    if (vx < 2 || vz < 2) return false;

    std::vector<VertexPNT> verts(vx * vz);
    std::vector<uint32_t>  inds;
    inds.reserve((vx - 1) * (vz - 1) * 6);

    const float halfX = sizeX * 0.5f;
    const float halfZ = sizeZ * 0.5f;

    for (int z = 0; z < vz; ++z) {
        for (int x = 0; x < vx; ++x) {
            float u = (float)x / (vx - 1);
            float v = (float)z / (vz - 1);
            float px = -halfX + u * sizeX;
            float pz = -halfZ + v * sizeZ;
            verts[z * vx + x] = { {px, 0.0f, pz}, {0,1,0}, {u, v} };
        }
    }

    for (int z = 0; z < vz - 1; ++z) {
        for (int x = 0; x < vx - 1; ++x) {
            uint32_t i0 = z * vx + x;
            uint32_t i1 = z * vx + (x + 1);
            uint32_t i2 = (z + 1) * vx + x;
            uint32_t i3 = (z + 1) * vx + (x + 1);
            // 두 삼각형
            inds.push_back(i0); inds.push_back(i2); inds.push_back(i1);
            inds.push_back(i1); inds.push_back(i2); inds.push_back(i3);
        }
    }
    mIndexCount = (UINT)inds.size();

    // VB
    D3D11_BUFFER_DESC vbd{};
    vbd.ByteWidth = (UINT)(verts.size() * sizeof(VertexPNT));
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vinit{ verts.data() };
    HR(d->CreateBuffer(&vbd, &vinit, mVB.GetAddressOf()));

    // IB
    D3D11_BUFFER_DESC ibd{};
    ibd.ByteWidth = (UINT)(inds.size() * sizeof(uint32_t));
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iinit{ inds.data() };
    HR(d->CreateBuffer(&ibd, &iinit, mIB.GetAddressOf()));

    return true;
}

void GridMesh::Bind(ID3D11DeviceContext* c) const
{
    UINT stride = sizeof(VertexPNT), offset = 0;
    ID3D11Buffer* vb = mVB.Get();
    c->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    c->IASetIndexBuffer(mIB.Get(), DXGI_FORMAT_R32_UINT, 0);
    c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
