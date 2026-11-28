#pragma once

#include <string>
#include <cstdint>
#include <vector>

#include "core/math.h"
#include "core/mesh.h"
#include "core/polymesh.h"

namespace core {

enum class Primitive : int {
    Cube = 0,
    Sphere = 1,
    Cylinder = 2,
    Cone = 3,
    Torus = 4,
    Plane = 5,
    Pyramid = 6,
    Capsule = 7,
    Octahedron = 8,
    HexPrism = 9,
    CustomMesh = 10,
    TriPrism = 11,
    PentPrism = 12,
    OctPrism = 13,
    DecPrism = 14,
    DodecPrism = 15,
    Tetrahedron = 16,
    Icosahedron = 17,
    IcoSphere = 18,
    Pipe = 19,
};
constexpr int kPrimitiveCount = 20;

const char* primitiveName(Primitive p);
PolyMesh makePrimitivePolyMesh(Primitive p, float size, int detail = 8);
float primitiveSmoothAngle(Primitive p);
Mesh makePrimitiveMesh(Primitive p, float size);

struct Transform {
    Vec3 position;
    Vec3 rotationDeg;  // Euler XYZ em graus
    Vec3 scale{1.0f, 1.0f, 1.0f};

    // T * Rz * Ry * Rx * S
    Mat4 matrix() const;
};

// Matriz 3x3 (ordem de colunas) para transformar normais: R * diag(1/scale).
void normalMatrix(const Transform& t, float out[9]);

struct Color {
    float r = 0.35f;
    float g = 0.60f;
    float b = 0.95f;
};

struct Material {
    float metallic = 0.0f;
    float roughness = 0.48f;
    float clearcoat = 0.0f;
    float emission = 0.0f;
};

struct Object {
    int id = 0;
    std::string name;
    Primitive type = Primitive::Cube;
    float size = 1.0f;
    int detail = 8;
    float smoothAngle = 35.0f;
    std::vector<Vec3> vertexColors;
    uint64_t meshRevision = 1;
    Mesh mesh;                 // malha triangulada de renderização
    PolyMesh topology;          // malha poligonal editável (quads/ngons)
    Transform transform;
    Color color;
    Material material;
};

struct Ray {
    Vec3 origin;
    Vec3 dir;
};

struct Scene {
    std::vector<Object> objects;
    int nextId = 1;
    int selectedId = -1;

    Object* find(int id);
    const Object* find(int id) const;
    int indexOf(int id) const;
};

// Id do objeto mais próximo atingido pelo raio, ou -1.
int pickObject(const Scene& scene, const Ray& ray);

}  // namespace core
