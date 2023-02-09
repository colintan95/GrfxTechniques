#include "App.h"

#include "gen/ShaderPS.h"
#include "gen/ShaderVS.h"

#include <d3dx12.h>
#include <DirectXMath.h>

#include <vector>

using namespace DirectX;

using winrt::check_bool;
using winrt::check_hresult;
using winrt::com_ptr;

App::App(HWND hwnd, InputManager* inputManager)
    : m_hwnd(hwnd), m_inputManager(inputManager)
{
    CreateDevice();

    CreateCmdQueueAndSwapChain();

    CreateCommandList();

    CreatePipelineState();

    CreateDescriptorHeaps();

    CreateDepthTexture();

    CreateVertexBuffers();

    CreateConstantBuffer();

    m_leftKeyDown = m_inputManager->AddKeyHoldListener(0x41);
    m_rightKeyDown = m_inputManager->AddKeyHoldListener(0x44);
}

void App::CreateDevice()
{
    com_ptr<ID3D12Debug1> debugController;
    check_hresult(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.put())));

    debugController->EnableDebugLayer();
    debugController->SetEnableGPUBasedValidation(true);

    check_hresult(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(m_factory.put())));

    static constexpr D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_1;

    com_ptr<IDXGIAdapter1> adapter;

    for (uint32_t adapterIdx = 0;
         m_factory->EnumAdapterByGpuPreference(adapterIdx, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                               IID_PPV_ARGS(adapter.put())) != DXGI_ERROR_NOT_FOUND;
        ++adapterIdx)
    {
        if (SUCCEEDED(D3D12CreateDevice(adapter.get(), featureLevel, _uuidof(ID3D12Device),
                                        nullptr)))
            break;
    }

    check_hresult(D3D12CreateDevice(adapter.get(), featureLevel, IID_PPV_ARGS(m_device.put())));
}

void App::CreateCmdQueueAndSwapChain()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    check_hresult(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_cmdQueue.put())));

    RECT clientRect{};
    check_bool(GetClientRect(m_hwnd, &clientRect));

    m_windowWidth = clientRect.right;
    m_windowHeight = clientRect.bottom;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.BufferCount = NUM_FRAMES;
    swapChainDesc.Width = m_windowWidth;
    swapChainDesc.Height = m_windowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    com_ptr<IDXGISwapChain1> swapChain;
    check_hresult(m_factory->CreateSwapChainForHwnd(m_cmdQueue.get(), m_hwnd, &swapChainDesc,
                                                    nullptr, nullptr, swapChain.put()));
    swapChain.as(m_swapChain);

    for (int i = 0; i < _countof(m_frames); ++i)
    {
        check_hresult(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_frames[i].SwapChainBuffer)));
    }

    m_viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(m_windowWidth),
                                  static_cast<float>(m_windowHeight));
    m_scissorRect = CD3DX12_RECT(0, 0, m_windowWidth, m_windowHeight);
}

void App::CreateCommandList()
{
    check_hresult(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                   IID_PPV_ARGS(m_cmdAlloc.put())));

    for (int i = 0; i < _countof(m_frames); ++i)
    {
        check_hresult(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                       IID_PPV_ARGS(m_frames[i].CmdAlloc.put())));
    }

    check_hresult(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAlloc.get(),
                                                nullptr, IID_PPV_ARGS(m_cmdList.put())));
    m_cmdList->Close();

    check_hresult(m_device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE,
                                        IID_PPV_ARGS(m_fence.put())));
    ++m_fenceValue;

    m_fenceEvent.reset(CreateEvent(nullptr, false, false, nullptr));
}

void App::CreatePipelineState()
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
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
    pipelineDesc.InputLayout = inputLayoutDesc;
    pipelineDesc.pRootSignature = m_rootSig.get();
    pipelineDesc.VS = CD3DX12_SHADER_BYTECODE(g_shaderVS, ARRAYSIZE(g_shaderVS));
    pipelineDesc.PS = CD3DX12_SHADER_BYTECODE(g_shaderPS, ARRAYSIZE(g_shaderPS));;
    pipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
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

void App::CreateDescriptorHeaps()
{
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = _countof(m_frames);
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

        check_hresult(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_rtvHeap.put())));
        m_rtvHandleSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        for (int i = 0; i < _countof(m_frames); ++i)
        {
            m_device->CreateRenderTargetView(m_frames[i].SwapChainBuffer.get(), nullptr, handle);
            m_frames[i].RtvHandle = handle;

            handle.Offset(m_rtvHandleSize);
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = 1;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

        check_hresult(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_dsvHeap.put())));
        m_dsvHandleSize = m_device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        m_dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    }
}

void App::CreateDepthTexture()
{
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC resourceDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_windowWidth, m_windowHeight, 1, 0, 1,
                                     0,
                                     D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL |
                                     D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
    CD3DX12_CLEAR_VALUE clearValue(DXGI_FORMAT_D32_FLOAT, 1.f, 0);

    check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                    D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
                                                    IID_PPV_ARGS(m_depthTexture.put())));

    D3D12_DEPTH_STENCIL_VIEW_DESC depthViewDesc{};
    depthViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    m_device->CreateDepthStencilView(m_depthTexture.get(), &depthViewDesc, m_dsvHandle);
}

struct CubeData
{
    std::vector<float> Positions;
    std::vector<uint16_t> Indices;

    int VertexCount = 0;
};

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

void App::CreateVertexBuffers()
{
    CubeData cubeData = GetCubeData(0.5f);

    m_positionBufferSize = cubeData.Positions.size() * sizeof(float);
    m_indexBufferSize = cubeData.Indices.size() * sizeof(uint16_t);

    m_vertexCount = cubeData.VertexCount;

    com_ptr<ID3D12Resource> uploadBuffer;

    {
        static constexpr int uploadBufferSize = 8 * 1024 * 1024;

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                                        &resourceDesc,
                                                        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                        IID_PPV_ARGS(uploadBuffer.put())));
    }

    {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_positionBufferSize);

        check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                                        &resourceDesc,
                                                        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                        IID_PPV_ARGS(m_positionBuffer.put())));
    }

    {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_indexBufferSize);

        check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                                        &resourceDesc,
                                                        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                        IID_PPV_ARGS(m_indexBuffer.put())));
    }

    std::byte* uploadPtr = nullptr;

    check_hresult(uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&uploadPtr)));

    memcpy(uploadPtr, cubeData.Positions.data(), m_positionBufferSize);
    uploadPtr += m_positionBufferSize;

    memcpy(uploadPtr, cubeData.Indices.data(), m_indexBufferSize);
    uploadPtr += m_indexBufferSize;

    uploadBuffer->Unmap(0, nullptr);

    m_cmdList->Reset(m_cmdAlloc.get(), nullptr);

    m_cmdList->CopyBufferRegion(m_positionBuffer.get(), 0, uploadBuffer.get(), 0,
                                m_positionBufferSize);
    m_cmdList->CopyBufferRegion(m_indexBuffer.get(), 0, uploadBuffer.get(), m_positionBufferSize,
                                m_indexBufferSize);

    auto posBufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_positionBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    auto indexBufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_indexBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    D3D12_RESOURCE_BARRIER barriers[] = {posBufferBarrier, indexBufferBarrier};

    m_cmdList->ResourceBarrier(_countof(barriers), barriers);

    ExecuteAndWait();
}

static size_t Align(size_t value, size_t alignment)
{
    return ((value - 1) / alignment + 1) * alignment;
}

struct Constants
{
    XMFLOAT4X4 WorldViewProjMatrix;
};

void App::CreateConstantBuffer()
{
    size_t bufferSize = Align(sizeof(Constants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                                    &resourceDesc,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                    IID_PPV_ARGS(m_constantBuffer.put())));

    XMMATRIX viewMat = XMMatrixTranslation(0.f, 0.f, 2.f);

    XMMATRIX projMat = XMMatrixPerspectiveFovLH(
        XM_PI / 4.f, static_cast<float>(m_windowWidth) / static_cast<float>(m_windowHeight), 0.1f,
        1000.f);

    Constants* constantsPtr = nullptr;

    check_hresult(m_constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&constantsPtr)));

    XMStoreFloat4x4(&constantsPtr->WorldViewProjMatrix, XMMatrixTranspose(viewMat * projMat));

    m_constantBuffer->Unmap(0, nullptr);
}

void App::Render()
{
    check_hresult(m_frames[m_currentFrame].CmdAlloc->Reset());
    check_hresult(m_cmdList->Reset(m_frames[m_currentFrame].CmdAlloc.get(), nullptr));

    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_frames[m_currentFrame].SwapChainBuffer.get(), D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        m_cmdList->ResourceBarrier(1, &barrier);
    }

    m_cmdList->SetPipelineState(m_pipeline.get());
    m_cmdList->SetGraphicsRootSignature(m_rootSig.get());

    m_cmdList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());

    m_cmdList->RSSetViewports(1, &m_viewport);
    m_cmdList->RSSetScissorRects(1, &m_scissorRect);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_frames[m_currentFrame].RtvHandle;

    m_cmdList->OMSetRenderTargets(1, &rtvHandle, false, &m_dsvHandle);

    static constexpr float clearColor[] = { 0.f, 0.f, 0.f, 1.f };

    m_cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_cmdList->ClearDepthStencilView(m_dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

    m_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW posBufferView{};
    posBufferView.BufferLocation = m_positionBuffer->GetGPUVirtualAddress();
    posBufferView.SizeInBytes = static_cast<UINT>(m_positionBufferSize);
    posBufferView.StrideInBytes = sizeof(float) * 3;

    m_cmdList->IASetVertexBuffers(0, 1, &posBufferView);

    D3D12_INDEX_BUFFER_VIEW indexBufferView{};
    indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    indexBufferView.SizeInBytes = static_cast<UINT>(m_indexBufferSize);
    indexBufferView.Format = DXGI_FORMAT_R16_UINT;

    m_cmdList->IASetIndexBuffer(&indexBufferView);

    m_cmdList->DrawIndexedInstanced(m_vertexCount, 1, 0, 0, 0);

    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_frames[m_currentFrame].SwapChainBuffer.get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);

        m_cmdList->ResourceBarrier(1, &barrier);
    }

    check_hresult(m_cmdList->Close());

    ID3D12CommandList* cmdLists[] = { m_cmdList.get() };

    m_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    check_hresult(m_swapChain->Present(1, 0));

    check_hresult(m_cmdQueue->Signal(m_fence.get(), m_fenceValue));

    m_frames[m_currentFrame].FenceWaitValue = m_fenceValue;
    ++m_fenceValue;

    m_currentFrame = m_swapChain->GetCurrentBackBufferIndex();

    if (m_fence->GetCompletedValue() < m_frames[m_currentFrame].FenceWaitValue)
    {
        check_hresult(m_fence->SetEventOnCompletion(m_frames[m_currentFrame].FenceWaitValue,
                                                    m_fenceEvent.get()));

        WaitForSingleObjectEx(m_fenceEvent.get(), INFINITE, false);
    }
}

void App::ExecuteAndWait()
{
    uint64_t waitValue = m_fenceValue;
    ++m_fenceValue;

    m_cmdList->Close();

    ID3D12CommandList* cmdLists[] = {m_cmdList.get()};
    m_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    m_cmdQueue->Signal(m_fence.get(), waitValue);

    check_hresult(m_fence->SetEventOnCompletion(waitValue, m_fenceEvent.get()));

    WaitForSingleObjectEx(m_fenceEvent.get(), INFINITE, false);
}
