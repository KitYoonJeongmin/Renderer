// 최소 Win32 + D3D11 + ImGui + 셰이더 핫리로드 예시
GDev.BeginFrame(GClearColor);


auto* ctx = GDev.GetContext();


// 파이프라인 바인딩
GShader.Bind(ctx);
GTri.Bind(ctx);


// 드로우 콜
ctx->Draw(3, 0);


// ImGui
/*
ImGui_ImplDX11_NewFrame();
ImGui_ImplWin32_NewFrame();
ImGui::NewFrame();


ImGui::Begin("M0 HUD");
ImGui::Text("Resolution: %u x %u", GWidth, GHeight);
ImGui::ColorEdit3("ClearColor", GClearColor);
ImGui::Checkbox("VSync", &GVsync);
ImGui::Text("Drag this window and resize the app to test backbuffer recreation.");
ImGui::End();


ImGui::Render();
ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
*/

// 프레젠트
GDev.EndFrame(GVsync);
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
	GInstance = hInstance;


	// Win32 창 클래스 등록
	WNDCLASSEXW wc{ sizeof(WNDCLASSEXW) };
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = L"MyRendererWndClass";
	RegisterClassExW(&wc);


	// 창 생성
	RECT rc{ 0, 0, (LONG)GWidth, (LONG)GHeight };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	GWnd = CreateWindowW(wc.lpszClassName, L"MyRenderer — M0",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left, rc.bottom - rc.top,
		nullptr, nullptr, hInstance, nullptr);


	ShowWindow(GWnd, nCmdShow);
	UpdateWindow(GWnd);


	InitAll();


	// 메시지 루프(Non-blocking)
	MSG msg{};
	bool running = true;
	while (running)
	{
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) { running = false; break; }
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (!running) break;
		RenderFrame();
	}


	ShutdownAll();
	return 0;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// ImGui Win32 백엔드에 먼저 전달
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}
		


	switch (message)
	{
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED) {
			GWidth = LOWORD(lParam);
			GHeight = HIWORD(lParam);
			if (GWidth && GHeight) GDev.Resize(GWidth, GHeight);
		}
		return 0;


	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}