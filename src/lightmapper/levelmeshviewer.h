#pragma once

#include "framework/vectors.h"

class VulkanRenderDevice;

#ifdef WIN32

#define WIN32_MEAN_AND_LEAN
#define NOMINMAX
#include <Windows.h>

class LevelMeshViewer
{
public:
	LevelMeshViewer();
	~LevelMeshViewer();

	void Exec(VulkanRenderDevice* renderdevice, const FVector3& sundir, const FVector3& suncolor, float sunintensity);
	HWND GetWindowHandle() { return WindowHandle; }

private:
	void RenderFrame();

	void OnMouseMove(double dx, double dy);
	void OnKeyDown(WPARAM key);
	void OnKeyUp(WPARAM key);
	void OnClose();

	void CreateViewerWindow();
	void DestroyViewerWindow();

	double GetDpiScale() const;
	int GetPixelWidth() const;
	int GetPixelHeight() const;

	LRESULT OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam);
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	VulkanRenderDevice* RenderDevice = nullptr;
	HWND WindowHandle = 0;
	bool MouseLocked = true;
	POINT LockPosition = {};

	double Yaw = 0.0;
	double Pitch = 0.0;

	bool MoveLeft = false;
	bool MoveRight = false;
	bool MoveForward = false;
	bool MoveBackward = false;
	FVector3 CameraPos = FVector3(0.0f, 70.0f, -128.0f);
	FVector3 SunDir = FVector3(0.0f, 0.0f, 0.0f);
	FVector3 SunColor = FVector3(0.0f, 0.0f, 0.0f);
	float SunIntensity = 0.0f;
};

#else

class LevelMeshViewer
{
public:
	LevelMeshViewer() { }
	void Exec(VulkanRenderDevice* renderdevice) { }
};

#endif
