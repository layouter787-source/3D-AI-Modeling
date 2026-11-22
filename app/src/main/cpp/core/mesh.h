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

// Todas as primitivas ficam centradas na origem, com normais e triângulos
// em sentido anti-horário visto de fora.
Mesh makeCube(float size);
Mesh makeSphere(float radius, int segments, int rings);
Mesh makeCylinder(float radius, float height, int segments);
Mesh makeCone(float radius, float height, int segments);
Mesh makeTorus(float majorRadius, float minorRadius, int segments, int sides);
Mesh makePlane(float size);

}  // namespace core
