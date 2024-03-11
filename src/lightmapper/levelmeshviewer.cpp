
#ifdef WIN32

#include "levelmeshviewer.h"
#include "vk_renderdevice.h"
#include "framework/matrix.h"
#include <windowsx.h>
#include <stdexcept>
#include <zvulkan/vulkanswapchain.h>

LevelMeshViewer::LevelMeshViewer()
{
	CreateViewerWindow();
}

LevelMeshViewer::~LevelMeshViewer()
{
	DestroyViewerWindow();
}

void LevelMeshViewer::RenderFrame()
{
	RECT clientRect = {};
	GetClientRect(WindowHandle, &clientRect);
	int width = clientRect.right;
	int height = clientRect.bottom;
	if (width <= 0 || height <= 0)
	{
		Sleep(500);
		return;
	}

	RenderDevice->ResizeSwapChain(width, height);

	VSMatrix movematrix;
	movematrix.loadIdentity();
	movematrix.rotate((float)Yaw, 0.0f, 1.0f, 0.0);
	movematrix.rotate((float)Pitch, 1.0f, 0.0f, 0.0);
	if (MoveLeft)
	{
		CameraPos += (movematrix * FVector4(-10.0f, 0.0f, 0.0f, 1.0f)).XYZ();
	}
	if (MoveRight)
	{
		CameraPos += (movematrix * FVector4(10.0f, 0.0f, 0.0f, 1.0f)).XYZ();
	}
	if (MoveForward)
	{
		CameraPos += (movematrix * FVector4(0.0f, 0.0f, 10.0f, 1.0f)).XYZ();
	}
	if (MoveBackward)
	{
		CameraPos += (movematrix * FVector4(0.0f, 0.0f, -10.0f, 1.0f)).XYZ();
	}

	VSMatrix worldToView;
	worldToView.loadIdentity();
	worldToView.rotate((float)-Pitch, 1.0f, 0.0f, 0.0);
	worldToView.rotate((float)-Yaw, 0.0f, 1.0f, 0.0);
	worldToView.translate(-CameraPos.X, -CameraPos.Y, -CameraPos.Z);

	VSMatrix viewToWorld;
	worldToView.inverseMatrix(viewToWorld);

	RenderDevice->DrawViewer(CameraPos, viewToWorld, 60.0f, width / (float)height, SunDir, SunColor, SunIntensity);
}

void LevelMeshViewer::OnMouseMove(double dx, double dy)
{
	Yaw += dx * 0.1;
	while (Yaw < 0.0) Yaw += 360.0;
	while (Yaw >= 360.0) Yaw -= 360.0;

	Pitch += dy * 0.1;
	if (Pitch < -90.0) Pitch = -90.0;
	if (Pitch > 90.0) Pitch = 90.0;
}

void LevelMeshViewer::OnKeyDown(WPARAM key)
{
	if (key == 'A')
	{
		MoveLeft = true;
	}
	else if (key == 'D')
	{
		MoveRight = true;
	}
	else if (key == 'W')
	{
		MoveForward = true;
	}
	else if (key == 'S')
	{
		MoveBackward = true;
	}
}

void LevelMeshViewer::OnKeyUp(WPARAM key)
{
	if (key == 'A')
	{
		MoveLeft = false;
	}
	else if (key == 'D')
	{
		MoveRight = false;
	}
	else if (key == 'W')
	{
		MoveForward = false;
	}
	else if (key == 'S')
	{
		MoveBackward = false;
	}
}

void LevelMeshViewer::Exec(VulkanRenderDevice* renderdevice, const FVector3& sundir, const FVector3& suncolor, float sunintensity)
{
	RenderDevice = renderdevice;
	SunDir = sundir;
	SunColor = suncolor;
	SunIntensity = sunintensity;
	bool exitFlag = false;
	while (!exitFlag)
	{
		MSG msg = {};
		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				exitFlag = true;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		RenderFrame();
	}
	ShowWindow(WindowHandle, SW_HIDE);
}

void LevelMeshViewer::CreateViewerWindow()
{
	WNDCLASSEX classdesc = {};
	classdesc.cbSize = sizeof(WNDCLASSEX);
	classdesc.hInstance = GetModuleHandle(nullptr);
	classdesc.lpszClassName = L"LevelMeshViewerWindow";
	classdesc.lpfnWndProc = &LevelMeshViewer::WindowProc;
	classdesc.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
	classdesc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
	BOOL result = RegisterClassEx(&classdesc);
	if (!result)
		throw std::runtime_error("RegisterClassEx failed");

	HDC screenDC = GetDC(0);
	int dpi = GetDeviceCaps(screenDC, LOGPIXELSX);
	ReleaseDC(0, screenDC);
	auto applyDpi = [=](int i) { return (i * dpi + (96 / 2)) / 96; };

	// Create and show the window
	WindowHandle = CreateWindowEx(WS_EX_APPWINDOW, L"LevelMeshViewerWindow", L"ZDRay Viewer", WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_MAXIMIZE, applyDpi(100), applyDpi(50), applyDpi(1400), applyDpi(800), 0, 0, GetModuleHandle(nullptr), this);
	if (!WindowHandle)
		throw std::runtime_error("CreateWindowEx failed");
}

void LevelMeshViewer::DestroyViewerWindow()
{
	if (WindowHandle)
		DestroyWindow(WindowHandle);
	WindowHandle = 0;
}

void LevelMeshViewer::OnClose()
{
	PostQuitMessage(0);
}

LRESULT LevelMeshViewer::OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
	/*if (msg == WM_RBUTTONDOWN)
	{
		if (!MouseLocked)
		{
			GetCursorPos(&LockPosition);
			::ShowCursor(FALSE);

			RECT box = {};
			GetClientRect(WindowHandle, &box);

			POINT center = {};
			center.x = box.right / 2;
			center.y = box.bottom / 2;
			ClientToScreen(WindowHandle, &center);

			SetCursorPos(center.x, center.y);
			MouseLocked = true;
		}
	}
	else if (msg == WM_RBUTTONUP)
	{
		if (MouseLocked)
		{
			MouseLocked = false;
			SetCursorPos(LockPosition.x, LockPosition.y);
			::ShowCursor(TRUE);
		}
	}
	else*/ if (msg == WM_MOUSEMOVE)
	{
		if (MouseLocked && GetFocus() != 0)
		{
			RECT box = {};
			GetClientRect(WindowHandle, &box);

			POINT center = {};
			center.x = box.right / 2;
			center.y = box.bottom / 2;
			int mousex = GET_X_LPARAM(lparam) - center.x;
			int mousey = GET_Y_LPARAM(lparam) - center.y;

			ClientToScreen(WindowHandle, &center);
			SetCursorPos(center.x, center.y);

			double dpiscale = GetDpiScale();
			OnMouseMove(mousex / dpiscale, mousey / dpiscale);
		}
		else
		{
			SetCursor((HCURSOR)LoadImage(0, IDC_ARROW, IMAGE_CURSOR, LR_DEFAULTSIZE, LR_DEFAULTSIZE, LR_SHARED));
		}
	}
	else if (msg == WM_SETFOCUS)
	{
		if (MouseLocked)
		{
			GetCursorPos(&LockPosition);
			::ShowCursor(FALSE);

			RECT box = {};
			GetClientRect(WindowHandle, &box);

			POINT center = {};
			center.x = box.right / 2;
			center.y = box.bottom / 2;
			ClientToScreen(WindowHandle, &center);

			SetCursorPos(center.x, center.y);
		}
	}
	else if (msg == WM_KILLFOCUS)
	{
		if (MouseLocked)
		{
			SetCursorPos(LockPosition.x, LockPosition.y);
			::ShowCursor(TRUE);
		}
	}
	else if (msg == WM_CLOSE)
	{
		OnClose();
		return 0;
	}
	else if (msg == WM_KEYDOWN)
	{
		OnKeyDown(wparam);
	}
	else if (msg == WM_KEYUP)
	{
		OnKeyUp(wparam);
	}

	return DefWindowProc(WindowHandle, msg, wparam, lparam);
}

double LevelMeshViewer::GetDpiScale() const
{
	return GetDpiForWindow(WindowHandle) / 96.0;
}

int LevelMeshViewer::GetPixelWidth() const
{
	RECT box = {};
	GetClientRect(WindowHandle, &box);
	return box.right;
}

int LevelMeshViewer::GetPixelHeight() const
{
	RECT box = {};
	GetClientRect(WindowHandle, &box);
	return box.bottom;
}

LRESULT LevelMeshViewer::WindowProc(HWND windowhandle, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_CREATE)
	{
		CREATESTRUCT* createstruct = (CREATESTRUCT*)lparam;
		LevelMeshViewer* viewport = (LevelMeshViewer*)createstruct->lpCreateParams;
		viewport->WindowHandle = windowhandle;
		SetWindowLongPtr(windowhandle, GWLP_USERDATA, (LONG_PTR)viewport);
		return viewport->OnWindowMessage(msg, wparam, lparam);
	}
	else
	{
		LevelMeshViewer* viewport = (LevelMeshViewer*)GetWindowLongPtr(windowhandle, GWLP_USERDATA);
		if (viewport)
		{
			LRESULT result = viewport->OnWindowMessage(msg, wparam, lparam);
			if (msg == WM_DESTROY)
			{
				SetWindowLongPtr(windowhandle, GWLP_USERDATA, 0);
				viewport->WindowHandle = 0;
			}
			return result;
		}
		else
		{
			return DefWindowProc(windowhandle, msg, wparam, lparam);
		}
	}
}

#endif
