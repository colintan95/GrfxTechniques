#include "App.h"
#include "InputManager.h"

#include <windows.h>

#include <memory>

static InputManager* g_inputManager = nullptr;

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    if (g_inputManager)
    {
        switch (message)
        {
            case WM_KEYDOWN:
                g_inputManager->HandleKeyDown(static_cast<UINT>(wparam));
                break;
            case WM_KEYUP:
                g_inputManager->HandleKeyUp(static_cast<UINT>(wparam));
                break;
        }
    }

    return DefWindowProc(hwnd, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE, LPSTR, int cmdShow)
{
    WNDCLASSEX windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hinstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"GrfxTechniques";
    RegisterClassEx(&windowClass);

    HWND hwnd = CreateWindow(windowClass.lpszClassName, L"Grfx Techniques", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, nullptr, nullptr, hinstance,
                             nullptr);
    ShowWindow(hwnd, cmdShow);

    auto inputManager = std::make_unique<InputManager>();
    g_inputManager = inputManager.get();

    auto app = std::make_unique<App>(hwnd, inputManager.get());

    MSG msg{};

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        app->Render();
    }

    app.reset();

    g_inputManager = nullptr;
    inputManager.reset();

    return 0;
}



