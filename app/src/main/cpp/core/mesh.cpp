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

Mesh makePyramid(float size) {
    const float h = size * 0.5f;
    const Vec3 p0{-h, -h, -h};
    const Vec3 p1{ h, -h, -h};
    const Vec3 p2{ h, -h,  h};
    const Vec3 p3{-h, -h,  h};
    const Vec3 apex{0.0f, h, 0.0f};

    Mesh mesh;
    const auto addFlatTriangle = [&mesh](Vec3 a, Vec3 b, Vec3 c) {
        Vec3 normal = normalize(cross(b - a, c - a));
        const Vec3 center = (a + b + c) * (1.0f / 3.0f);
        if (dot(normal, center) < 0.0f) {
            std::swap(b, c);
            normal = normal * -1.0f;
        }
        const size_t base = mesh.vertices.size();
        addVertex(mesh, a.x, a.y, a.z, normal.x, normal.y, normal.z);
        addVertex(mesh, b.x, b.y, b.z, normal.x, normal.y, normal.z);
        addVertex(mesh, c.x, c.y, c.z, normal.x, normal.y, normal.z);
        addTri(mesh, base, base + 1, base + 2);
    };

    // Base quadrada e quatro faces triangulares, com normais planas orientadas para fora.
    addFlatTriangle(p0, p1, p2);
    addFlatTriangle(p0, p2, p3);
    addFlatTriangle(p0, p1, apex);
    addFlatTriangle(p1, p2, apex);
    addFlatTriangle(p2, p3, apex);
    addFlatTriangle(p3, p0, apex);
    return mesh;
}


Mesh makeCapsule(float radius, float height, int segments, int hemisphereRings) {
    segments = std::clamp(segments, 8, 96);
    hemisphereRings = std::clamp(hemisphereRings, 4, 24);
    radius = std::max(radius, 0.001f);
    height = std::max(height, radius * 2.0f);
    const float bodyHalf = height * 0.5f - radius;
    struct Profile { float y; float r; float ny; };
    std::vector<Profile> profile;
    profile.reserve(static_cast<size_t>(2 * hemisphereRings + 2));
    for (int i = 0; i <= hemisphereRings; ++i) {
        const float angle = -0.5f * kPi + 0.5f * kPi * i / hemisphereRings;
        profile.push_back({-bodyHalf + radius * std::sin(angle), radius * std::cos(angle), std::sin(angle)});
    }
    for (int i = 1; i <= hemisphereRings; ++i) {
        const float angle = 0.5f * kPi * i / hemisphereRings;
        profile.push_back({bodyHalf + radius * std::sin(angle), radius * std::cos(angle), std::sin(angle)});
    }
    Mesh mesh;
    for (const Profile& ring : profile) {
        const float radialNormal = std::sqrt(std::max(0.0f, 1.0f - ring.ny * ring.ny));
        for (int j = 0; j <= segments; ++j) {
            const float phi = 2.0f * kPi * j / segments;
            const float c = std::cos(phi), s = std::sin(phi);
            addVertex(mesh, ring.r * c, ring.y, ring.r * s, radialNormal * c, ring.ny, radialNormal * s);
        }
    }
    const size_t stride = static_cast<size_t>(segments + 1);
    for (size_t row = 0; row + 1 < profile.size(); ++row) {
        const size_t base = row * stride, next = base + stride;
        for (int j = 0; j < segments; ++j) {
            const size_t a = base + static_cast<size_t>(j), b = next + static_cast<size_t>(j);
            addTri(mesh, a, a + 1, b);
            addTri(mesh, a + 1, b + 1, b);
        }
    }
    return mesh;
}

Mesh makeOctahedron(float size) {
    const float h = size * 0.5f;
    const Vec3 top{0,h,0}, bottom{0,-h,0}, px{h,0,0}, nx{-h,0,0}, pz{0,0,h}, nz{0,0,-h};
    const Vec3 faces[8][3] = {
        {top,px,pz},{top,pz,nx},{top,nx,nz},{top,nz,px},
        {bottom,pz,px},{bottom,nx,pz},{bottom,nz,nx},{bottom,px,nz}
    };
    Mesh mesh;
    for (const auto& face : faces) {
        Vec3 a=face[0], b=face[1], c=face[2];
        Vec3 normal=normalize(cross(b-a,c-a));
        if (dot(normal,(a+b+c)*(1.0f/3.0f))<0.0f) { std::swap(b,c); normal=normal*-1.0f; }
        const size_t base=mesh.vertices.size();
        addVertex(mesh,a.x,a.y,a.z,normal.x,normal.y,normal.z);
        addVertex(mesh,b.x,b.y,b.z,normal.x,normal.y,normal.z);
        addVertex(mesh,c.x,c.y,c.z,normal.x,normal.y,normal.z);
        addTri(mesh,base,base+1,base+2);
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
