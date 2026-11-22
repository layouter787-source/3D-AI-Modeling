#pragma once

#include <cstdint>
#include <vector>

namespace core {

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

// Cubo centrado na origem, com normais por face (24 vértices, 36 índices).
Mesh makeCube(float size);

}  // namespace core
