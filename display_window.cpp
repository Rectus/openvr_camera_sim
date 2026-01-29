
#include "pch.h"
#include "display_window.h"



const wchar_t WINDOW_TITLE[] = L"OpenVR Camera Sim";
const wchar_t WINDOW_CLASS_NAME[] = L"OpenVR Camera Sim Window Class";

DisplayWindow::DisplayWindow(int32_t posX, int32_t posY, int32_t width, int32_t height)
     : m_windowPosX(posX)
     , m_windowPosY(posY)
     , m_windowWidth(width)
     , m_windowHeight(height)
{
    m_bRunThread = true;
    m_bIsInitalized = false;
    m_thread = std::thread(&DisplayWindow::RunThread, this);
}

DisplayWindow::~DisplayWindow()
{
    vr::VRDriverLog()->Log("~DisplayWindow");
    m_bIsInitalized = false;
    m_bRunThread = false;
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

void DisplayWindow::RunThread()
{
    if (!InitWindow())
    {
        m_bRunThread = false;
        return;
    }

    MSG msg = {};

    while (m_bRunThread && GetMessage(&msg, m_window, 0, 0))
    {

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyWindow(m_window);
    m_bRunThread = false;

    vr::VRDriverLog()->Log("Display window stopping");
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void DummyFunc() {}

bool DisplayWindow::InitWindow()
{
    HMODULE hModuleHandle = NULL;

    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(&DummyFunc), &hModuleHandle);

    {
        std::string message = std::format("Module address: 0x{:X}", reinterpret_cast<uint64_t>(hModuleHandle));
        vr::VRDriverLog()->Log(message.c_str());
    }

    WNDCLASSEXW wcex = {};

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hModuleHandle;
    wcex.hIcon = NULL;
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszClassName = WINDOW_CLASS_NAME;

    if (RegisterClassExW(&wcex) == 0)
    {
        std::string message = std::format("Failed to create window class: 0x{:X}", (unsigned long)GetLastError());
        vr::VRDriverLog()->Log(message.c_str());
        return false;
    }

    
    m_window = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW, WINDOW_CLASS_NAME, WINDOW_TITLE, WS_OVERLAPPEDWINDOW, m_windowPosX, m_windowPosY, m_windowWidth, m_windowHeight, NULL, NULL, hModuleHandle, nullptr);


    if (!m_window)
    {
        std::string message = std::format("Failed to create window: 0x{:X}", (unsigned long)GetLastError());
        vr::VRDriverLog()->Log(message.c_str());
        return false;
    }

    ShowWindow(m_window, SW_SHOWNORMAL);
    UpdateWindow(m_window);

    m_bIsInitalized = true;

    return true;
}


LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    case WM_MOVE:
        
        break;

    case WM_CLOSE:

        break;

    case WM_DESTROY:

        break;

    default:

        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
 }