#include "core/mesh.h"

#include <algorithm>
#include <cmath>

#include "core/math.h"

namespace core {

namespace {

uint16_t u16(size_t v) { return static_cast<uint16_t>(v); }

void addVertex(Mesh& m, float px, float py, float pz, float nx, float ny, float nz) {
    m.vertices.push_back({px, py, pz, nx, ny, nz});
}

void addTri(Mesh& m, size_t a, size_t b, size_t c) {
    m.indices.push_back(u16(a));
    m.indices.push_back(u16(b));
    m.indices.push_back(u16(c));
}

// Tampa circular (disco) em y, virada para cima ou para baixo.
void addDiscCap(Mesh& m, float radius, float y, int segments, bool top) {
    const size_t center = m.vertices.size();
    const float ny = top ? 1.0f : -1.0f;
    addVertex(m, 0.0f, y, 0.0f, 0.0f, ny, 0.0f);
    for (int j = 0; j <= segments; ++j) {
        const float phi = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(segments);
        addVertex(m, radius * std::cos(phi), y, radius * std::sin(phi), 0.0f, ny, 0.0f);
    }
    for (int j = 0; j < segments; ++j) {
        const size_t r0 = center + 1 + static_cast<size_t>(j);
        const size_t r1 = r0 + 1;
        if (top) {
            addTri(m, center, r1, r0);
        } else {
            addTri(m, center, r0, r1);
        }
    }
}

}  // namespace

Mesh makeCube(float size) {
    const float h = size * 0.5f;

    struct Face {
        float n[3];
        float v[4][3];
    };

    const Face faces[6] = {
        {{0, 0, 1},  {{-h, -h, h},  {h, -h, h},   {h, h, h},   {-h, h, h}}},
        {{0, 0, -1}, {{h, -h, -h},  {-h, -h, -h}, {-h, h, -h}, {h, h, -h}}},
        {{1, 0, 0},  {{h, -h, h},   {h, -h, -h},  {h, h, -h},  {h, h, h}}},
        {{-1, 0, 0}, {{-h, -h, -h}, {-h, -h, h},  {-h, h, h},  {-h, h, -h}}},
        {{0, 1, 0},  {{-h, h, h},   {h, h, h},    {h, h, -h},  {-h, h, -h}}},
        {{0, -1, 0}, {{-h, -h, -h}, {h, -h, -h},  {h, -h, h},  {-h, -h, h}}},
    };

    Mesh mesh;
    for (const Face& f : faces) {
        const size_t base = mesh.vertices.size();
        for (int i = 0; i < 4; ++i) {
            addVertex(mesh, f.v[i][0], f.v[i][1], f.v[i][2], f.n[0], f.n[1], f.n[2]);
        }
        addTri(mesh, base, base + 1, base + 2);
        addTri(mesh, base, base + 2, base + 3);
    }
    return mesh;
}

Mesh makeSphere(float radius, int segments, int rings) {
    segments = std::max(3, segments);
    rings = std::max(2, rings);

    Mesh mesh;
    for (int i = 0; i <= rings; ++i) {
        const float theta = kPi * static_cast<float>(i) / static_cast<float>(rings);
        const float st = std::sin(theta);
        const float ct = std::cos(theta);
        for (int j = 0; j <= segments; ++j) {
            const float phi = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(segments);
            const float nx = st * std::cos(phi);
            const float ny = ct;
            const float nz = st * std::sin(phi);
            addVertex(mesh, nx * radius, ny * radius, nz * radius, nx, ny, nz);
        }
    }
    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < segments; ++j) {
            const size_t a = static_cast<size_t>(i * (segments + 1) + j);
            const size_t b = a + static_cast<size_t>(segments + 1);
            addTri(mesh, a, a + 1, b);
            addTri(mesh, a + 1, b + 1, b);
        }
    }
    return mesh;
}

Mesh makeCylinder(float radius, float height, int segments) {
    segments = std::max(3, segments);
    const float h = height * 0.5f;

    Mesh mesh;
    for (int j = 0; j <= segments; ++j) {
        const float phi = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(segments);
        const float c = std::cos(phi);
        const float s = std::sin(phi);
        addVertex(mesh, radius * c, -h, radius * s, c, 0.0f, s);
        addVertex(mesh, radius * c, h, radius * s, c, 0.0f, s);
    }
    for (int j = 0; j < segments; ++j) {
        const size_t b0 = static_cast<size_t>(2 * j);
        const size_t t0 = b0 + 1;
        const size_t b1 = b0 + 2;
        const size_t t1 = b0 + 3;
        addTri(mesh, b0, t1, b1);
        addTri(mesh, b0, t0, t1);
    }
    addDiscCap(mesh, radius, h, segments, true);
    addDiscCap(mesh, radius, -h, segments, false);
    return mesh;
}

Mesh makeCone(float radius, float height, int segments) {
    segments = std::max(3, segments);
    const float h = height * 0.5f;

    Mesh mesh;
    for (int j = 0; j <= segments; ++j) {
        const float phi = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(segments);
        const float c = std::cos(phi);
        const float s = std::sin(phi);
        const Vec3 n = normalize({height * c, radius, height * s});
        addVertex(mesh, radius * c, -h, radius * s, n.x, n.y, n.z);
        addVertex(mesh, 0.0f, h, 0.0f, n.x, n.y, n.z);
    }
    for (int j = 0; j < segments; ++j) {
        const size_t b0 = static_cast<size_t>(2 * j);
        const size_t apex = b0 + 1;
        const size_t b1 = b0 + 2;
        addTri(mesh, b0, apex, b1);
    }
    addDiscCap(mesh, radius, -h, segments, false);
    return mesh;
}

Mesh makeTorus(float majorRadius, float minorRadius, int segments, int sides) {
    segments = std::max(3, segments);
    sides = std::max(3, sides);

    Mesh mesh;
    for (int i = 0; i <= segments; ++i) {
        const float u = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
        const float cu = std::cos(u);
        const float su = std::sin(u);
        for (int j = 0; j <= sides; ++j) {
            const float v = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(sides);
            const float cv = std::cos(v);
            const float sv = std::sin(v);
            const float ring = majorRadius + minorRadius * cv;
            addVertex(mesh, ring * cu, minorRadius * sv, ring * su, cv * cu, sv, cv * su);
        }
    }
    for (int i = 0; i < segments; ++i) {
        for (int j = 0; j < sides; ++j) {
            const size_t a = static_cast<size_t>(i * (sides + 1) + j);
            const size_t b = a + static_cast<size_t>(sides + 1);
            addTri(mesh, a, a + 1, b);
            addTri(mesh, a + 1, b + 1, b);
        }
    }
    return mesh;
}

Mesh makePlane(float size) {
    const float h = size * 0.5f;
    Mesh mesh;

    // Face de cima.
    addVertex(mesh, -h, 0.0f, h, 0.0f, 1.0f, 0.0f);
    addVertex(mesh, h, 0.0f, h, 0.0f, 1.0f, 0.0f);
    addVertex(mesh, h, 0.0f, -h, 0.0f, 1.0f, 0.0f);
    addVertex(mesh, -h, 0.0f, -h, 0.0f, 1.0f, 0.0f);
    addTri(mesh, 0, 1, 2);
    addTri(mesh, 0, 2, 3);

    // Face de baixo.
    addVertex(mesh, -h, 0.0f, -h, 0.0f, -1.0f, 0.0f);
    addVertex(mesh, h, 0.0f, -h, 0.0f, -1.0f, 0.0f);
    addVertex(mesh, h, 0.0f, h, 0.0f, -1.0f, 0.0f);
    addVertex(mesh, -h, 0.0f, h, 0.0f, -1.0f, 0.0f);
    addTri(mesh, 4, 5, 6);
    addTri(mesh, 4, 6, 7);
    return mesh;
}

}  // namespace core
