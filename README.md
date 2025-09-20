# [M0] 삼각형 그리기
### 작업 내역
- Win32 창 생성, **DXGI 스왑체인 + D3D11 디바이스/컨텍스트** 생성
- 렌더 루프: `BeginFrame → Clear → EndFrame`
- **ImGui** 붙여서 FPS, 해상도, 톤(감마) 슬라이더 노출
- **HLSL 핫리로드**: 셰이더 파일이 바뀌면 자동 재컴파일 & 바인딩

<img width="1589" height="923" alt="image" src="https://github.com/user-attachments/assets/619ad8bb-8fb2-406a-be1e-d4afb8739cc9" />

# [M1] Terrain
### 작업 내역
- 카메라 이동 추가
  - `W`: 앞
  - `S`: 뒤
  - `A`: 좌
  - `D`: 우
  - `E`: 위
  - `Q`: 아래
- 라이트 추가
- 그리드 메시 추가
- 하이트 맵 버텍스 쉐이더 추가
- 픽셀 쉐이더 추가
  - Grass, Stone, Snow 비트맵 블랜딩
- 와이어프레임 기능 추가

<img width="1578" height="883" alt="image" src="https://github.com/user-attachments/assets/5b4a56b7-a31b-404f-8b2c-80f0e9834b40" />

<img width="1580" height="876" alt="image" src="https://github.com/user-attachments/assets/eaed0bbf-2843-46a3-aeb9-59fa41852d3e" />



