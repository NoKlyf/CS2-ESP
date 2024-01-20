#include "include/memory.h"
#include <Windows.h>
#include <thread>
#include <math.h>

#include <dwmapi.h>
#include <d3d11.h>
#include <windowsx.h>
#include "../external/ImGui/imgui.h"
#include "../external/ImGui/imgui_impl_dx11.h"
#include "../external/ImGui/imgui_impl_win32.h"

#include "include/vector.h"
#include "include/render.h"
#include "include/bone.h"
#include "include/offsets.h"

int screenWidth = GetSystemMetrics(SM_CXSCREEN);
int screenHeight = GetSystemMetrics(SM_CYSCREEN);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return 0L;

	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0L;
	}

	switch (msg)
	{
	case WM_NCHITTEST:
	{
		const LONG borderWidth = GetSystemMetrics(SM_CXSIZEFRAME);
		const LONG titleBarHeight = GetSystemMetrics(SM_CYCAPTION);
		POINT cursorPos = { GET_X_LPARAM(wParam), GET_Y_LPARAM(lParam) };
		RECT windowRect;
		GetWindowRect(hWnd, &windowRect);

		if (cursorPos.y >= windowRect.top && cursorPos.y < windowRect.top + titleBarHeight)
			return HTCAPTION;

		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

INT WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ PSTR, _In_ INT cmdShow)
{
	FILE* file;
	AllocConsole();
	freopen_s(&file, "CONOUT$", "w", stdout);

	SetConsoleTitle("CS2 ESP");

	Memory mem{ "cs2.exe" };
	auto client = mem.GetModuleAddress("client.dll");

	if (client == 0)
	{
		std::cout << "Open Counter-Strike 2 before starting the cheat" << std::endl;
		Sleep(5000);
		fclose(file);
		return 1;
	}

	std::cout << "Got client.dll address: 0x" << client << std::endl;

	WNDCLASSEXW wc{ };
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"CS2 ESP";

	RegisterClassExW(&wc);

	const HWND overlay = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
		wc.lpszClassName,
		L"CS2 ESP",
		WS_POPUP,
		0,
		0,
		screenWidth,
		screenHeight,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);

	SetLayeredWindowAttributes(overlay, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

	{
		RECT client_area{ };
		GetClientRect(overlay, &client_area);

		RECT window_area{ };
		GetWindowRect(overlay, &window_area);

		POINT diff{ };
		ClientToScreen(overlay, &diff);

		const MARGINS margins{
			window_area.left + (diff.x - window_area.left),
			window_area.top + (diff.y - window_area.top),
			client_area.right,
			client_area.bottom
		};

		DwmExtendFrameIntoClientArea(overlay, &margins);
	}

	DXGI_SWAP_CHAIN_DESC sd{ };
	sd.BufferDesc.RefreshRate.Numerator = 60U;
	sd.BufferDesc.RefreshRate.Denominator = 1U;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1U;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2U;
	sd.OutputWindow = overlay;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	constexpr D3D_FEATURE_LEVEL levels[2]{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0
	};

	ID3D11Device* device{ nullptr };
	ID3D11DeviceContext* device_context{ nullptr };
	IDXGISwapChain* swap_chain{ nullptr };
	ID3D11RenderTargetView* render_target_view{ nullptr };
	D3D_FEATURE_LEVEL level{ };

	D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0U,
		levels,
		2U,
		D3D11_SDK_VERSION,
		&sd,
		&swap_chain,
		&device,
		&level,
		&device_context
	);

	ID3D11Texture2D* back_buffer{ nullptr };
	swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

	if (back_buffer) {
		device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
		back_buffer->Release();
	} else
		return 1;

	ShowWindow(overlay, cmdShow);
	UpdateWindow(overlay);

	ImGui::CreateContext();
	ImGui::StyleColorsClassic();

	ImGui_ImplWin32_Init(overlay);
	ImGui_ImplDX11_Init(device, device_context);

	bool running = true;

	while (running)
	{
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				running = false;
		}

		if (!running)
			break;

		uintptr_t localPlayer = mem.Read<uintptr_t>(client + offsets::dwLocalPlayer);
		uintptr_t entityList = mem.Read<uintptr_t>(client + offsets::dwEntityList);
		Vector3 localOrigin = mem.Read<Vector3>(localPlayer + offsets::m_vOldOrigin);
		view_matrix_t view_matrix = mem.Read<view_matrix_t>(client + offsets::dwViewMatrix);
		int localTeam = mem.Read<int>(localPlayer + offsets::m_iTeamNum);

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		for (int i = 1; i < 64; ++i)
		{
			uintptr_t listEntry = mem.Read<uintptr_t>(entityList + (8 * (i & 0x7FFF) >> 9) + 16);

			if (!listEntry)
				continue;

			uintptr_t player = mem.Read<uintptr_t>(listEntry + 120 * (i & 0x1FF));

			if (!player)
				continue;

			int playerTeam = mem.Read<int>(player + offsets::m_iTeamNum);

			if (playerTeam == localTeam)
				continue;

			uint32_t playerPawn = mem.Read<uint32_t>(player + offsets::m_hPlayerPawn);
			uintptr_t listEntry2 = mem.Read<uintptr_t>(entityList + 0x8 * ((playerPawn & 0x7FFF) >> 9) + 16);

			if (!listEntry2)
				continue;

			uintptr_t pCSPlayerPawn = mem.Read<uintptr_t>(listEntry2 + 120 * (playerPawn & 0x1FF));

			if (!pCSPlayerPawn)
				continue;

			int health = mem.Read<int>(pCSPlayerPawn + offsets::m_iHealth);

			if (health <= 0 || health > 100)
				continue;

			if (pCSPlayerPawn == localPlayer)
				continue;

			uintptr_t gameScene = mem.Read<uintptr_t>(pCSPlayerPawn + offsets::m_pGameSceneNode);
			uintptr_t boneArray = mem.Read<uintptr_t>(gameScene + offsets::m_modelState + 0x80);

			Vector3 origin = mem.Read<Vector3>(pCSPlayerPawn + offsets::m_vOldOrigin);
			Vector3 head = mem.Read<Vector3>(boneArray + bones::head * 32);

			std::string name = mem.Read<std::string>(player + offsets::m_iszPlayerName);
			int32_t spotted = mem.Read<int32_t>(pCSPlayerPawn + offsets::m_entitySpottedState + offsets::m_bSpottedByMask);

			Vector3 screenPos = origin.WTS(view_matrix);
			Vector3 screenHead = head.WTS(view_matrix);
			float distance = Vector3::distance(localOrigin, origin);

			float headHeight = (screenPos.y - screenHead.y) / 8;

			float height = screenPos.y - screenHead.y;
			float width = height / 2.4f;

			RGB enemy = { 255, 0, 0 };
			RGB skeleton = { 255, 255, 255 };
			RGB healthColor = { 0, 255, 0 };

			if (!mem.InForeground())
				continue;
			
			if (!((screenPos.x == NULL && screenPos.y == NULL && screenPos.z == NULL) || (screenHead.x == NULL && screenHead.y == NULL && screenHead.z == NULL)))
			{
				Render::Rect(
					screenHead.x - width / 2,
					screenHead.y,
					width,
					height,
					enemy,
					1.5,
					false,
					255
				);

				Render::Circle(
					screenHead.x,
					screenHead.y,
					headHeight - 3,
					skeleton,
					false,
					255
				);

				Render::Rect(
					screenHead.x - (width / 2 + 10),
					screenHead.y + (height * (100 - health) / 100),
					2,
					height - (height * (100 - health) / 100),
					healthColor,
					1.5 / distance,
					true,
					255
				);

				Render::Text(screenPos.x - width / 2, screenPos.y, name.c_str(), skeleton);

				if (spotted) //TODO: This will render whenever a player is spotted by a teammate on the radar. It's checking bSpotted instead of bSpottedByMask. idk why. Fix
				{
					Render::Rect(
						screenHead.x - width / 2,
						screenHead.y,
						width,
						height,
						enemy,
						1.5,
						true,
						50
					);
				}
			}

			for (int j = 0; j < sizeof(boneConnections) / sizeof(boneConnections[0]); j++)
			{
				int bone1 = boneConnections[j].bone1;
				int bone2 = boneConnections[j].bone2;

				Vector3 vecBone1 = mem.Read<Vector3>(boneArray + bone1 * 32);
				Vector3 vecBone2 = mem.Read<Vector3>(boneArray + bone2 * 32);

				Vector3 screenBone1 = vecBone1.WTS(view_matrix);
				Vector3 screenBone2 = vecBone2.WTS(view_matrix);

				if (!((screenBone1.x == NULL && screenBone1.y == NULL && screenBone1.z == NULL) ||
					(screenBone2.x == NULL && screenBone2.y == NULL && screenBone2.z == NULL)))
					Render::Line(screenBone1.x, screenBone1.y, screenBone2.x, screenBone2.y, skeleton, 1.5 / distance, 255);
			}
		}

		ImGui::Render();
		float color[4]{ 0, 0, 0, 0 };
		device_context->OMSetRenderTargets(1U, &render_target_view, nullptr);

		if (render_target_view)
			device_context->ClearRenderTargetView(render_target_view, color);

		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		swap_chain->Present(0U, 0U);

		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	if (swap_chain)
		swap_chain->Release();

	if (device_context)
		device_context->Release();

	if (device)
		device->Release();

	if (render_target_view)
		render_target_view->Release();

	DestroyWindow(overlay);
	UnregisterClassW(wc.lpszClassName, wc.hInstance);

	fclose(file);

	return 0;
}
