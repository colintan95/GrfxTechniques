#include "App.h"
#include "InputManager.h"

#include <imgui_impl_win32.h>
#include <windows.h>

#include <chrono>
#include <memory>

static InputManager* g_inputManager = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam))
        return 1;

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

static void CheckMMResult(MMRESULT result)
{
    if (result != MMSYSERR_NOERROR)
        throw std::runtime_error("Invalid MMResult.");
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

    static constexpr int timerResolutionMs = 1;

    TIMECAPS timecaps{};
    CheckMMResult(timeGetDevCaps(&timecaps, sizeof(TIMECAPS)));

    if (timecaps.wPeriodMin > timerResolutionMs)
        exit(-1);

    // Sets the resolution of Sleep().
    CheckMMResult(timeBeginPeriod(timerResolutionMs));

    MSG msg{};

    auto prevTime = std::chrono::steady_clock::now();
    double prevDuration = 0.0;

    while (msg.message != WM_QUIT)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        app->Tick(prevDuration);

        app->Render();

        auto endTime = std::chrono::steady_clock::now();
        double duration = std::chrono::duration<double>(endTime - prevTime).count();

        static constexpr double minFrameTime = 1.0 / 60.0;

        if (duration < minFrameTime)
        {
            int sleepMs = static_cast<int>(std::ceil((minFrameTime - duration) * 1000.0));

            Sleep(sleepMs);

            endTime = std::chrono::steady_clock::now();
            duration = std::chrono::duration<double>(endTime - prevTime).count();
        }

        prevTime = endTime;
        prevDuration = duration;
    }

    app.reset();

    g_inputManager = nullptr;
    inputManager.reset();

    return 0;
}



