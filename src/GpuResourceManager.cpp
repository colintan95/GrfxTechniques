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

namespace
{

struct GltfAccessor
{
    int BufferView;
    int ByteOffset;
    int ComponentType;
    int Count;
    std::string Type;
};

struct GltfBufferView
{
    int Buffer;
    int ByteOffset;
    int ByteLength;
    int Target;
};

} // namespace

static void ParseAccessorAndBufferView(int accessorIdx, const json& gltfJson,
                                       GltfAccessor* accessor, GltfBufferView* bufferView)
{
    const auto& accessorJson = gltfJson["accessors"][accessorIdx];

    accessor->BufferView = accessorJson["bufferView"];
    accessor->ByteOffset = accessorJson["byteOffset"];
    accessor->ComponentType = accessorJson["componentType"];
    accessor->Count = accessorJson["count"];
    accessor->Type = accessorJson["type"];

    const auto& bufferViewJson = gltfJson["bufferViews"][accessor->BufferView];

    bufferView->Buffer = bufferViewJson["buffer"];
    bufferView->ByteOffset = bufferViewJson["byteOffset"];
    bufferView->ByteLength = bufferViewJson["byteLength"];
    bufferView->Target = bufferViewJson["target"];
}

static void CreateVertexBufferView(int accessorIdx, const json& gltfJson,
                                   const std::vector<com_ptr<ID3D12Resource>>& buffers,
                                   D3D12_VERTEX_BUFFER_VIEW* vertexBufferView)
{
    GltfAccessor accessor{};
    GltfBufferView bufferView{};

    ParseAccessorAndBufferView(accessorIdx, gltfJson, &accessor, &bufferView);

    int componentSize = 0;

    switch (accessor.ComponentType)
    {
        case 5126:
            componentSize = sizeof(float);
            break;
        default:
            throw std::runtime_error("Unsupported component type.");
    }

    int numComponents = 0;

    if (accessor.Type == "VEC3")
    {
        numComponents = 3;
    }
    else
    {
        throw std::runtime_error("Unsupported accessor type.");
    }

    int stride = componentSize * numComponents;

    vertexBufferView->BufferLocation = buffers[bufferView.Buffer]->GetGPUVirtualAddress() +
        accessor.ByteOffset + bufferView.ByteOffset;
    vertexBufferView->SizeInBytes = stride * accessor.Count;
    vertexBufferView->StrideInBytes = stride;
}

static void CreateIndexBufferView(int accessorIdx, const json& gltfJson,
                                  const std::vector<com_ptr<ID3D12Resource>>& buffers,
                                  D3D12_INDEX_BUFFER_VIEW* indexBufferView)
{
    GltfAccessor accessor{};
    GltfBufferView bufferView{};

    ParseAccessorAndBufferView(accessorIdx, gltfJson, &accessor, &bufferView);

    if (accessor.ComponentType != 5123 || accessor.Type != "SCALAR")
        throw std::runtime_error("Unsupported index type.");

    indexBufferView->BufferLocation = buffers[bufferView.Buffer]->GetGPUVirtualAddress() +
        accessor.ByteOffset + bufferView.ByteOffset;
    indexBufferView->SizeInBytes = sizeof(uint16_t) * accessor.Count;
    indexBufferView->Format = DXGI_FORMAT_R16_UINT;
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
