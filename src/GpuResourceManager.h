#pragma once

#include "Model.h"

#include <d3d12.h>
#include <winrt/base.h>

#include <filesystem>
#include <vector>

class GpuResourceManager
{
public:
    GpuResourceManager(ID3D12Device* device);

    void LoadGltfModel(std::filesystem::path path, Model& model);

private:
    winrt::com_ptr<ID3D12Resource> LoadBufferToGpu(std::filesystem::path path);

    void ExecuteCommandListSync();

    ID3D12Device* m_device;

    winrt::com_ptr<ID3D12CommandQueue> m_copyQueue;
    winrt::com_ptr<ID3D12CommandAllocator> m_cmdAllocator;
    winrt::com_ptr<ID3D12GraphicsCommandList> m_cmdList;

    winrt::com_ptr<ID3D12Fence> m_fence;
    uint64_t m_fenceValue = 0;

    std::vector<winrt::com_ptr<ID3D12Resource>> m_resources;
};
