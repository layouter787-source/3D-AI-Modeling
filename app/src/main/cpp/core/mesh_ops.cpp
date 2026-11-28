#include "core/mesh_ops.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <utility>

#include "core/math.h"

namespace core {

namespace {

constexpr size_t kMaxVerts = 60000;

Vec3 posOf(const Vertex& v) { return {v.px, v.py, v.pz}; }
void setPos(Vertex& v, Vec3 p) { v.px = p.x; v.py = p.y; v.pz = p.z; }
void setNrm(Vertex& v, Vec3 n) { v.nx = n.x; v.ny = n.y; v.nz = n.z; }

Vec3 faceNormal(const Mesh& m, size_t i0, size_t i1, size_t i2) {
    const Vec3 a = posOf(m.vertices[m.indices[i0]]);
    const Vec3 b = posOf(m.vertices[m.indices[i1]]);
    const Vec3 c = posOf(m.vertices[m.indices[i2]]);
    return normalize(cross(b - a, c - a));
}

struct Vec3Key {
    int x, y, z;
    bool operator==(const Vec3Key& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct Vec3KeyHash {
    size_t operator()(const Vec3Key& k) const {
        return static_cast<size_t>(k.x) * 73856093u ^ static_cast<size_t>(k.y) * 19349663u ^
               static_cast<size_t>(k.z) * 83492791u;
    }
};

Vec3Key quantize(Vec3 p, float invEps) {
    return {static_cast<int>(std::round(p.x * invEps)), static_cast<int>(std::round(p.y * invEps)),
            static_cast<int>(std::round(p.z * invEps))};
}

}  // namespace

void recomputeSmoothNormals(Mesh& mesh) {
    if (mesh.vertices.empty() || mesh.indices.size() < 3) return;
    std::vector<Vec3> acc(mesh.vertices.size(), Vec3{});
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint16_t ia = mesh.indices[i];
        const uint16_t ib = mesh.indices[i + 1];
        const uint16_t ic = mesh.indices[i + 2];
        if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size())
            continue;
        const Vec3 n = faceNormal(mesh, i, i + 1, i + 2);
        acc[ia] = acc[ia] + n;
        acc[ib] = acc[ib] + n;
        acc[ic] = acc[ic] + n;
    }
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        setNrm(mesh.vertices[i], normalize(acc[i]));
    }
}

void recomputeHardNormals(Mesh& mesh) {
    if (mesh.indices.size() < 3) return;
    Mesh out;
    out.vertices.reserve(mesh.indices.size());
    out.indices.reserve(mesh.indices.size());
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint16_t ia = mesh.indices[i];
        const uint16_t ib = mesh.indices[i + 1];
        const uint16_t ic = mesh.indices[i + 2];
        if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size())
            continue;
        const Vec3 n = faceNormal(mesh, i, i + 1, i + 2);
        const size_t base = out.vertices.size();
        Vertex va = mesh.vertices[ia];
        Vertex vb = mesh.vertices[ib];
        Vertex vc = mesh.vertices[ic];
        setNrm(va, n);
        setNrm(vb, n);
        setNrm(vc, n);
        out.vertices.push_back(va);
        out.vertices.push_back(vb);
        out.vertices.push_back(vc);
        out.indices.push_back(static_cast<uint16_t>(base));
        out.indices.push_back(static_cast<uint16_t>(base + 1));
        out.indices.push_back(static_cast<uint16_t>(base + 2));
        if (out.vertices.size() >= kMaxVerts) break;
    }
    mesh = std::move(out);
}

bool subdivideMesh(Mesh& mesh) {
    if (mesh.indices.size() < 3) return false;
    const size_t nTris = mesh.indices.size() / 3;
    // Each tri -> 4 tris, ~4x verts worst case
    if (mesh.vertices.size() * 4 > kMaxVerts) return false;

    // Edge midpoint cache: key (min,max) of endpoint indices
    std::map<std::pair<uint16_t, uint16_t>, uint16_t> mid;
    auto midpoint = [&](uint16_t a, uint16_t b) -> uint16_t {
        if (a > b) std::swap(a, b);
        auto it = mid.find({a, b});
        if (it != mid.end()) return it->second;
        const Vec3 pa = posOf(mesh.vertices[a]);
        const Vec3 pb = posOf(mesh.vertices[b]);
        const Vec3 m = (pa + pb) * 0.5f;
        Vertex v{};
        setPos(v, m);
        setNrm(v, normalize(posOf(mesh.vertices[a]) * 0.0f +  // placeholder
                            (Vec3{mesh.vertices[a].nx, mesh.vertices[a].ny, mesh.vertices[a].nz} +
                             Vec3{mesh.vertices[b].nx, mesh.vertices[b].ny, mesh.vertices[b].nz}) *
                                0.5f));
        const uint16_t id = static_cast<uint16_t>(mesh.vertices.size());
        mesh.vertices.push_back(v);
        mid[{a, b}] = id;
        return id;
    };

    std::vector<uint16_t> newIdx;
    newIdx.reserve(nTris * 12);
    for (size_t t = 0; t < nTris; ++t) {
        const uint16_t a = mesh.indices[t * 3 + 0];
        const uint16_t b = mesh.indices[t * 3 + 1];
        const uint16_t c = mesh.indices[t * 3 + 2];
        if (a >= mesh.vertices.size() || b >= mesh.vertices.size() || c >= mesh.vertices.size())
            continue;
        const uint16_t ab = midpoint(a, b);
        const uint16_t bc = midpoint(b, c);
        const uint16_t ca = midpoint(c, a);
        // 4 triangles
        newIdx.insert(newIdx.end(), {a, ab, ca});
        newIdx.insert(newIdx.end(), {b, bc, ab});
        newIdx.insert(newIdx.end(), {c, ca, bc});
        newIdx.insert(newIdx.end(), {ab, bc, ca});
    }
    mesh.indices = std::move(newIdx);
    recomputeSmoothNormals(mesh);
    return true;
}

bool triangulateMesh(Mesh& mesh) {
    // Mesh is already triangle list. Just validate.
    if (mesh.indices.size() % 3 != 0) {
        mesh.indices.resize(mesh.indices.size() - (mesh.indices.size() % 3));
    }
    return true;
}

void smoothMesh(Mesh& mesh, int iterations, float factor) {
    iterations = std::clamp(iterations, 1, 20);
    factor = std::clamp(factor, 0.0f, 1.0f);
    if (mesh.vertices.empty() || mesh.indices.size() < 3) return;

    // Build adjacency
    std::vector<std::vector<uint16_t>> adj(mesh.vertices.size());
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint16_t a = mesh.indices[i];
        const uint16_t b = mesh.indices[i + 1];
        const uint16_t c = mesh.indices[i + 2];
        if (a >= mesh.vertices.size() || b >= mesh.vertices.size() || c >= mesh.vertices.size())
            continue;
        adj[a].push_back(b);
        adj[a].push_back(c);
        adj[b].push_back(a);
        adj[b].push_back(c);
        adj[c].push_back(a);
        adj[c].push_back(b);
    }

    for (int it = 0; it < iterations; ++it) {
        std::vector<Vec3> next(mesh.vertices.size());
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            if (adj[i].empty()) {
                next[i] = posOf(mesh.vertices[i]);
                continue;
            }
            Vec3 sum{};
            // unique neighbors roughly
            for (uint16_t n : adj[i]) sum = sum + posOf(mesh.vertices[n]);
            const float inv = 1.0f / static_cast<float>(adj[i].size());
            const Vec3 avg = sum * inv;
            const Vec3 cur = posOf(mesh.vertices[i]);
            next[i] = cur + (avg - cur) * factor;
        }
        for (size_t i = 0; i < mesh.vertices.size(); ++i) setPos(mesh.vertices[i], next[i]);
           recomputeSmoothNormals(mesh);
    }
}

int weldVertices(Mesh& mesh, float epsilon) {
    epsilon = std::max(epsilon, 1e-6f);
    const float inv = 1.0f / epsilon;
    std::unordered_map<Vec3Key, uint16_t, Vec3KeyHash> map;
    std::vector<uint16_t> remap(mesh.vertices.size());
    Mesh out;
    out.vertices.reserve(mesh.vertices.size());

    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const Vec3Key k = quantize(posOf(mesh.vertices[i]), inv);
        auto it = map.find(k);
        if (it != map.end()) {
            remap[i] = it->second;
        } else {
            const uint16_t id = static_cast<uint16_t>(out.vertices.size());
            map[k] = id;
            remap[i] = id;
            out.vertices.push_back(mesh.vertices[i]);
        }
    }
    out.indices.reserve(mesh.indices.size());
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint16_t a = remap[mesh.indices[i]];
        const uint16_t b = remap[mesh.indices[i + 1]];
        const uint16_t c = remap[mesh.indices[i + 2]];
        if (a == b || b == c || a == c) continue;  // degenerate
        out.indices.push_back(a);
        out.indices.push_back(b);
        out.indices.push_back(c);
    }
    const int removed = static_cast<int>(mesh.vertices.size()) - static_cast<int>(out.vertices.size());
    mesh = std::move(out);
    recomputeSmoothNormals(mesh);
    return std::max(0, removed);
}

void mirrorMesh(Mesh& mesh, int axis, bool weldSeam) {
    axis = std::clamp(axis, 0, 2);
    const size_t base = mesh.vertices.size();
    mesh.vertices.reserve(base * 2);
    // Duplicate mirrored verts
    for (size_t i = 0; i < base; ++i) {
        Vertex v = mesh.vertices[i];
        if (axis == 0) {
            v.px = -v.px;
            v.nx = -v.nx;
        } else if (axis == 1) {
            v.py = -v.py;
            v.ny = -v.ny;
        } else {
            v.pz = -v.pz;
            v.nz = -v.nz;
        }
        mesh.vertices.push_back(v);
    }
    const size_t nIdx = mesh.indices.size();
    mesh.indices.reserve(nIdx * 2);
    for (size_t i = 0; i + 2 < nIdx; i += 3) {
        // Flip winding for mirrored side
        mesh.indices.push_back(static_cast<uint16_t>(mesh.indices[i] + base));
        mesh.indices.push_back(static_cast<uint16_t>(mesh.indices[i + 2] + base));
        mesh.indices.push_back(static_cast<uint16_t>(mesh.indices[i + 1] + base));
    }
    if (weldSeam) weldVertices(mesh, 1e-4f);
    else recomputeSmoothNormals(mesh);
}

bool extrudeFaces(Mesh& mesh, const std::vector<int>& faceIndices, float distance) {
    if (mesh.indices.size() < 3) return false;
    const int nTris = static_cast<int>(mesh.indices.size() / 3);
    std::vector<int> faces = faceIndices;
    if (faces.empty()) {
        faces.resize(static_cast<size_t>(nTris));
        for (int i = 0; i < nTris; ++i) faces[static_cast<size_t>(i)] = i;
    }

    // For each selected face: create extruded top and side walls
    Mesh out = mesh;  // keep original as base (cap)
    for (int fi : faces) {
        if (fi < 0 || fi >= nTris) continue;
        const size_t i0 = static_cast<size_t>(fi) * 3;
        const uint16_t ia = mesh.indices[i0];
        const uint16_t ib = mesh.indices[i0 + 1];
        const uint16_t ic = mesh.indices[i0 + 2];
        if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size())
            continue;
        const Vec3 n = faceNormal(mesh, i0, i0 + 1, i0 + 2);
        const Vec3 off = n * distance;

        Vertex va = mesh.vertices[ia];
        Vertex vb = mesh.vertices[ib];
        Vertex vc = mesh.vertices[ic];
        setPos(va, posOf(va) + off);
        setPos(vb, posOf(vb) + off);
        setPos(vc, posOf(vc) + off);
        setNrm(va, n);
        setNrm(vb, n);
        setNrm(vc, n);

        if (out.vertices.size() + 3 > kMaxVerts) break;
        const uint16_t ta = static_cast<uint16_t>(out.vertices.size());
        out.vertices.push_back(va);
        const uint16_t tb = static_cast<uint16_t>(out.vertices.size());
        out.vertices.push_back(vb);
        const uint16_t tc = static_cast<uint16_t>(out.vertices.size());
        out.vertices.push_back(vc);

        // Top face
        out.indices.push_back(ta);
        out.indices.push_back(tb);
        out.indices.push_back(tc);

        // Side quads as tris (original edges -> extruded)
        auto side = [&](uint16_t a0, uint16_t a1, uint16_t b0, uint16_t b1) {
            out.indices.push_back(a0);
            out.indices.push_back(a1);
            out.indices.push_back(b1);
            out.indices.push_back(a0);
            out.indices.push_back(b1);
            out.indices.push_back(b0);
        };
        side(ia, ib, ta, tb);
        side(ib, ic, tb, tc);
        side(ic, ia, tc, ta);
    }
    mesh = std::move(out);
    recomputeSmoothNormals(mesh);
    return true;
}

bool bevelMesh(Mesh& mesh, float amount, int segments) {
    amount = std::clamp(amount, 0.001f, 10.0f);
    segments = std::clamp(segments, 1, 4);
    // Approximation: inset verts along average normal, then subdivide
    recomputeSmoothNormals(mesh);
    for (auto& v : mesh.vertices) {
        const Vec3 n = {v.nx, v.ny, v.nz};
        setPos(v, posOf(v) + n * (-amount * 0.25f));  // slight inset
    }
    // Expand outward a bit for rounded look
    for (auto& v : mesh.vertices) {
        const Vec3 n = {v.nx, v.ny, v.nz};
        setPos(v, posOf(v) + n * (amount * 0.5f));
    }
    for (int s = 0; s < segments; ++s) {
        if (!subdivideMesh(mesh)) break;
        smoothMesh(mesh, 1, 0.4f);
    }
    recomputeSmoothNormals(mesh);
    return true;
}

void appendMesh(Mesh& dst, const Mesh& other) {
    const uint16_t base = static_cast<uint16_t>(dst.vertices.size());
    if (dst.vertices.size() + other.vertices.size() > kMaxVerts) return;
    dst.vertices.insert(dst.vertices.end(), other.vertices.begin(), other.vertices.end());
    for (uint16_t idx : other.indices) {
        dst.indices.push_back(static_cast<uint16_t>(idx + base));
    }
}

Mesh extractFaces(const Mesh& mesh, const std::vector<int>& faceIndices) {
    Mesh out;
    const int nTris = static_cast<int>(mesh.indices.size() / 3);
    for (int fi : faceIndices) {
        if (fi < 0 || fi >= nTris) continue;
        const size_t i0 = static_cast<size_t>(fi) * 3;
        const uint16_t ia = mesh.indices[i0];
        const uint16_t ib = mesh.indices[i0 + 1];
        const uint16_t ic = mesh.indices[i0 + 2];
        if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size())
            continue;
        const size_t base = out.vertices.size();
        out.vertices.push_back(mesh.vertices[ia]);
        out.vertices.push_back(mesh.vertices[ib]);
        out.vertices.push_back(mesh.vertices[ic]);
        out.indices.push_back(static_cast<uint16_t>(base));
        out.indices.push_back(static_cast<uint16_t>(base + 1));
        out.indices.push_back(static_cast<uint16_t>(base + 2));
    }
    return out;
}

void removeFaces(Mesh& mesh, const std::vector<int>& faceIndices) {
    const int nTris = static_cast<int>(mesh.indices.size() / 3);
    std::vector<char> drop(static_cast<size_t>(nTris), 0);
    for (int fi : faceIndices) {
        if (fi >= 0 && fi < nTris) drop[static_cast<size_t>(fi)] = 1;
    }
    std::vector<uint16_t> newIdx;
    newIdx.reserve(mesh.indices.size());
    for (int fi = 0; fi < nTris; ++fi) {
        if (drop[static_cast<size_t>(fi)]) continue;
        const size_t i0 = static_cast<size_t>(fi) * 3;
        newIdx.push_back(mesh.indices[i0]);
        newIdx.push_back(mesh.indices[i0 + 1]);
        newIdx.push_back(mesh.indices[i0 + 2]);
    }
    mesh.indices = std::move(newIdx);
}

MeshStats meshStats(const Mesh& mesh) {
    return {static_cast<int>(mesh.vertices.size()), static_cast<int>(mesh.indices.size() / 3)};
}

}  // namespace core
