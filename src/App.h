#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wil/resource.h>
#include <winrt/base.h>

class App
{
public:
    App(HWND hwnd);

private:
    void CreateDevice();

    void CreateCmdQueueAndSwapChain();

    void CreateCommandList();

    void CreatePipelineState();

    HWND m_hwnd;

    int m_numFrames = 2;

    winrt::com_ptr<IDXGIFactory6> m_factory;
    winrt::com_ptr<ID3D12Device> m_device;

    winrt::com_ptr<ID3D12CommandQueue> m_cmdQueue;
    winrt::com_ptr<IDXGISwapChain3> m_swapChain;

    winrt::com_ptr<ID3D12CommandAllocator> m_cmdAlloc;
    winrt::com_ptr<ID3D12GraphicsCommandList> m_cmdList;

    winrt::com_ptr<ID3D12Fence> m_fence;
    uint64_t m_fenceValue = 0;

    wil::unique_handle m_fenceEvent;

    winrt::com_ptr<ID3D12RootSignature> m_rootSig;
    winrt::com_ptr<ID3D12PipelineState> m_pipeline;
};
