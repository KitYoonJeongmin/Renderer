#include "BMTexture.h"
#include <vector>
#include <cstdio>
#include <cassert>

using Microsoft::WRL::ComPtr;

#pragma pack(push,1)
struct BmpFileHeader {
    unsigned short bfType;      // 'BM' = 0x4D42
    unsigned int   bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int   bfOffBits;
};
struct BmpInfoHeader {
    unsigned int   biSize;      // 40 (BITMAPINFOHEADER)
    int            biWidth;
    int            biHeight;    // >0: bottom-up, <0: top-down
    unsigned short biPlanes;    // 1
    unsigned short biBitCount;  // 24 또는 32 지원
    unsigned int   biCompression; // 0(BI_RGB)만 지원
    unsigned int   biSizeImage;
    int            biXPelsPerMeter;
    int            biYPelsPerMeter;
    unsigned int   biClrUsed;
    unsigned int   biClrImportant;
};
#pragma pack(pop)

static inline unsigned char toGray(unsigned char r, unsigned char g, unsigned char b) {
    float y = 0.299f * r + 0.587f * g + 0.114f * b;
    int v = (int)(y + 0.5f);
    if (v < 0) v = 0; if (v > 255) v = 255;
    return (unsigned char)v;
}

static bool ReadHeaders(FILE* fp, BmpFileHeader& fh, BmpInfoHeader& ih) {
    if (std::fread(&fh, sizeof(fh), 1, fp) != 1) return false;
    if (std::fread(&ih, sizeof(ih), 1, fp) != 1) return false;
    if (fh.bfType != 0x4D42) return false;
    if (ih.biSize < 40 || ih.biPlanes != 1) return false;
    if (ih.biCompression != 0) return false; // BI_RGB만
    if (ih.biWidth <= 0 || ih.biHeight == 0) return false;
    return true;
}

namespace BMP {

    bool LoadRGBA8(ID3D11Device* dev, const wchar_t* path,
        ComPtr<ID3D11ShaderResourceView>& outSRV,
        unsigned* outW, unsigned* outH)
    {
        outSRV.Reset();
        if (outW) *outW = 0; if (outH) *outH = 0;

        FILE* fp = nullptr;
#ifdef _WIN32
        _wfopen_s(&fp, path, L"rb");
#else
        fp = std::fopen(/*convert path*/, "rb");
#endif
        if (!fp) return false;

        BmpFileHeader fh{};
        BmpInfoHeader ih{};
        bool ok = ReadHeaders(fp, fh, ih);
        if (!ok) { std::fclose(fp); return false; }

        const unsigned W = (unsigned)ih.biWidth;
        const unsigned H = (unsigned)(ih.biHeight > 0 ? ih.biHeight : -ih.biHeight);
        const bool bottomUp = (ih.biHeight > 0);
        const unsigned bc = ih.biBitCount;

        if (!(bc == 24 || bc == 32)) { std::fclose(fp); return false; }

        std::fseek(fp, (long)fh.bfOffBits, SEEK_SET);

        std::vector<unsigned char> dataRGBA(W * H * 4);

        if (bc == 24) {
            const unsigned srcStride = ((W * 3u) + 3u) & ~3u; // 4바이트 패딩
            std::vector<unsigned char> row(srcStride);
            for (unsigned y = 0; y < H; ++y) {
                if (std::fread(row.data(), 1, srcStride, fp) != srcStride) { std::fclose(fp); return false; }
                unsigned dy = bottomUp ? (H - 1 - y) : y;
                unsigned char* dst = &dataRGBA[dy * W * 4];
                for (unsigned x = 0; x < W; ++x) {
                    unsigned char B = row[x * 3 + 0];
                    unsigned char G = row[x * 3 + 1];
                    unsigned char R = row[x * 3 + 2];
                    dst[x * 4 + 0] = R;
                    dst[x * 4 + 1] = G;
                    dst[x * 4 + 2] = B;
                    dst[x * 4 + 3] = 255; // 불투명
                }
            }
        }
        else { // 32-bit BGRA
            const unsigned srcStride = W * 4u;
            std::vector<unsigned char> row(srcStride);
            for (unsigned y = 0; y < H; ++y) {
                if (std::fread(row.data(), 1, srcStride, fp) != srcStride) { std::fclose(fp); return false; }
                unsigned dy = bottomUp ? (H - 1 - y) : y;
                unsigned char* dst = &dataRGBA[dy * W * 4];
                for (unsigned x = 0; x < W; ++x) {
                    unsigned char B = row[x * 4 + 0];
                    unsigned char G = row[x * 4 + 1];
                    unsigned char R = row[x * 4 + 2];
                    unsigned char A = row[x * 4 + 3];
                    dst[x * 4 + 0] = R;
                    dst[x * 4 + 1] = G;
                    dst[x * 4 + 2] = B;
                    dst[x * 4 + 3] = A; // 그대로 사용(대부분 0x00 또는 0xFF)
                }
            }
        }

        std::fclose(fp);

        // D3D 텍스처/ SRV 생성
        D3D11_TEXTURE2D_DESC td{};
        td.Width = W; td.Height = H; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA srd{};
        srd.pSysMem = dataRGBA.data();
        srd.SysMemPitch = W * 4;

        ComPtr<ID3D11Texture2D> tex;
        HRESULT hr = dev->CreateTexture2D(&td, &srd, tex.GetAddressOf());
        if (FAILED(hr)) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = td.Format;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = 1;

        hr = dev->CreateShaderResourceView(tex.Get(), &sd, outSRV.GetAddressOf());
        if (FAILED(hr)) return false;

        if (outW) *outW = W;
        if (outH) *outH = H;
        return true;
    }

    bool LoadR8(ID3D11Device* dev, const wchar_t* path,
        ComPtr<ID3D11ShaderResourceView>& outSRV,
        unsigned* outW, unsigned* outH)
    {
        outSRV.Reset();
        if (outW) *outW = 0; if (outH) *outH = 0;

        FILE* fp = nullptr;
#ifdef _WIN32
        _wfopen_s(&fp, path, L"rb");
#else
        fp = std::fopen(/*convert path*/, "rb");
#endif
        if (!fp) return false;

        BmpFileHeader fh{};
        BmpInfoHeader ih{};
        bool ok = ReadHeaders(fp, fh, ih);
        if (!ok) { std::fclose(fp); return false; }

        const unsigned W = (unsigned)ih.biWidth;
        const unsigned H = (unsigned)(ih.biHeight > 0 ? ih.biHeight : -ih.biHeight);
        const bool bottomUp = (ih.biHeight > 0);
        const unsigned bc = ih.biBitCount;

        if (bc != 24) { std::fclose(fp); return false; } // R8 로더는 24-bit만

        std::fseek(fp, (long)fh.bfOffBits, SEEK_SET);

        const unsigned srcStride = ((W * 3u) + 3u) & ~3u; // 4바이트 패딩
        std::vector<unsigned char> row(srcStride);
        std::vector<unsigned char> gray(W * H);

        for (unsigned y = 0; y < H; ++y) {
            if (std::fread(row.data(), 1, srcStride, fp) != srcStride) { std::fclose(fp); return false; }
            unsigned dy = bottomUp ? (H - 1 - y) : y;
            unsigned char* dst = &gray[dy * W];
            for (unsigned x = 0; x < W; ++x) {
                unsigned char B = row[x * 3 + 0];
                unsigned char G = row[x * 3 + 1];
                unsigned char R = row[x * 3 + 2];
                dst[x] = toGray(R, G, B);
            }
        }
        std::fclose(fp);

        D3D11_TEXTURE2D_DESC td{};
        td.Width = W; td.Height = H; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8_UNORM;
        td.SampleDesc.Count = 1;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA srd{};
        srd.pSysMem = gray.data();
        srd.SysMemPitch = W * sizeof(unsigned char);

        ComPtr<ID3D11Texture2D> tex;
        HRESULT hr = dev->CreateTexture2D(&td, &srd, tex.GetAddressOf());
        if (FAILED(hr)) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = td.Format;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = 1;

        hr = dev->CreateShaderResourceView(tex.Get(), &sd, outSRV.GetAddressOf());
        if (FAILED(hr)) return false;

        if (outW) *outW = W;
        if (outH) *outH = H;
        return true;
    }

} // namespace BMP
