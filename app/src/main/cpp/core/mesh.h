#pragma once

#include <cstdint>
#include <string>
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

// Primitivas centradas na origem, normais e triângulos anti-horários de fora.
Mesh makeCube(float size);
Mesh makeSphere(float radius, int segments, int rings);
Mesh makeCylinder(float radius, float height, int segments);
Mesh makeCone(float radius, float height, int segments);
Mesh makeTorus(float majorRadius, float minorRadius, int segments, int sides);
Mesh makePlane(float size);

// ---- Operações de malha (vídeo / Blender-like) ----

// Soft normals: média das faces adjacentes por posição de vértice.
void meshSoftNormals(Mesh& mesh);

// Hard normals: uma normal por face (duplica vértices se necessário).
void meshHardNormals(Mesh& mesh);

// Subdivide cada triângulo em 4 (midpoint). levels = 1..2.
bool meshSubdivide(Mesh& mesh, int levels);

// Suavização laplaciana (iterations 1..20, factor 0..1).
void meshSmooth(Mesh& mesh, int iterations, float factor);

// Espelhar em torno de um eixo ("x", "y" ou "z").
void meshMirror(Mesh& mesh, const char* axis);

// Fundir vértices a menos de epsilon (weld / merge close).
bool meshWeld(Mesh& mesh, float epsilon);

// Extrusão simples: desloca todos os vértices ao longo da normal média * amount
// e cria uma "casca" lateral (aproximação de solidify/extrude).
bool meshExtrude(Mesh& mesh, float amount);

// Junta duas malhas (b = transformada por offset antes de anexar).
Mesh meshCombine(const Mesh& a, const Mesh& b);

// Inverte a ordem dos índices (flip faces).
void meshFlipNormals(Mesh& mesh);

// Aplica operação por nome (para ToolDispatcher).
// Retorna mensagem de sucesso/erro.
std::string applyMeshOp(Mesh& mesh, const std::string& op, float amount, int iterations);

}  // namespace core
