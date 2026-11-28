#include "core/mesh_ops.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

#include "core/math.h"

namespace core {

namespace {

uint16_t u16(size_t v) { return static_cast<uint16_t>(v); }

Vec3 posOf(const Vertex& v) { return {v.px, v.py, v.pz}; }

void setNormal(Vertex& v, Vec3 n) {
    v.nx = n.x;
    v.ny = n.y;
    v.nz = n.z;
}

struct Vec3Key {
    int x, y, z;
    bool operator<(const Vec3Key& o) const {
        if (x != o.x) return x < o.x;
        if (y != o.y) return y < o.y;
        return z < o.z;
    }
};

Vec3Key quantize(Vec3 p, float scale) {
    return {static_cast<int>(std::lround(p.x * scale)),
            static_cast<int>(std::lround(p.y * scale)),
            static_cast<int>(std::lround(p.z * scale))};
}

}  // namespace

void recomputeSmoothNormals(Mesh& mesh) {
    if (mesh.vertices.empty() || mesh.indices.size() < 3) return;

    std::map<Vec3Key, Vec3> accum;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        Vec3 a = posOf(mesh.vertices[mesh.indices[i]]);
        Vec3 b = posOf(mesh.vertices[mesh.indices[i + 1]]);
        Vec3 c = posOf(mesh.vertices[mesh.indices[i + 2]]);
        Vec3 n = normalize(cross(b - a, c - a));
        for (int k = 0; k < 3; ++k) {
            Vec3 p = posOf(mesh.vertices[mesh.indices[i + static_cast<size_t>(k)]]);
            Vec3Key key = quantize(p, 10000.0f);
            accum[key] = accum[key] + n;
        }
    }
    for (Vertex& v : mesh.vertices) {
        auto it = accum.find(quantize(posOf(v), 10000.0f));
        if (it != accum.end()) setNormal(v, normalize(it->second));
    }
}

void recomputeHardNormals(Mesh& mesh) {
    if (mesh.indices.size() < 3) return;
    Mesh out;
    out.vertices.reserve(mesh.indices.size());
    out.indices.reserve(mesh.indices.size());
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        Vertex va = mesh.vertices[mesh.indices[i]];
        Vertex vb = mesh.vertices[mesh.indices[i + 1]];
        Vertex vc = mesh.vertices[mesh.indices[i + 2]];
        Vec3 n = normalize(cross(posOf(vb) - posOf(va), posOf(vc) - posOf(va)));
        setNormal(va, n);
        setNormal(vb, n);
        setNormal(vc, n);
        size_t base = out.vertices.size();
        out.vertices.push_back(va);
        out.vertices.push_back(vb);
        out.vertices.push_back(vc);
        out.indices.push_back(u16(base));
        out.indices.push_back(u16(base + 1));
        out.indices.push_back(u16(base + 2));
    }
    mesh = std::move(out);
}

bool subdivideMesh(Mesh& mesh) {
    if (mesh.indices.size() / 3 > 20000) return false;
    if (mesh.vertices.size() > 40000) return false;

    std::map<std::pair<uint16_t, uint16_t>, uint16_t> midCache;
    auto midpoint = [&](uint16_t ia, uint16_t ib) -> uint16_t {
        if (ia > ib) std::swap(ia, ib);
        auto key = std::make_pair(ia, ib);
        auto it = midCache.find(key);
        if (it != midCache.end()) return it->second;
        const Vertex& a = mesh.vertices[ia];
        const Vertex& b = mesh.vertices[ib];
        Vertex m;
        m.px = 0.5f * (a.px + b.px);
        m.py = 0.5f * (a.py + b.py);
        m.pz = 0.5f * (a.pz + b.pz);
        m.nx = 0.5f * (a.nx + b.nx);
        m.ny = 0.5f * (a.ny + b.ny);
        m.nz = 0.5f * (a.nz + b.nz);
        setNormal(m, normalize({m.nx, m.ny, m.nz}));
        uint16_t idx = u16(mesh.vertices.size());
        mesh.vertices.push_back(m);
        midCache[key] = idx;
        return idx;
    };

    std::vector<uint16_t> newIndices;
    newIndices.reserve(mesh.indices.size() * 4);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        uint16_t a = mesh.indices[i];
        uint16_t b = mesh.indices[i + 1];
        uint16_t c = mesh.indices[i + 2];
        uint16_t ab = midpoint(a, b);
        uint16_t bc = midpoint(b, c);
        uint16_t ca = midpoint(c, a);
        newIndices.push_back(a);  newIndices.push_back(ab); newIndices.push_back(ca);
        newIndices.push_back(ab); newIndices.push_back(b);  newIndices.push_back(bc);
        newIndices.push_back(ca); newIndices.push_back(bc); newIndices.push_back(c);
        newIndices.push_back(ab); newIndices.push_back(bc); newIndices.push_back(ca);
    }
    mesh.indices = std::move(newIndices);
    recomputeSmoothNormals(mesh);
    return true;
}

bool triangulateMesh(Mesh& mesh) {
    // Já somos só triângulos; weld suave opcional.
    (void)mesh;
    return true;
}

void smoothMesh(Mesh& mesh, int iterations, float factor) {
    iterations = std::clamp(iterations, 1, 20);
    factor = std::clamp(factor, 0.0f, 1.0f);
    if (mesh.vertices.empty() || mesh.indices.size() < 3) return;

    std::vector<std::vector<uint16_t>> adj(mesh.vertices.size());
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        uint16_t a = mesh.indices[i];
        uint16_t b = mesh.indices[i + 1];
        uint16_t c = mesh.indices[i + 2];
        adj[a].push_back(b); adj[a].push_back(c);
        adj[b].push_back(a); adj[b].push_back(c);
        adj[c].push_back(a); adj[c].push_back(b);
    }

    for (int it = 0; it < iterations; ++it) {
        std::vector<Vec3> next(mesh.vertices.size());
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            if (adj[i].empty()) {
                next[i] = posOf(mesh.vertices[i]);
                continue;
            }
            Vec3 sum{};
            for (uint16_t n : adj[i]) sum = sum + posOf(mesh.vertices[n]);
            Vec3 avg = sum * (1.0f / static_cast<float>(adj[i].size()));
            Vec3 cur = posOf(mesh.vertices[i]);
            next[i] = cur + (avg - cur) * factor;
        }
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            mesh.vertices[i].px = next[i].x;
            mesh.vertices[i].py = next[i].y;
            mesh.vertices[i].pz = next[i].z;
        }
    }
    recomputeSmoothNormals(mesh);
}

int weldVertices(Mesh& mesh, float epsilon) {
    epsilon = std::clamp(epsilon, 1e-6f, 0.1f);
    if (mesh.vertices.empty()) return 0;
    const int before = static_cast<int>(mesh.vertices.size());
    const float scale = 1.0f / epsilon;
    std::map<Vec3Key, uint16_t> map;
    std::vector<uint16_t> remap(mesh.vertices.size());
    Mesh out;
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        Vec3Key key = quantize(posOf(mesh.vertices[i]), scale);
        auto it = map.find(key);
        if (it != map.end()) {
            remap[i] = it->second;
        } else {
            uint16_t ni = u16(out.vertices.size());
            out.vertices.push_back(mesh.vertices[i]);
            map[key] = ni;
            remap[i] = ni;
        }
    }
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        uint16_t a = remap[mesh.indices[i]];
        uint16_t b = remap[mesh.indices[i + 1]];
        uint16_t c = remap[mesh.indices[i + 2]];
        if (a == b || b == c || a == c) continue;
        out.indices.push_back(a);
        out.indices.push_back(b);
        out.indices.push_back(c);
    }
    mesh = std::move(out);
    recomputeSmoothNormals(mesh);
    return before - static_cast<int>(mesh.vertices.size());
}

void mirrorMesh(Mesh& mesh, int axis, bool weldSeam) {
    if (mesh.vertices.empty()) return;
    axis = std::clamp(axis, 0, 2);
    Mesh mirrored = mesh;
    for (Vertex& v : mirrored.vertices) {
        if (axis == 0) { v.px = -v.px; v.nx = -v.nx; }
        else if (axis == 1) { v.py = -v.py; v.ny = -v.ny; }
        else { v.pz = -v.pz; v.nz = -v.nz; }
    }
    for (size_t i = 0; i + 2 < mirrored.indices.size(); i += 3) {
        std::swap(mirrored.indices[i + 1], mirrored.indices[i + 2]);
    }
    appendMesh(mesh, mirrored);
    if (weldSeam) weldVertices(mesh, 1e-4f);
    else recomputeSmoothNormals(mesh);
}

bool extrudeFaces(Mesh& mesh, const std::vector<int>& faceIndices, float distance) {
    if (mesh.indices.size() < 3) return false;
    distance = std::clamp(distance, -5.0f, 5.0f);

    const int numTris = static_cast<int>(mesh.indices.size() / 3);
    std::vector<int> faces = faceIndices;
    if (faces.empty()) {
        faces.resize(static_cast<size_t>(numTris));
        for (int i = 0; i < numTris; ++i) faces[static_cast<size_t>(i)] = i;
    }

    // Offset all verts of selected faces along their normals, keep original as inner.
    Mesh outer = mesh;
    for (int fi : faces) {
        if (fi < 0 || fi >= numTris) continue;
        size_t base = static_cast<size_t>(fi) * 3;
        for (int k = 0; k < 3; ++k) {
            uint16_t idx = mesh.indices[base + static_cast<size_t>(k)];
            Vertex& v = outer.vertices[idx];
            Vec3 n = normalize({v.nx, v.ny, v.nz});
            v.px += n.x * distance;
            v.py += n.y * distance;
            v.pz += n.z * distance;
        }
    }
    // Keep only selected faces on outer shell for a lighter result
    Mesh shell = extractFaces(outer, faces);
    appendMesh(mesh, shell);
    recomputeSmoothNormals(mesh);
    return true;
}

bool bevelMesh(Mesh& mesh, float amount, int segments) {
    amount = std::clamp(amount, 0.001f, 1.0f);
    segments = std::clamp(segments, 1, 3);
    // Approximation: slight inset along normals + subdivide
    for (Vertex& v : mesh.vertices) {
        Vec3 n = normalize({v.nx, v.ny, v.nz});
        v.px -= n.x * amount * 0.25f;
        v.py -= n.y * amount * 0.25f;
        v.pz -= n.z * amount * 0.25f;
    }
    for (int s = 0; s < segments; ++s) {
        if (!subdivideMesh(mesh)) break;
    }
    smoothMesh(mesh, 2, 0.4f);
    return true;
}

void appendMesh(Mesh& dst, const Mesh& other) {
    const size_t base = dst.vertices.size();
    if (base + other.vertices.size() > 65535) return;
    dst.vertices.insert(dst.vertices.end(), other.vertices.begin(), other.vertices.end());
    dst.indices.reserve(dst.indices.size() + other.indices.size());
    for (uint16_t idx : other.indices) {
        dst.indices.push_back(u16(base + static_cast<size_t>(idx)));
    }
}

Mesh extractFaces(const Mesh& mesh, const std::vector<int>& faceIndices) {
    Mesh out;
    const int numTris = static_cast<int>(mesh.indices.size() / 3);
    for (int fi : faceIndices) {
        if (fi < 0 || fi >= numTris) continue;
        size_t base = static_cast<size_t>(fi) * 3;
        size_t vb = out.vertices.size();
        for (int k = 0; k < 3; ++k) {
            out.vertices.push_back(mesh.vertices[mesh.indices[base + static_cast<size_t>(k)]]);
            out.indices.push_back(u16(vb + static_cast<size_t>(k)));
        }
    }
    return out;
}

void removeFaces(Mesh& mesh, const std::vector<int>& faceIndices) {
    if (faceIndices.empty()) return;
    std::vector<char> kill(mesh.indices.size() / 3, 0);
    for (int fi : faceIndices) {
        if (fi >= 0 && fi < static_cast<int>(kill.size())) kill[static_cast<size_t>(fi)] = 1;
    }
    std::vector<uint16_t> ni;
    ni.reserve(mesh.indices.size());
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        if (!kill[i / 3]) {
            ni.push_back(mesh.indices[i]);
            ni.push_back(mesh.indices[i + 1]);
            ni.push_back(mesh.indices[i + 2]);
        }
    }
    mesh.indices = std::move(ni);
}

MeshStats meshStats(const Mesh& mesh) {
    return {static_cast<int>(mesh.vertices.size()), static_cast<int>(mesh.indices.size() / 3)};
}

}  // namespace core
