#include "core/scene.h"

#include <algorithm>
#include <cmath>

namespace core {

namespace {

Vec3 toVec(const Vertex& v) { return {v.px, v.py, v.pz}; }

// Möller-Trumbore, de dois lados. A direção do raio não precisa de estar normalizada.
bool rayTriangle(const Ray& ray, Vec3 a, Vec3 b, Vec3 c, float& t) {
    const Vec3 e1 = b - a;
    const Vec3 e2 = c - a;
    const Vec3 p = cross(ray.dir, e2);
    const float det = dot(e1, p);
    if (std::fabs(det) < 1e-12f) return false;
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
    const Mat4 rot = multiply(rotateZ(radians(rotationDeg.z)),
                              multiply(rotateY(radians(rotationDeg.y)), rotateX(radians(rotationDeg.x))));
    return multiply(translate(position.x, position.y, position.z), multiply(rot, scaleMatrix(scale)));
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

Light* Scene::findLight(int id) {
    for (Light& l : lights) {
        if (l.id == id) return &l;
    }
    return nullptr;
}

Mat4 worldMatrix(const Scene& scene, const Object& object) {
    Mat4 m = object.transform.matrix();
    int parent = object.parent;
    for (int depth = 0; parent >= 0 && depth < 32; ++depth) {
        const Object* p = scene.find(parent);
        if (p == nullptr) break;
        m = multiply(p->transform.matrix(), m);
        parent = p->parent;
    }
    return m;
}

void normalMatrixFromWorld(const Mat4& w, float out[9]) {
    const float A00 = w.m[0], A10 = w.m[1], A20 = w.m[2];
    const float A01 = w.m[4], A11 = w.m[5], A21 = w.m[6];
    const float A02 = w.m[8], A12 = w.m[9], A22 = w.m[10];
    const float det = A00 * (A11 * A22 - A12 * A21) - A01 * (A10 * A22 - A12 * A20) +
                      A02 * (A10 * A21 - A11 * A20);
    const float inv = std::fabs(det) < 1e-20f ? 1.0f : 1.0f / det;
    const float C00 = A11 * A22 - A12 * A21, C01 = A12 * A20 - A10 * A22, C02 = A10 * A21 - A11 * A20;
    const float C10 = A02 * A21 - A01 * A22, C11 = A00 * A22 - A02 * A20, C12 = A01 * A20 - A00 * A21;
    const float C20 = A01 * A12 - A02 * A11, C21 = A02 * A10 - A00 * A12, C22 = A00 * A11 - A01 * A10;
    out[0] = C00 * inv; out[1] = C10 * inv; out[2] = C20 * inv;
    out[3] = C01 * inv; out[4] = C11 * inv; out[5] = C21 * inv;
    out[6] = C02 * inv; out[7] = C12 * inv; out[8] = C22 * inv;
}

int pickObject(const Scene& scene, const Ray& ray) {
    int bestId = -1;
    float bestT = 1e30f;
    for (const Object& o : scene.objects) {
        if (!o.visible || !o.render || o.render->indices.empty()) continue;
        const Mat4 inv = inverseAffine(worldMatrix(scene, o));
        Ray local;
        local.origin = transformPoint(inv, ray.origin);
        local.dir = transformDir(inv, ray.dir);
        const Mesh& mesh = *o.render;
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const Vec3 a = toVec(mesh.vertices[mesh.indices[i]]);
            const Vec3 b = toVec(mesh.vertices[mesh.indices[i + 1]]);
            const Vec3 c = toVec(mesh.vertices[mesh.indices[i + 2]]);
            float t = 0.0f;
            if (rayTriangle(local, a, b, c, t) && t < bestT) {
                bestT = t;
                bestId = o.id;
            }
        }
    }
    return bestId;
}

void objectBounds(const Object& object, Vec3& mn, Vec3& mx) {
    if (!object.poly) {
        mn = Vec3{};
        mx = Vec3{};
        return;
    }
    meshBounds(*object.poly, mn, mx);
}

void worldBounds(const Scene& scene, const Object& object, Vec3& mn, Vec3& mx) {
    Vec3 lo, hi;
    objectBounds(object, lo, hi);
    const Mat4 w = worldMatrix(scene, object);
    bool first = true;
    for (int i = 0; i < 8; ++i) {
        const Vec3 corner{(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z};
        const Vec3 p = transformPoint(w, corner);
        if (first) {
            mn = mx = p;
            first = false;
        } else {
            mn = vmin(mn, p);
            mx = vmax(mx, p);
        }
    }
}

}  // namespace core
