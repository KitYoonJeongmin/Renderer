# [M0] 삼각형 그리기
### 작업 내역
- Win32 창 생성, **DXGI 스왑체인 + D3D11 디바이스/컨텍스트** 생성
- 렌더 루프: `BeginFrame → Clear → EndFrame`
- **ImGui** 붙여서 FPS, 해상도, 톤(감마) 슬라이더 노출
- **HLSL 핫리로드**: 셰이더 파일이 바뀌면 자동 재컴파일 & 바인딩

<img width="1589" height="923" alt="image" src="https://github.com/user-attachments/assets/619ad8bb-8fb2-406a-be1e-d4afb8739cc9" />
