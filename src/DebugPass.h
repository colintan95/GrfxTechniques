#pragma once

#include "GpuResourceManager.h"

#include <d3d12.h>
#include <glm/glm.hpp>
#include <winrt/base.h>

class DebugPass
{
public:
    DebugPass(ID3D12Device* device, GpuResourceManager* resourceManager);

    void RecordCommands(const glm::mat4& viewProjMat, ID3D12GraphicsCommandList* cmdList);

private:
    void CreatePipelineState();

    void CreateVertexBuffers();

    void CreateConstantBuffer();

    ID3D12Device* m_device;

    GpuResourceManager* m_resourceManager;

    winrt::com_ptr<ID3D12RootSignature> m_rootSig;
    winrt::com_ptr<ID3D12PipelineState> m_pipeline;

    winrt::com_ptr<ID3D12Resource> m_positionBuffer;
    size_t m_positionBufferSize = 0;

    winrt::com_ptr<ID3D12Resource> m_indexBuffer;
    size_t m_indexBufferSize = 0;

    int m_vertexCount = 0;

    winrt::com_ptr<ID3D12Resource> m_constantBuffer;

    struct Constants
    {
        glm::mat4 WorldViewProjMatrix;
    };

    Constants* m_constantsPtr = nullptr;
};
