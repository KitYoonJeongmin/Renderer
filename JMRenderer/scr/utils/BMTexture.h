#pragma once
#include <wrl/client.h>
#include <d3d11.h>

namespace BMP {

    // 24/32-bit BMP → RGBA8_UNORM 텍스처 SRV
    // outW/outH는 선택(필요 없으면 nullptr)
    bool LoadRGBA8(
        ID3D11Device* dev,
        const wchar_t* path,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV,
        unsigned* outW = nullptr,
        unsigned* outH = nullptr);

    // 24-bit BMP → R8_UNORM 텍스처 SRV (높이맵)
    // RGB를 가중치로 그레이 변환(0.299, 0.587, 0.114)
    bool LoadR8(
        ID3D11Device* dev,
        const wchar_t* path,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV,
        unsigned* outW = nullptr,
        unsigned* outH = nullptr);

} // namespace BMP
