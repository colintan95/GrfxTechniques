#include "DebugPass.h"

#include "gen/DebugPS.h"
#include "gen/DebugVS.h"
#include "Utils.h"

#include <d3dx12.h>
#include <glm/gtc/matrix_transform.hpp>

using winrt::check_hresult;
using winrt::com_ptr;

DebugPass::DebugPass(Scene* scene, ID3D12Device* device, GpuResourceManager* resourceManager)
    : m_scene(scene), m_device(device), m_resourceManager(resourceManager)
{
    CreatePipelineState();

    CreateVertexBuffers();

    CreateConstantBuffer();
}

void DebugPass::CreatePipelineState()
{
    CD3DX12_DESCRIPTOR_RANGE1 range{};
    range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParam{};
    rootParam.InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
                                       D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init_1_1(1, &rootParam, 0, nullptr,
                         D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    com_ptr<ID3DBlob> signatureBlob;
    com_ptr<ID3DBlob> errorBlob;
    check_hresult(D3D12SerializeVersionedRootSignature(&rootSigDesc, signatureBlob.put(),
                                                       errorBlob.put()));
    check_hresult(m_device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
                                                signatureBlob->GetBufferSize(),
                                                IID_PPV_ARGS(m_rootSig.put())));

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    D3D12_RASTERIZER_DESC rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
    pipelineDesc.InputLayout = inputLayoutDesc;
    pipelineDesc.pRootSignature = m_rootSig.get();
    pipelineDesc.VS = CD3DX12_SHADER_BYTECODE(g_debugVS, ARRAYSIZE(g_debugVS));
    pipelineDesc.PS = CD3DX12_SHADER_BYTECODE(g_debugPS, ARRAYSIZE(g_debugPS));;
    pipelineDesc.RasterizerState = rasterizer;
    pipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pipelineDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pipelineDesc.SampleMask = UINT_MAX;
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipelineDesc.SampleDesc.Count = 1;

    check_hresult(m_device->CreateGraphicsPipelineState(&pipelineDesc,
                                                        IID_PPV_ARGS(m_pipeline.put())));
}

namespace
{

struct CubeData
{
    std::vector<float> Positions;
    std::vector<uint16_t> Indices;

    int VertexCount = 0;
};

} // namespace

CubeData GetCubeData(float width)
{
    CubeData data{};

    float h = width / 2.f;

    data.Positions = {
        -h, h, h,
        -h, h, -h,
        -h, -h, -h,
        -h, -h, h,
        h, h, -h,
        h, h, h,
        h, -h, h,
        h, -h, -h,
    };

    data.Indices = {
        0, 1, 2,
        2, 3, 0, // -x face
        4, 5, 6,
        6, 7, 4, // +x face
        2, 7, 6,
        6, 3, 2, // -y face
        0, 5, 4,
        4, 1, 0, // +y face
        1, 4, 7,
        7, 2, 1, // -z face
        0, 3, 6,
        6, 5, 0  // +z face
    };

    data.VertexCount = static_cast<int>(data.Indices.size());

    return data;
}

void DebugPass::CreateVertexBuffers()
{
    CubeData cubeData = GetCubeData(0.5f);

    m_positionBufferSize = cubeData.Positions.size() * sizeof(float);
    m_indexBufferSize = cubeData.Indices.size() * sizeof(uint16_t);

    m_vertexCount = cubeData.VertexCount;

    m_positionBuffer = m_resourceManager->LoadBufferToGpu(
        std::as_bytes(std::span(cubeData.Positions)));

    m_indexBuffer = m_resourceManager->LoadBufferToGpu(std::as_bytes(std::span(cubeData.Indices)));
}

void DebugPass::CreateConstantBuffer()
{
    size_t bufferSize = utils::Align(sizeof(Constants),
                                     D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                                    &resourceDesc,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                    IID_PPV_ARGS(m_constantBuffer.put())));

    check_hresult(m_constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_constantsPtr)));
}

void DebugPass::RecordCommands(const glm::mat4& viewProjMat, ID3D12GraphicsCommandList* cmdList)
{
    glm::mat4 modelMat = glm::translate(glm::mat4(1.f), m_scene->LightPos) *
        glm::scale(glm::mat4(1.f), glm::vec3(0.1f));

    m_constantsPtr->WorldViewProjMatrix = viewProjMat * modelMat;

    cmdList->SetPipelineState(m_pipeline.get());
    cmdList->SetGraphicsRootSignature(m_rootSig.get());

    cmdList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW posBufferView{};
    posBufferView.BufferLocation = m_positionBuffer->GetGPUVirtualAddress();
    posBufferView.SizeInBytes = static_cast<UINT>(m_positionBufferSize);
    posBufferView.StrideInBytes = sizeof(float) * 3;

    cmdList->IASetVertexBuffers(0, 1, &posBufferView);

    D3D12_INDEX_BUFFER_VIEW indexBufferView{};
    indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    indexBufferView.SizeInBytes = static_cast<UINT>(m_indexBufferSize);
    indexBufferView.Format = DXGI_FORMAT_R16_UINT;

    cmdList->IASetIndexBuffer(&indexBufferView);

    cmdList->DrawIndexedInstanced(m_vertexCount, 1, 0, 0, 0);
}
