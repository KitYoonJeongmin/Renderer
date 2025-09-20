// src/utils/Math.h
#pragma once
#include <DirectXMath.h>
#include <algorithm>

namespace Math {
    using namespace DirectX;

    constexpr float PI = 3.14159265358979323846f;
    constexpr float DEG2RAD = PI / 180.0f;
    constexpr float RAD2DEG = 180.0f / PI;

    // --- 기본 변환 ---
    inline XMMATRIX WorldTRS(const XMFLOAT3& t,
        const XMFLOAT3& eulerRad,  // pitch=x, yaw=y, roll=z
        const XMFLOAT3& s = XMFLOAT3{ 1,1,1 })
    {
        XMMATRIX T = XMMatrixTranslation(t.x, t.y, t.z);
        XMMATRIX R = XMMatrixRotationRollPitchYaw(eulerRad.x, eulerRad.y, eulerRad.z);
        XMMATRIX S = XMMatrixScaling(s.x, s.y, s.z);
        return S * R * T; // (S→R→T)
    }

    inline XMMATRIX ViewFPS(const XMFLOAT3& camPos, float yaw, float pitch)
    {
        // 앞·옆·위 벡터 계산 (FPS 스타일)
        XMVECTOR fwd = XMVector3Normalize(XMVectorSet(
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            cosf(pitch) * cosf(yaw),
            0.0f));
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), fwd));
        XMVECTOR up = XMVector3Normalize(XMVector3Cross(fwd, right));
        return XMMatrixLookToLH(XMLoadFloat3(&camPos), fwd, up);
    }

    inline XMMATRIX ProjFovLH(float fovY, float aspect, float znear = 0.1f, float zfar = 500.0f)
    {
        return XMMatrixPerspectiveFovLH(fovY, aspect, znear, zfar);
    }

    // 법선 변환용 (월드 행렬의 역전치)
    inline XMMATRIX InverseTranspose(XMMATRIX M)
    {
        M.r[3] = XMVectorSet(0, 0, 0, 1); // 이동 제거
        return XMMatrixTranspose(XMMatrixInverse(nullptr, M));
    }

    // 안전 클램프
    template<typename T>
    inline T Clamp(T v, T a, T b) { return std::max(a, std::min(b, v)); }

    // --- 초간단 프레임 타이머 ---
    struct FrameTimer {
        double freq = 0.0;
        long long prev = 0;

        void Reset();
        float  Tick(); // dt(s)
    };

#ifdef _WIN32
#include <windows.h>
    inline void FrameTimer::Reset() {
        LARGE_INTEGER f; QueryPerformanceFrequency(&f); freq = (double)f.QuadPart;
        LARGE_INTEGER t; QueryPerformanceCounter(&t);   prev = t.QuadPart;
    }
    inline float FrameTimer::Tick() {
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        double dt = (now.QuadPart - prev) / freq; prev = now.QuadPart;
        return (float)dt;
    }
#else
    // 플랫폼별로 바꿔 쓰세요
    inline void FrameTimer::Reset() { prev = 0; freq = 1.0; }
    inline float FrameTimer::Tick() { return 1.0f / 60.0f; }
#endif

} // namespace Math
