#include "App.h"

#include "gen/ShaderPS.h"
#include "gen/ShaderVS.h"
#include "Utils.h"

#include <d3dx12.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#pragma warning(push)
#pragma warning(disable:4201)
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#pragma warning(pop)

#include <numbers>
#include <vector>

using winrt::check_bool;
using winrt::check_hresult;
using winrt::com_ptr;

App::App(HWND hwnd, InputManager* inputManager)
    : m_hwnd(hwnd), m_inputManager(inputManager)
{
    CreateDevice();

    m_resourceManager = std::make_unique<GpuResourceManager>(m_device.get());

    CreateCmdQueueAndSwapChain();

    CreateCommandList();

    CreatePipelineState();

    CreateDescriptorHeaps();

    CreateDepthTexture();

    CreateConstantBuffer();

    m_debugPass = std::make_unique<DebugPass>(&m_scene, m_device.get(), m_resourceManager.get());

    m_camera = std::make_unique<Camera>(m_inputManager);

    m_resourceManager->LoadGltfModel("assets/box/Box.gltf", &m_model);

    m_resourceManager->LoadGltfModel("assets/sponza/Sponza.gltf", &m_sponza);

    m_scene.LightPos = glm::vec3(0.f, 1.f, -1.5f);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX12_Init(m_device.get(), NUM_FRAMES, DXGI_FORMAT_R8G8B8A8_UNORM, m_guiSrvHeap.get(),
                        m_guiSrvHeap->GetCPUDescriptorHandleForHeapStart(),
                        m_guiSrvHeap->GetGPUDescriptorHandleForHeapStart());
}

App::~App()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();

    ImGui::DestroyContext();
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
        check_hresult(m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_frames[i].BeginCmdAlloc.put())));

        check_hresult(m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_frames[i].DrawCmdAlloc.put())));

        check_hresult(m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_frames[i].GuiCmdAlloc.put())));

        check_hresult(m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_frames[i].PresentCmdAlloc.put())));
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
    CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParams[3];
    rootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
                                           D3D12_SHADER_VISIBILITY_ALL);
    rootParams[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
    rootParams[2].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init_1_1(_countof(rootParams), rootParams, 0, nullptr,
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
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
         0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
         0}
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

    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = 1;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        check_hresult(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_samplerHeap.put())));
        m_samplerHandleSize = m_device->GetDescriptorHandleIncrementSize(heapDesc.Type);

        D3D12_CPU_DESCRIPTOR_HANDLE samplerCpuHandle =
            m_samplerHeap->GetCPUDescriptorHandleForHeapStart();

        D3D12_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.MinLOD = 0.f;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        samplerDesc.BorderColor[0] = 0;
        samplerDesc.BorderColor[1] = 0;
        samplerDesc.BorderColor[2] = 0;
        samplerDesc.BorderColor[3] = 0;

        m_device->CreateSampler(&samplerDesc, samplerCpuHandle);

        m_samplerGpuHandle = m_samplerHeap->GetGPUDescriptorHandleForHeapStart();
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = 1;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        check_hresult(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_guiSrvHeap.put())));
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

void App::CreateConstantBuffer()
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

    m_projMat = glm::perspective(
        std::numbers::pi_v<float> / 4.f,
        static_cast<float>(m_windowWidth) / static_cast<float>(m_windowHeight), 0.1f, 1000.f);
}

void App::Render()
{
    BeginFrame();

    DrawModels();

    RenderGui();

    PresentFrame();
}

void App::BeginFrame()
{
    check_hresult(m_frames[m_currentFrame].BeginCmdAlloc->Reset());
    check_hresult(m_cmdList->Reset(m_frames[m_currentFrame].BeginCmdAlloc.get(), nullptr));

    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_frames[m_currentFrame].SwapChainBuffer.get(), D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        m_cmdList->ResourceBarrier(1, &barrier);
    }

    static constexpr float clearColor[] = { 0.f, 0.f, 0.f, 1.f };

    m_cmdList->ClearRenderTargetView(m_frames[m_currentFrame].RtvHandle, clearColor, 0, nullptr);
    m_cmdList->ClearDepthStencilView(m_dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

    check_hresult(m_cmdList->Close());

    ID3D12CommandList* cmdLists[] = { m_cmdList.get() };
    m_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
}

void App::DrawModels()
{
    glm::mat4 worldMat = glm::scale(glm::mat4(1.f), glm::vec3(0.008f));

    m_constantsPtr->WorldViewProjMatrix = m_projMat * m_camera->GetViewMat() * worldMat;
    m_constantsPtr->LightPos = glm::vec4(m_scene.LightPos, 1.f);

    check_hresult(m_frames[m_currentFrame].DrawCmdAlloc->Reset());
    check_hresult(m_cmdList->Reset(m_frames[m_currentFrame].DrawCmdAlloc.get(), nullptr));

    m_cmdList->OMSetRenderTargets(1, &m_frames[m_currentFrame].RtvHandle, false, &m_dsvHandle);

    m_cmdList->RSSetViewports(1, &m_viewport);
    m_cmdList->RSSetScissorRects(1, &m_scissorRect);

    m_cmdList->SetPipelineState(m_pipeline.get());
    m_cmdList->SetGraphicsRootSignature(m_rootSig.get());

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        m_samplerHeap.get(), m_resourceManager->GetTextureSrvHeap()
    };

    m_cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    m_cmdList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());
    m_cmdList->SetGraphicsRootDescriptorTable(2, m_samplerGpuHandle);

    for (const auto& mesh : m_sponza.Meshes)
    {
        for (const auto& prim : mesh.Primitives)
        {
            TextureId baseColorTextureId =
                m_sponza.Materials[prim.MaterialIdx].BaseColorTextureId;

            m_cmdList->SetGraphicsRootDescriptorTable(
                1, m_resourceManager->GetTextureSrvHandle(baseColorTextureId));

            m_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            m_cmdList->IASetVertexBuffers(0, 1, &prim.Positions);
            m_cmdList->IASetVertexBuffers(1, 1, &prim.Normals);
            m_cmdList->IASetVertexBuffers(2, 1, &prim.TexCoords);

            m_cmdList->IASetIndexBuffer(&prim.Indices);

            m_cmdList->DrawIndexedInstanced(prim.VertexCount, 1, 0, 0, 0);
        }
    }

    m_debugPass->RecordCommands(m_constantsPtr->WorldViewProjMatrix, m_cmdList.get());

    check_hresult(m_cmdList->Close());

    ID3D12CommandList* cmdLists[] = { m_cmdList.get() };
    m_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
}

void App::RenderGui()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // static bool showDemoWindow = true;

    // if (showDemoWindow)
    // {
    //     ImGui::ShowDemoWindow(&showDemoWindow);
    // }

    // static bool showWindow = true;

    // static int mode = 0;

    // if (showWindow)
    // {
    //     ImGui::Begin("Window", &showWindow);

    //     ImGui::RadioButton("Static", &mode, 0);
    //     ImGui::RadioButton("FPS", &mode, 1);

    //     ImGui::End();
    // }

    ImGui::Render();

    check_hresult(m_frames[m_currentFrame].GuiCmdAlloc->Reset());
    check_hresult(m_cmdList->Reset(m_frames[m_currentFrame].GuiCmdAlloc.get(), nullptr));

    m_cmdList->OMSetRenderTargets(1, &m_frames[m_currentFrame].RtvHandle, false, nullptr);

    ID3D12DescriptorHeap* heaps[] = { m_guiSrvHeap.get() };
    m_cmdList->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_cmdList.get());

    check_hresult(m_cmdList->Close());

    ID3D12CommandList* cmdLists[] = { m_cmdList.get() };
    m_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
}

void App::PresentFrame()
{
    check_hresult(m_frames[m_currentFrame].PresentCmdAlloc->Reset());
    check_hresult(m_cmdList->Reset(m_frames[m_currentFrame].PresentCmdAlloc.get(), nullptr));

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

void App::Tick(double elapsedSec)
{
    m_camera->Tick(elapsedSec);
}
