#pragma once

#include <cstdint>
#include <vector>

#include "core/polymesh.h"

namespace core {

// Malha de renderização: um vértice por canto de face, com normais suaves ou duras.
struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float cr = 1.0f, cg = 1.0f, cb = 1.0f;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

// smoothAngleDegrees: arestas com ângulo menor que isto ficam suaves; 0 = tudo plano; 180 = tudo suave.
Mesh buildRenderMesh(const PolyMesh& poly, float smoothAngleDegrees, const std::vector<Vec3>& vertexColors = {});

}  // namespace core
