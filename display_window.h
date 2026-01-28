
#include "d3d11_renderer.h"

#pragma once
class DisplayWindow
{
public:
	DisplayWindow(int32_t posX, int32_t posY, int32_t width, int32_t height);
	~DisplayWindow();

	HWND GetWindow() { return m_window; }
	bool IsRunning() { return m_bRunThread; }
	bool IsInitialized() { return m_bIsInitalized; }

protected:
	void RunThread();
	bool InitWindow();

	std::thread m_thread;
	bool m_bRunThread;
	bool m_bIsInitalized;

	HWND m_window = NULL;

	int32_t m_windowPosX;
	int32_t m_windowPosY;
	int32_t m_windowWidth;
	int32_t m_windowHeight;
};

