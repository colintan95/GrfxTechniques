#pragma once

#include <d3d12.h>
#include <glm/glm.hpp>

#include <vector>

struct Primitive
{
    D3D12_VERTEX_BUFFER_VIEW Positions;
    D3D12_VERTEX_BUFFER_VIEW Normals;
    D3D12_VERTEX_BUFFER_VIEW TexCoords;
    D3D12_VERTEX_BUFFER_VIEW Tangents;

    D3D12_INDEX_BUFFER_VIEW Indices;

    int MaterialIdx = -1;

    int VertexCount;
};

struct Mesh
{
    std::vector<Primitive> Primitives;
};

struct Material
{
    glm::vec4 BaseColorFactor;
    float MetallicFactor = 1.f;
    float RoughnessFactor = 1.f;

    int BaseColorTextureId = -1;
    int RoughnessTextureId = -1;
    int NormalTextureId = -1;
};

struct Model
{
    std::vector<Mesh> Meshes;

    std::vector<Material> Materials;
};

