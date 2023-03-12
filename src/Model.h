#pragma once

#include <d3d12.h>

#include <vector>

struct Primitive
{
    D3D12_VERTEX_BUFFER_VIEW Positions;
    D3D12_VERTEX_BUFFER_VIEW Normals;

    D3D12_INDEX_BUFFER_VIEW Indices;

    int VertexCount;
};

struct Mesh
{
    std::vector<Primitive> Primitives;
};

struct Model
{
    std::vector<Mesh> Meshes;
};

