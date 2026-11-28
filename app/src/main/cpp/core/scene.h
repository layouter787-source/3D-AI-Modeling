#pragma once

#include <string>
#include <vector>

#include "core/math.h"
#include "core/mesh.h"

namespace core {

enum class Primitive : int {
    Cube = 0,
    Sphere = 1,
    Cylinder = 2,
    Cone = 3,
    Torus = 4,
    Plane = 5,
};
constexpr int kPrimitiveCount = 6;

const char* primitiveName(Primitive p);
Mesh makePrimitiveMesh(Primitive p, float size);

struct Transform {
    Vec3 position;
    Vec3 rotationDeg;
    Vec3 scale{1.0f, 1.0f, 1.0f};
    Mat4 matrix() const;
};

void normalMatrix(const Transform& t, float out[9]);

// PBR material factors (baseColor + metallic-roughness).
struct Color {
    float r = 0.35f;
    float g = 0.60f;
    float b = 0.95f;
    float metallic = 0.0f;
    float roughness = 0.5f;
};

struct Object {
    int id = 0;
    std::string name;
    Primitive type = Primitive::Cube;
    float size = 1.0f;
    Mesh mesh;
    Transform transform;
    Color color;
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

int pickObject(const Scene& scene, const Ray& ray);

}  // namespace core
