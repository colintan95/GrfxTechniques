#pragma once

#include "Model.h"

#include <d3d12.h>
#include <d3dx12.h>
#include <wincodec.h>
#include <winrt/base.h>

#include <filesystem>
#include <span>
#include <vector>

class GpuResourceManager
{
public:
    GpuResourceManager(ID3D12Device* device);

    void LoadGltfModel(std::filesystem::path path, Model* model);

    winrt::com_ptr<ID3D12Resource> LoadBufferToGpu(std::span<const std::byte> data);
    winrt::com_ptr<ID3D12Resource> LoadBufferToGpu(std::filesystem::path path);

    TextureId LoadTextureToGpu(std::filesystem::path path);

    ID3D12DescriptorHeap* GetTextureSrvHeap();

    D3D12_GPU_DESCRIPTOR_HANDLE GetTextureSrvHandle(TextureId id);

private:
    void ExecuteCommandListSync();

    ID3D12Device* m_device;

    winrt::com_ptr<ID3D12CommandQueue> m_copyQueue;
    winrt::com_ptr<ID3D12CommandAllocator> m_cmdAllocator;
    winrt::com_ptr<ID3D12GraphicsCommandList> m_cmdList;

    winrt::com_ptr<ID3D12Fence> m_fence;
    uint64_t m_fenceValue = 0;

    std::vector<winrt::com_ptr<ID3D12Resource>> m_buffers;

    std::vector<winrt::com_ptr<ID3D12Resource>> m_textures;

    winrt::com_ptr<IWICImagingFactory> m_wicFactory;

    winrt::com_ptr<ID3D12DescriptorHeap> m_descriptorHeap;
    uint32_t m_descriptorHandleSize = 0;

    CD3DX12_CPU_DESCRIPTOR_HANDLE m_currentCpuDescriptorHandle;
    CD3DX12_GPU_DESCRIPTOR_HANDLE m_currentGpuDescriptorHandle;

    std::unordered_map<TextureId, D3D12_GPU_DESCRIPTOR_HANDLE> m_textureSrvHandles;
};
