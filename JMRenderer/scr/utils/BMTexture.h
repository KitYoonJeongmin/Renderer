#pragma once
#include <wrl/client.h>
#include <d3d11.h>

namespace BMP {

    // 24/32-bit BMP �� RGBA8_UNORM �ؽ�ó SRV
    // outW/outH�� ����(�ʿ� ������ nullptr)
    bool LoadRGBA8(
        ID3D11Device* dev,
        const wchar_t* path,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV,
        unsigned* outW = nullptr,
        unsigned* outH = nullptr);

    // 24-bit BMP �� R8_UNORM �ؽ�ó SRV (���̸�)
    // RGB�� ����ġ�� �׷��� ��ȯ(0.299, 0.587, 0.114)
    bool LoadR8(
        ID3D11Device* dev,
        const wchar_t* path,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV,
        unsigned* outW = nullptr,
        unsigned* outH = nullptr);

} // namespace BMP
