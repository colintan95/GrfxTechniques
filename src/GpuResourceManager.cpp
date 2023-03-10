#include "GpuResourceManager.h"

#include <d3dx12.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <variant>

namespace fs = std::filesystem;

using nlohmann::json;
using winrt::check_hresult;
using winrt::com_ptr;

GpuResourceManager::GpuResourceManager(ID3D12Device* device)
    : m_device(device)
{
    static constexpr auto cmdListType = D3D12_COMMAND_LIST_TYPE_COPY;

    D3D12_COMMAND_QUEUE_DESC copyQueueDesc{};
    copyQueueDesc.Type = cmdListType;

    check_hresult(m_device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(m_copyQueue.put())));

    check_hresult(m_device->CreateCommandAllocator(cmdListType,
                                                   IID_PPV_ARGS(m_cmdAllocator.put())));

    check_hresult(m_device->CreateCommandList(0, cmdListType, m_cmdAllocator.get(), nullptr,
                                              IID_PPV_ARGS(m_cmdList.put())));
    check_hresult(m_cmdList->Close());

    check_hresult(m_device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE,
                                        IID_PPV_ARGS(m_fence.put())));
    ++m_fenceValue;
}

struct BufferViewInfo
{
    int BufferIdx;
    int ByteOffset;
    int ComponentType;
    int Count;
    std::string AccessorType;
};

static void ParseBufferViewInfo(int accessorIdx, const json& gltfJson, BufferViewInfo* viewInfo)
{
    const auto& accessorJson = gltfJson["accessors"][accessorIdx];

    int bufferViewIdx = accessorJson["bufferView"];

    const auto& bufferViewJson = gltfJson["bufferViews"][bufferViewIdx];

    int bufferIdx = bufferViewJson["buffer"];

    viewInfo->BufferIdx = bufferIdx;
    viewInfo->ByteOffset = accessorJson["byteOffset"] + bufferViewJson["byteOffset"];
    viewInfo->ComponentType = accessorJson["componentType"];
    viewInfo->Count = accessorJson["count"];
    viewInfo->AccessorType = accessorJson["type"];
}

static void CreateVertexBufferView(int accessorIdx, const json& gltfJson,
                                   const std::vector<com_ptr<ID3D12Resource>>& buffers,
                                   D3D12_VERTEX_BUFFER_VIEW* view)
{
    BufferViewInfo viewInfo;
    ParseBufferViewInfo(accessorIdx, gltfJson, &viewInfo);

    int componentSize = 0;

    switch (viewInfo.ComponentType)
    {
        case 5126:
            componentSize = sizeof(float);
            break;
        default:
            throw std::runtime_error("Unsupported component type.");
    }

    int numComponents = 0;

    if (viewInfo.AccessorType == "VEC3")
    {
        numComponents = 3;
    }
    else
    {
        throw std::runtime_error("Unsupported accessor type.");
    }

    int stride = componentSize * numComponents;

    view->BufferLocation = buffers[viewInfo.BufferIdx]->GetGPUVirtualAddress() +
        viewInfo.ByteOffset;
    view->SizeInBytes = stride * viewInfo.Count;
    view->StrideInBytes = stride;
}

static void CreateIndexBufferView(int accessorIdx, const json& gltfJson,
                                  const std::vector<com_ptr<ID3D12Resource>>& buffers,
                                  D3D12_INDEX_BUFFER_VIEW* view)
{
    BufferViewInfo viewInfo;
    ParseBufferViewInfo(accessorIdx, gltfJson, &viewInfo);

    if (viewInfo.ComponentType != 5123 || viewInfo.AccessorType != "SCALAR")
        throw std::runtime_error("Unsupported index type.");

    view->BufferLocation = buffers[viewInfo.BufferIdx]->GetGPUVirtualAddress() +
        viewInfo.ByteOffset;
    view->SizeInBytes = sizeof(uint16_t) * viewInfo.Count;
    view->Format = DXGI_FORMAT_R16_UINT;
}

void GpuResourceManager::LoadGltfModel(fs::path path, Model* model)
{
    std::ifstream strm(path);
    if (!strm.is_open())
        throw std::runtime_error("Could not open file.");

    json gltfJson = json::parse(strm);

    std::vector<com_ptr<ID3D12Resource>> buffers;

    for (const auto& bufferJson : gltfJson["buffers"])
    {
        buffers.push_back(
            LoadBufferToGpu(path.parent_path() / bufferJson["uri"].get<std::string>()));
    }

    for (const auto& meshJson : gltfJson["meshes"])
    {
        Mesh mesh{};

        for (const auto& primJson : meshJson["primitives"])
        {
            Primitive prim{};

            int posAccessorIdx = primJson["attributes"]["POSITION"];
            CreateVertexBufferView(posAccessorIdx, gltfJson, buffers, &prim.Positions);

            int normalsAccessorIdx = primJson["attributes"]["NORMAL"];
            CreateVertexBufferView(normalsAccessorIdx, gltfJson, buffers, &prim.Normals);

            int indicesAccessorIdx = primJson["indices"];
            CreateIndexBufferView(indicesAccessorIdx, gltfJson, buffers, &prim.Indices);

            prim.VertexCount = gltfJson["accessors"][indicesAccessorIdx]["count"];

            mesh.Primitives.push_back(std::move(prim));
        }

        model->Meshes.push_back(std::move(mesh));
    }
}


com_ptr<ID3D12Resource> GpuResourceManager::LoadBufferToGpu(std::span<const std::byte> data)
{
    com_ptr<ID3D12Resource> uploadBuffer;

    {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(data.size());
        check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                                        &bufferDesc,
                                                        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                        IID_PPV_ARGS(uploadBuffer.put())));
    }

    std::byte* uploadPtr = nullptr;
    check_hresult(uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&uploadPtr)));

    memcpy(uploadPtr, data.data(), data.size());

    uploadBuffer->Unmap(0, nullptr);

    com_ptr<ID3D12Resource> resource;

    {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(data.size());
        check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                                        &bufferDesc, D3D12_RESOURCE_STATE_COMMON,
                                                        nullptr, IID_PPV_ARGS(resource.put())));
    }

    check_hresult(m_cmdAllocator->Reset());
    check_hresult(m_cmdList->Reset(m_cmdAllocator.get(), nullptr));

    m_cmdList->CopyBufferRegion(resource.get(), 0, uploadBuffer.get(), 0, data.size());

    check_hresult(m_cmdList->Close());

    ExecuteCommandListSync();

    m_resources.push_back(resource);

    return resource;
}

com_ptr<ID3D12Resource> GpuResourceManager::LoadBufferToGpu(fs::path path)
{
    std::ifstream strm(path, std::ios::binary);
    if (!strm.is_open())
        throw std::runtime_error("Could not open file.");

    std::vector<std::byte> data(fs::file_size(path));

    strm.read(reinterpret_cast<char*>(data.data()), data.size());

    return LoadBufferToGpu(data);
}

void GpuResourceManager::ExecuteCommandListSync()
{
    ID3D12CommandList* cmdLists[] = { m_cmdList.get() };
    m_copyQueue->ExecuteCommandLists(static_cast<uint32_t>(std::size(cmdLists)), cmdLists);

    check_hresult(m_copyQueue->Signal(m_fence.get(), m_fenceValue));

    while (m_fence->GetCompletedValue() < m_fenceValue)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ++m_fenceValue;
}
