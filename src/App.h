#pragma once

#include "DebugPass.h"
#include "GpuResourceManager.h"
#include "InputManager.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <glm/glm.hpp>
#include <wil/resource.h>
#include <winrt/base.h>

#include <optional>

class App
{
public:
    App(HWND hwnd, InputManager* inputManager);
    ~App();

    void Render();

    void Tick(double elapsedSec);

private:
    void CreateDevice();

    void CreateCmdQueueAndSwapChain();

    void CreateCommandList();

    void CreatePipelineState();

    void CreateDescriptorHeaps();

    void CreateDepthTexture();

    void CreateConstantBuffer();

    void BeginFrame();

    void DrawModels();

    void RenderGui();

    void PresentFrame();

    void ExecuteAndWait();

    InputManager* m_inputManager = nullptr;

    HWND m_hwnd;

    UINT m_windowWidth = 0;
    UINT m_windowHeight = 0;

    static constexpr int NUM_FRAMES = 2;

    winrt::com_ptr<IDXGIFactory6> m_factory;
    winrt::com_ptr<ID3D12Device> m_device;

    std::unique_ptr<GpuResourceManager> m_resourceManager;

    winrt::com_ptr<ID3D12CommandQueue> m_cmdQueue;
    winrt::com_ptr<IDXGISwapChain3> m_swapChain;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    winrt::com_ptr<ID3D12CommandAllocator> m_cmdAlloc;
    winrt::com_ptr<ID3D12GraphicsCommandList> m_cmdList;

    winrt::com_ptr<ID3D12Fence> m_fence;
    uint64_t m_fenceValue = 0;

    wil::unique_handle m_fenceEvent;

    struct Frame
    {
        winrt::com_ptr<ID3D12Resource> SwapChainBuffer;

        winrt::com_ptr<ID3D12CommandAllocator> BeginCmdAlloc;
        winrt::com_ptr<ID3D12CommandAllocator> DrawCmdAlloc;
        winrt::com_ptr<ID3D12CommandAllocator> GuiCmdAlloc;
        winrt::com_ptr<ID3D12CommandAllocator> PresentCmdAlloc;

        D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle;

        uint64_t FenceWaitValue = 0;
    };

    Frame m_frames[NUM_FRAMES];

    winrt::com_ptr<ID3D12RootSignature> m_rootSig;
    winrt::com_ptr<ID3D12PipelineState> m_pipeline;

    winrt::com_ptr<ID3D12DescriptorHeap> m_rtvHeap;
    uint32_t m_rtvHandleSize = 0;

    winrt::com_ptr<ID3D12DescriptorHeap> m_dsvHeap;
    uint32_t m_dsvHandleSize = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle;

    winrt::com_ptr<ID3D12DescriptorHeap> m_guiSrvHeap;

    winrt::com_ptr<ID3D12Resource> m_depthTexture;

    winrt::com_ptr<ID3D12Resource> m_constantBuffer;

    std::unique_ptr<DebugPass> m_debugPass;

    int m_currentFrame = 0;

    InputHandle<bool> m_upKeyDown;
    InputHandle<bool> m_downKeyDown;
    InputHandle<bool> m_leftKeyDown;
    InputHandle<bool> m_rightKeyDown;

    glm::vec3 m_cameraPos;

    std::optional<int> m_prevMouseX;
    std::optional<int> m_prevMouseY;

    float m_cameraYaw = 0.f;
    float m_cameraPitch = 0.f;

    glm::mat4 m_projMat;

    struct Constants
    {
        glm::mat4 WorldViewProjMatrix;
        glm::vec4 LightPos;
    };

    Constants* m_constantsPtr = nullptr;

    Model m_model;
};
