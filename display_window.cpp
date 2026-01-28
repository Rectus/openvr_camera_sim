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
    //wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    //wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_OPENVRAMBIENTLIGHT);
    wcex.lpszClassName = WINDOW_CLASS_NAME;
    //wcex.hIconSm = g_iconOn;

    if (RegisterClassExW(&wcex) == 0)
    {
        std::string message = std::format("Failed to create window class: 0x{:X}", (unsigned long)GetLastError());
        vr::VRDriverLog()->Log(message.c_str());
        return false;
    }

    

    m_window = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW, WINDOW_CLASS_NAME, WINDOW_TITLE, WS_OVERLAPPEDWINDOW, m_windowPosX, m_windowPosY, m_windowWidth, m_windowHeight, NULL, NULL, hModuleHandle, nullptr);
    //m_window = CreateWindowExW(0, WINDOW_CLASS_NAME, WINDOW_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 800, 600, NULL, NULL, hModuleHandle, nullptr);


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
    //std::string msg = std::format("WindowProc: 0x{:x} {} {}", message, wParam, lParam);
    //vr::VRDriverLog()->Log(msg.c_str());

    switch (message)
    {
    case WM_PAINT:
    case WM_MOVE:
        //vr::VRDriverLog()->Log("WindowProc: WM_PAINT");

        /*{
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

            EndPaint(hWnd, &ps);
        }*/
        break;

    case WM_CLOSE:
        //vr::VRDriverLog()->Log("WindowProc: WM_CLOSE");
        break;

    case WM_DESTROY:
        //vr::VRDriverLog()->Log("WindowProc: WM_DESTROY");
        break;

    default:

        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;


    //return DefWindowProc(hWnd, message, wParam, lParam);

    //switch (message)
    //{
    //case WM_COMMAND:
    //{
    //    int wmId = LOWORD(wParam);
    //    switch (wmId)
    //    {
    //    case SC_KEYMENU:
    //        return 0; // Disable alt menu
    //        break;

    //    default:
    //        return DefWindowProc(hWnd, message, wParam, lParam);
    //    }
    //}
    //break;

    //case WM_PAINT:
    //case WM_MOVE: // Paint on move to prevent the window from blanking out (only works partially).

    //    if (g_bIsSettingsWindowShown && !g_bIsPainting && g_settingsMenu.get())
    //    {
    //        g_bIsPainting = true;
    //        g_settingsMenu->TickMenu();
    //        g_bIsPainting = false;
    //    }

    //    break;

    //case WM_CLOSE:
    //    if (g_bExitOnClose)
    //    {
    //        g_bRun = false;
    //    }
    //    else
    //    {
    //        g_bIsSettingsWindowShown = false;
    //        ShowWindow(g_hSettingsWindow, SW_HIDE);
    //    }

    //    break;

    //case WM_DESTROY:
    //    g_bRun = false;
    //    break;


    //case WM_TRAYMESSAGE:
    //    switch (lParam)
    //    {
    //    case WM_LBUTTONDBLCLK:

    //        KillTimer(g_hSettingsWindow, g_trayClickTimer);
    //        g_bIsDoubleClicking = true;

    //        break;

    //    case WM_LBUTTONDOWN:

    //        g_trayClickTimer = SetTimer(g_hSettingsWindow, 100, GetDoubleClickTime(), TrayClickTimerCallback);
    //        GetCursorPos(&g_trayCursorPos);

    //        break;

    //    case WM_LBUTTONUP:

    //        if (g_bIsDoubleClicking)
    //        {
    //            g_bIsDoubleClicking = false;
    //            ShowWindow(g_hSettingsWindow, g_bIsSettingsWindowShown ? SW_HIDE : SW_SHOW);
    //            SetForegroundWindow(g_hSettingsWindow);
    //            g_bIsSettingsWindowShown = !g_bIsSettingsWindowShown;
    //        }

    //        break;

    //    case WM_RBUTTONUP:

    //        GetCursorPos(&g_trayCursorPos);
    //        TrayShowMenu();
    //        break;

    //    default:
    //        return DefWindowProc(hWnd, message, wParam, lParam);
    //    }
    //    break;

    //case WM_SETTINGS_UPDATED:

    //    DispatchSettingsUpdated(LOWORD(wParam));

    //    break;

    //case WM_MENU_QUIT:

    //    g_bRun = false;

    //    break;

    //default:

    //    if (!g_settingsMenu.get() || !g_settingsMenu->HandleWin32Events(hWnd, message, wParam, lParam))
    //    {
    //        return DefWindowProc(hWnd, message, wParam, lParam);
    //    }
    //}



    //return 0;
 }