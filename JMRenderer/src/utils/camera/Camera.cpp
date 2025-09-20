#include "Camera.h"
#include <algorithm>
using namespace DirectX;

static inline bool KeyDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

CameraFPS::CameraFPS()
{
    mPos = XMFLOAT3(0.f, 2.f, -5.f);
    mYaw = 0.f; mPitch = 0.f;
}

void CameraFPS::SetPosition(const XMFLOAT3& p) { mPos = p; }

void CameraFPS::SetYawPitch(float yawRad, float pitchRad) {
    mYaw = yawRad;
    mPitch = Clamp(pitchRad, -1.5f, 1.5f);
}

void CameraFPS::AddYawPitch(float dYaw, float dPitch) {
    mYaw += dYaw;
    mPitch = Clamp(mPitch + dPitch, -1.5f, 1.5f);
}

void CameraFPS::SetLens(float fovY, float zn, float zf) {
    mFovY = fovY; mZNear = zn; mZFar = zf;
}

XMVECTOR CameraFPS::Forward() const {
    XMVECTOR f = XMVectorSet(
        cosf(mPitch) * sinf(mYaw),
        sinf(mPitch),
        cosf(mPitch) * cosf(mYaw),
        0.0f);
    return XMVector3Normalize(f);
}

XMVECTOR CameraFPS::Right() const {
    return XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), Forward()));
}

XMVECTOR CameraFPS::Up() const {
    return XMVector3Normalize(XMVector3Cross(Forward(), Right()));
}

XMMATRIX CameraFPS::View() const {
    return XMMatrixLookToLH(XMLoadFloat3(&mPos), Forward(), Up());
}

XMMATRIX CameraFPS::Proj(float aspect) const {
    return XMMatrixPerspectiveFovLH(mFovY, aspect, mZNear, mZFar);
}

void CameraFPS::UpdateFromKeyboardWin32(float dt)
{
    // 방향키 회전(마우스 룩 중엔 비활성화하고 싶으면 if(!mMouseLook)로 감싸도 됨)
    if (KeyDown(VK_LEFT))  AddYawPitch(-mTurnSpd * dt, 0);
    if (KeyDown(VK_RIGHT)) AddYawPitch(mTurnSpd * dt, 0);
    if (KeyDown(VK_UP))    AddYawPitch(0, -mTurnSpd * dt);
    if (KeyDown(VK_DOWN))  AddYawPitch(0, mTurnSpd * dt);

    // WASD + Space/Ctrl 이동
    XMVECTOR pos = XMLoadFloat3(&mPos);
    if (KeyDown('W')) pos = XMVectorAdd(pos, XMVectorScale(Forward(), mMoveSpd * dt));
    if (KeyDown('S')) pos = XMVectorAdd(pos, XMVectorScale(Forward(), -mMoveSpd * dt));
    if (KeyDown('A')) pos = XMVectorAdd(pos, XMVectorScale(Right(), -mMoveSpd * dt));
    if (KeyDown('D')) pos = XMVectorAdd(pos, XMVectorScale(Right(), mMoveSpd * dt));
    if (KeyDown('E'))   pos = XMVectorAdd(pos, XMVectorSet(0, mMoveSpd * dt, 0, 0));
    if (KeyDown('Q')) pos = XMVectorAdd(pos, XMVectorSet(0, -mMoveSpd * dt, 0, 0));
    XMStoreFloat3(&mPos, pos);
}

bool CameraFPS::HandleWin32MouseMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, bool wantCaptureMouse)
{
    switch (msg)
    {
    case WM_RBUTTONDOWN:
        if (!wantCaptureMouse) {
            SetCapture(hWnd);
            mMouseLook = true;
            GetCursorPos(&mPrev);
            ShowCursor(FALSE);
            return true;
        }
        break;

    case WM_RBUTTONUP:
        if (mMouseLook) {
            mMouseLook = false;
            ReleaseCapture();
            ShowCursor(TRUE);
            return true;
        }
        break;

    case WM_MOUSEMOVE:
        if (mMouseLook) {
            POINT p; GetCursorPos(&p);
            int dx = p.x - mPrev.x;
            int dy = p.y - mPrev.y;
            mPrev = p;
            AddYawPitch(dx * mMouseSens, -dy * mMouseSens);
            return true;
        }
        break;

    case WM_KILLFOCUS:
        if (mMouseLook) {
            mMouseLook = false;
            ReleaseCapture();
            ShowCursor(TRUE);
            return false; // 알림만
        }
        break;
    }
    return false;
}

float CameraFPS::Clamp(float v, float a, float b) {
    return std::max(a, std::min(b, v));
}
