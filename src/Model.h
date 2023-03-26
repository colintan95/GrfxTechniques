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

    int VertexCount;
};

struct Mesh
{
    std::vector<Primitive> Primitives;
};

struct Material
{
    glm::vec4 BaseColor;

    int BaseColorTextureId;
    int RoughnessTextureId;
    int NormalTextureId;
};

struct Model
{
    std::vector<Mesh> Meshes;
};

