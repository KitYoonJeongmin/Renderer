#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <DirectXMath.h>

class CameraFPS {
public:
    CameraFPS();

    // 상태
    void SetPosition(const DirectX::XMFLOAT3& p);
    DirectX::XMFLOAT3 Position() const { return mPos; }

    void SetYawPitch(float yawRad, float pitchRad);
    void AddYawPitch(float dYaw, float dPitch);

    void SetMoveSpeed(float s) { mMoveSpd = s; }
    void SetTurnSpeed(float s) { mTurnSpd = s; }
    void SetLens(float fovY, float zn, float zf);
    void SetMouseSensitivity(float s) { mMouseSens = s; }

    // 입력 업데이트
    void UpdateFromKeyboardWin32(float dt);

    // Win32 마우스 메시지를 카메라에서 처리
    // (반환값: 이 메시지를 카메라가 소비했으면 true)
    bool HandleWin32MouseMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, bool wantCaptureMouse);

    // 행렬
    DirectX::XMMATRIX View() const;
    DirectX::XMMATRIX Proj(float aspect) const;

    // 방향 벡터
    DirectX::XMVECTOR Forward() const;
    DirectX::XMVECTOR Right() const;
    DirectX::XMVECTOR Up() const;

private:
    static float Clamp(float v, float a, float b);

    DirectX::XMFLOAT3 mPos{};
    float mYaw = 0.f;
    float mPitch = 0.f;
    float mMoveSpd = 5.f;
    float mTurnSpd = 1.5f;
    float mFovY = DirectX::XM_PIDIV4;
    float mZNear = 0.1f;
    float mZFar = 500.f;

    // 마우스 룩 상태
    bool  mMouseLook = false;
    POINT mPrev{ 0,0 };
    float mMouseSens = 0.0025f; // rad/pixel
};
