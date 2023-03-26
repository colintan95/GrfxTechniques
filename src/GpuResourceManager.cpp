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

    check_hresult(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(m_wicFactory.put())));

    static constexpr int maxDescriptors = 128;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = maxDescriptors;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    check_hresult(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_descriptorHeap.put())));
    m_descriptorHandleSize = device->GetDescriptorHandleIncrementSize(heapDesc.Type);

    m_currentCpuDescriptorHandle = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_currentGpuDescriptorHandle = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
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
        case 5123:
            componentSize = sizeof(uint16_t);
            break;
        case 5126:
            componentSize = sizeof(float);
            break;
        default:
            throw std::runtime_error("Unsupported component type.");
    }

    int numComponents = 0;

    if (accessor.Type == "SCALAR")
    {
        numComponents = 1;
    }
    else if (accessor.Type == "VEC2")
    {
        numComponents = 2;
    }
    else if (accessor.Type == "VEC3")
    {
        numComponents = 3;
    }
    else if (accessor.Type == "VEC4")
    {
        numComponents = 4;
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

    std::vector<TextureId> textureIds;

    for (const auto& imageJson : gltfJson["images"])
    {
        textureIds.push_back(
            LoadTextureToGpu(path.parent_path() / imageJson["uri"].get<std::string>()));
    }

    for (const auto& materialJson : gltfJson["materials"])
    {
        Material material{};

        const auto& pbrJson = materialJson["pbrMetallicRoughness"];

        if (pbrJson.contains("baseColorFactor"))
        {
            const auto& factor = pbrJson["baseColorFactor"];
            material.BaseColorFactor = glm::vec4(factor[0], factor[1], factor[2], factor[3]);
        }
        else
        {
            material.BaseColorFactor = glm::vec4(1.f, 1.f, 1.f, 1.f);
        }

        if (pbrJson.contains("metallicFactor"))
        {
            float factor = pbrJson["metallicFactor"];
            material.MetallicFactor = factor;
        }

        if (pbrJson.contains("roughnessFactor"))
        {
            float factor = pbrJson["roughnessFactor"];
            material.RoughnessFactor = factor;
        }

        if (pbrJson.contains("baseColorTexture"))
        {
            int idx = pbrJson["baseColorTexture"]["index"];
            material.BaseColorTextureId = textureIds[idx];
        }

        if (pbrJson.contains("metallicRoughnessTexture"))
        {
            int idx = pbrJson["metallicRoughnessTexture"]["index"];
            material.RoughnessTextureId = textureIds[idx];
        }

        if (pbrJson.contains("normalTexture"))
        {
            int idx = pbrJson["normalTexture"]["index"];
            material.NormalTextureId = textureIds[idx];
        }

        model->Materials.push_back(std::move(material));
    }

    for (const auto& meshJson : gltfJson["meshes"])
    {
        Mesh mesh{};

        for (const auto& primJson : meshJson["primitives"])
        {
            Primitive prim{};

            const auto& attrJson = primJson["attributes"];

            int posAccessorIdx = attrJson["POSITION"];
            CreateVertexBufferView(posAccessorIdx, gltfJson, buffers, &prim.Positions);

            int normalsAccessorIdx = attrJson["NORMAL"];
            CreateVertexBufferView(normalsAccessorIdx, gltfJson, buffers, &prim.Normals);

            if (attrJson.contains("TEXCOORD_0"))
            {
                int accessorIdx = attrJson["TEXCOORD_0"];
                CreateVertexBufferView(accessorIdx, gltfJson, buffers, &prim.TexCoords);
            }

            if (attrJson.contains("TANGENT"))
            {
                int accessorIdx = attrJson["TANGENT"];
                CreateVertexBufferView(accessorIdx, gltfJson, buffers, &prim.Tangents);
            }

            int indicesAccessorIdx = primJson["indices"];
            CreateIndexBufferView(indicesAccessorIdx, gltfJson, buffers, &prim.Indices);

            if (primJson.contains("material"))
            {
                prim.MaterialIdx = primJson["material"];
            }

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

    m_buffers.push_back(resource);

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

TextureId GpuResourceManager::LoadTextureToGpu(fs::path path)
{
    com_ptr<IWICBitmapDecoder> decoder;
    check_hresult(m_wicFactory->CreateDecoderFromFilename(path.wstring().c_str(), nullptr,
                                                          GENERIC_READ,
                                                          WICDecodeMetadataCacheOnLoad,
                                                          decoder.put()));

    com_ptr<IWICBitmapFrameDecode> decoderFrame;
    check_hresult(decoder->GetFrame(0, decoderFrame.put()));

    WICPixelFormatGUID srcFormat;
    check_hresult(decoderFrame->GetPixelFormat(&srcFormat));
    assert(srcFormat == GUID_WICPixelFormat24bppBGR || srcFormat == GUID_WICPixelFormat32bppBGRA);

    com_ptr<IWICFormatConverter> formatConverter;
    check_hresult(m_wicFactory->CreateFormatConverter(formatConverter.put()));

    static WICPixelFormatGUID dstFormat = GUID_WICPixelFormat32bppRGBA;

    BOOL canConvert = false;
    check_hresult(formatConverter->CanConvert(srcFormat, dstFormat, &canConvert));
    assert(canConvert);

    check_hresult(formatConverter->Initialize(decoderFrame.get(), dstFormat,
                                              WICBitmapDitherTypeNone, nullptr, 0.f,
                                              WICBitmapPaletteTypeCustom));

    com_ptr<IWICBitmap> bitmap;
    check_hresult(m_wicFactory->CreateBitmapFromSource(formatConverter.get(), WICBitmapCacheOnLoad,
                                                       bitmap.put()));

    com_ptr<IWICBitmapLock> bitmapLock;
    check_hresult(bitmap->Lock(nullptr, WICBitmapLockRead, bitmapLock.put()));

    uint32_t bitmapBufferSize = 0;
    BYTE* bitmapBuffer = nullptr;

    check_hresult(bitmapLock->GetDataPointer(&bitmapBufferSize, &bitmapBuffer));

    uint32_t bitmapWidth = 0;
    uint32_t bitmapHeight = 0;
    check_hresult(formatConverter->GetSize(&bitmapWidth, &bitmapHeight));

    CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
                                                                     bitmapWidth, bitmapHeight);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT copySrcLayout{};
    uint64_t uploadBufferSize = 0;
    m_device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &copySrcLayout, nullptr, nullptr,
                                    &uploadBufferSize);

    com_ptr<ID3D12Resource> uploadBuffer;

    {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
        check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                                        &bufferDesc,
                                                        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                        IID_PPV_ARGS(uploadBuffer.put())));
    }

    std::byte* uploadPtr = nullptr;
    check_hresult(uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&uploadPtr)));

    std::byte* bitmapPtr = reinterpret_cast<std::byte*>(bitmapBuffer);

    for (size_t i = 0; i < copySrcLayout.Footprint.Height; ++i)
    {
        memcpy(uploadPtr, bitmapPtr, bitmapWidth * 4);

        uploadPtr += copySrcLayout.Footprint.RowPitch;
        bitmapPtr += bitmapWidth * 4;
    }

    uploadBuffer->Unmap(0, nullptr);

    com_ptr<ID3D12Resource> resource;

    {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
        check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                                        &textureDesc, D3D12_RESOURCE_STATE_COMMON,
                                                        nullptr, IID_PPV_ARGS(resource.put())));
    }

    check_hresult(m_cmdAllocator->Reset());
    check_hresult(m_cmdList->Reset(m_cmdAllocator.get(), nullptr));

    D3D12_TEXTURE_COPY_LOCATION copySrc{};
    copySrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    copySrc.pResource = uploadBuffer.get();
    copySrc.PlacedFootprint = copySrcLayout;

    D3D12_TEXTURE_COPY_LOCATION copyDst;
    copyDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    copyDst.pResource = resource.get();
    copyDst.SubresourceIndex = 0;

    m_cmdList->CopyTextureRegion(&copyDst, 0, 0, 0, &copySrc, nullptr);

    check_hresult(m_cmdList->Close());

    ExecuteCommandListSync();

    int textureId = static_cast<int>(m_textures.size());

    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = m_currentCpuDescriptorHandle;
    m_currentCpuDescriptorHandle.Offset(1, m_descriptorHandleSize);

    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = m_currentGpuDescriptorHandle;
    m_currentGpuDescriptorHandle.Offset(1, m_descriptorHandleSize);

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;
    srv_desc.Texture2D.MostDetailedMip = 0;

    m_device->CreateShaderResourceView(resource.get(), &srv_desc, srvCpuHandle);

    m_textureSrvHandles[textureId] = srvGpuHandle;

    m_textures.push_back(resource);

    return textureId;
}

ID3D12DescriptorHeap* GpuResourceManager::GetTextureSrvHeap()
{
    return m_descriptorHeap.get();
}

D3D12_GPU_DESCRIPTOR_HANDLE GpuResourceManager::GetTextureSrvHandle(TextureId id)
{
    return m_textureSrvHandles.at(id);
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
