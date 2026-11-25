#include "core/scene.h"

#include <algorithm>
#include <cmath>

namespace core {

const char* primitiveName(Primitive p) {
    switch (p) {
        case Primitive::Cube: return "cube";
        case Primitive::Sphere: return "sphere";
        case Primitive::Cylinder: return "cylinder";
        case Primitive::Cone: return "cone";
        case Primitive::Torus: return "torus";
        case Primitive::Plane: return "plane";
        case Primitive::Pyramid: return "pyramid";
        case Primitive::Capsule: return "capsule";
        case Primitive::Octahedron: return "octahedron";
        case Primitive::HexPrism: return "hex_prism";
        case Primitive::CustomMesh: return "custom_mesh";
    }
    return "cube";
}

Mesh makePrimitiveMesh(Primitive p, float size) {
    switch (p) {
        case Primitive::Cube: return makeCube(size);
        case Primitive::Sphere: return makeSphere(size * 0.5f, 48, 32);
        case Primitive::Cylinder: return makeCylinder(size * 0.5f, size, 48);
        case Primitive::Cone: return makeCone(size * 0.5f, size, 48);
        case Primitive::Torus: return makeTorus(size * 0.35f, size * 0.15f, 64, 24);
        case Primitive::Plane: return makePlane(size);
        case Primitive::Pyramid: return makePyramid(size);
        case Primitive::Capsule: return makeCapsule(size * 0.25f, size, 40, 12);
        case Primitive::Octahedron: return makeOctahedron(size);
        case Primitive::HexPrism: return makeCylinder(size * 0.5f, size, 6);
        case Primitive::CustomMesh: return makeCube(size);
    }
    return makeCube(size);
}

namespace {

Mat4 rotationMatrix(const Transform& t) {
    return multiply(rotateZ(radians(t.rotationDeg.z)),
                    multiply(rotateY(radians(t.rotationDeg.y)), rotateX(radians(t.rotationDeg.x))));
}

Vec3 toVec(const Vertex& v) { return {v.px, v.py, v.pz}; }

// Möller-Trumbore, de dois lados.
bool rayTriangle(const Ray& ray, Vec3 a, Vec3 b, Vec3 c, float& t) {
    const Vec3 e1 = b - a;
    const Vec3 e2 = c - a;
    const Vec3 p = cross(ray.dir, e2);
    const float det = dot(e1, p);
    if (std::fabs(det) < 1e-9f) return false;
    const float inv = 1.0f / det;
    const Vec3 s = ray.origin - a;
    const float u = dot(s, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const Vec3 q = cross(s, e1);
    const float v = dot(ray.dir, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = dot(e2, q) * inv;
    return t > 1e-4f;
}

}  // namespace

Mat4 Transform::matrix() const {
    return multiply(translate(position.x, position.y, position.z),
                    multiply(rotationMatrix(*this), scaleMatrix(scale)));
}

void normalMatrix(const Transform& t, float out[9]) {
    const Mat4 r = rotationMatrix(t);
    const float s[3] = {std::max(std::fabs(t.scale.x), 1e-4f), std::max(std::fabs(t.scale.y), 1e-4f),
                        std::max(std::fabs(t.scale.z), 1e-4f)};
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            out[col * 3 + row] = r.m[col * 4 + row] / s[col];
        }
    }
}

Object* Scene::find(int id) {
    const int i = indexOf(id);
    return i >= 0 ? &objects[static_cast<size_t>(i)] : nullptr;
}

const Object* Scene::find(int id) const {
    const int i = indexOf(id);
    return i >= 0 ? &objects[static_cast<size_t>(i)] : nullptr;
}

int Scene::indexOf(int id) const {
    for (size_t i = 0; i < objects.size(); ++i) {
        if (objects[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

int pickObject(const Scene& scene, const Ray& ray) {
    int bestId = -1;
    float bestT = 1e30f;
    for (const Object& o : scene.objects) {
        const Mat4 model = o.transform.matrix();
        const Mesh& mesh = o.mesh;
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const Vec3 a = transformPoint(model, toVec(mesh.vertices[mesh.indices[i]]));
            const Vec3 b = transformPoint(model, toVec(mesh.vertices[mesh.indices[i + 1]]));
            const Vec3 c = transformPoint(model, toVec(mesh.vertices[mesh.indices[i + 2]]));
            float t = 0.0f;
            if (rayTriangle(ray, a, b, c, t) && t < bestT) {
                bestT = t;
                bestId = o.id;
            }
        }
    }
    return bestId;
}

}  // namespace core
