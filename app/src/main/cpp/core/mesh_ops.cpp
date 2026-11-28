#include "core/mesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <unordered_map>
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

void meshSoftNormals(Mesh& mesh) {
    if (mesh.vertices.empty() || mesh.indices.size() < 3) return;

    // Agrupa por posição quantizada e acumula normais de face.
    std::map<Vec3Key, Vec3> accum;
    std::map<Vec3Key, int> counts;

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const Vertex& va = mesh.vertices[mesh.indices[i]];
        const Vertex& vb = mesh.vertices[mesh.indices[i + 1]];
        const Vertex& vc = mesh.vertices[mesh.indices[i + 2]];
        Vec3 a = posOf(va);
        Vec3 b = posOf(vb);
        Vec3 c = posOf(vc);
        Vec3 n = normalize(cross(b - a, c - a));
        for (int k = 0; k < 3; ++k) {
            Vec3 p = posOf(mesh.vertices[mesh.indices[i + static_cast<size_t>(k)]]);
            Vec3Key key = quantize(p, 10000.0f);
            accum[key] = accum[key] + n;
            counts[key] += 1;
        }
    }

    for (Vertex& v : mesh.vertices) {
        Vec3Key key = quantize(posOf(v), 10000.0f);
        auto it = accum.find(key);
        if (it != accum.end()) {
            setNormal(v, normalize(it->second));
        }
    }
}

void meshHardNormals(Mesh& mesh) {
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
        const size_t base = out.vertices.size();
        out.vertices.push_back(va);
        out.vertices.push_back(vb);
        out.vertices.push_back(vc);
        out.indices.push_back(u16(base));
        out.indices.push_back(u16(base + 1));
        out.indices.push_back(u16(base + 2));
    }
    mesh = std::move(out);
}

bool meshSubdivide(Mesh& mesh, int levels) {
    levels = std::clamp(levels, 1, 2);
    for (int level = 0; level < levels; ++level) {
        if (mesh.indices.size() / 3 > 20000) return false;  // safety
        if (mesh.vertices.size() > 40000) return false;

        // Midpoint cache: edge key -> new vertex index
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
            Vec3 n = normalize({m.nx, m.ny, m.nz});
            setNormal(m, n);
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
            // 4 triângulos
            newIndices.push_back(a); newIndices.push_back(ab); newIndices.push_back(ca);
            newIndices.push_back(ab); newIndices.push_back(b); newIndices.push_back(bc);
            newIndices.push_back(ca); newIndices.push_back(bc); newIndices.push_back(c);
            newIndices.push_back(ab); newIndices.push_back(bc); newIndices.push_back(ca);
        }
        mesh.indices = std::move(newIndices);
        meshSoftNormals(mesh);
    }
    return true;
}

void meshSmooth(Mesh& mesh, int iterations, float factor) {
    iterations = std::clamp(iterations, 1, 20);
    factor = std::clamp(factor, 0.0f, 1.0f);
    if (mesh.vertices.empty() || mesh.indices.size() < 3) return;

    // Adjacency by index
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
            // unique neighbors roughly
            for (uint16_t n : adj[i]) {
                sum = sum + posOf(mesh.vertices[n]);
            }
            float inv = 1.0f / static_cast<float>(adj[i].size());
            Vec3 avg = sum * inv;
            Vec3 cur = posOf(mesh.vertices[i]);
            next[i] = cur + (avg - cur) * factor;
        }
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            mesh.vertices[i].px = next[i].x;
            mesh.vertices[i].py = next[i].y;
            mesh.vertices[i].pz = next[i].z;
        }
    }
    meshSoftNormals(mesh);
}

void meshMirror(Mesh& mesh, const char* axis) {
    if (!axis || mesh.vertices.empty()) return;
    char a = static_cast<char>(std::tolower(static_cast<unsigned char>(axis[0])));
    for (Vertex& v : mesh.vertices) {
        if (a == 'x') { v.px = -v.px; v.nx = -v.nx; }
        else if (a == 'y') { v.py = -v.py; v.ny = -v.ny; }
        else if (a == 'z') { v.pz = -v.pz; v.nz = -v.nz; }
    }
    // Flip winding to keep outward normals
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
    }
}

bool meshWeld(Mesh& mesh, float epsilon) {
    epsilon = std::clamp(epsilon, 1e-6f, 0.1f);
    if (mesh.vertices.empty()) return false;

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

    out.indices.reserve(mesh.indices.size());
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        uint16_t a = remap[mesh.indices[i]];
        uint16_t b = remap[mesh.indices[i + 1]];
        uint16_t c = remap[mesh.indices[i + 2]];
        if (a == b || b == c || a == c) continue;  // degenerate
        out.indices.push_back(a);
        out.indices.push_back(b);
        out.indices.push_back(c);
    }
    mesh = std::move(out);
    meshSoftNormals(mesh);
    return true;
}

bool meshExtrude(Mesh& mesh, float amount) {
    if (mesh.indices.size() < 3 || mesh.vertices.empty()) return false;
    amount = std::clamp(amount, -5.0f, 5.0f);

    // Copy original as outer shell offset along normals
    Mesh shell = mesh;
    for (Vertex& v : shell.vertices) {
        Vec3 n = normalize({v.nx, v.ny, v.nz});
        v.px += n.x * amount;
        v.py += n.y * amount;
        v.pz += n.z * amount;
    }

    // Combine: original + shell (flipped if amount negative handled by normal dir)
    Mesh combined = meshCombine(mesh, shell);
    mesh = std::move(combined);
    meshSoftNormals(mesh);
    return true;
}

Mesh meshCombine(const Mesh& a, const Mesh& b) {
    Mesh out = a;
    const size_t base = out.vertices.size();
    out.vertices.insert(out.vertices.end(), b.vertices.begin(), b.vertices.end());
    out.indices.reserve(out.indices.size() + b.indices.size());
    for (uint16_t idx : b.indices) {
        out.indices.push_back(u16(base + static_cast<size_t>(idx)));
    }
    // Cap at uint16
    if (out.vertices.size() > 65535) {
        out.vertices.resize(65535);
    }
    return out;
}

void meshFlipNormals(Mesh& mesh) {
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
    }
    for (Vertex& v : mesh.vertices) {
        v.nx = -v.nx;
        v.ny = -v.ny;
        v.nz = -v.nz;
    }
}

std::string applyMeshOp(Mesh& mesh, const std::string& op, float amount, int iterations) {
    if (op == "soft_normals") {
        meshSoftNormals(mesh);
        return "Soft normals applied";
    }
    if (op == "hard_normals") {
        meshHardNormals(mesh);
        return "Hard normals applied";
    }
    if (op == "divide" || op == "subdivide") {
        int levels = iterations > 0 ? iterations : 1;
        if (!meshSubdivide(mesh, levels)) return "Subdivide failed (mesh too dense)";
        return "Subdivided";
    }
    if (op == "smooth") {
        float factor = amount > 0.0f ? amount : 0.5f;
        int iters = iterations > 0 ? iterations : 3;
        meshSmooth(mesh, iters, factor);
        return "Smoothed";
    }
    if (op == "mirror_x") {
        meshMirror(mesh, "x");
        return "Mirrored on X";
    }
    if (op == "mirror_y") {
        meshMirror(mesh, "y");
        return "Mirrored on Y";
    }
    if (op == "mirror_z") {
        meshMirror(mesh, "z");
        return "Mirrored on Z";
    }
    if (op == "weld" || op == "merge_vertices") {
        float eps = amount > 0.0f ? amount : 0.001f;
        if (!meshWeld(mesh, eps)) return "Weld failed";
        return "Vertices welded";
    }
    if (op == "extrude" || op == "solidify") {
        float am = amount != 0.0f ? amount : 0.05f;
        if (!meshExtrude(mesh, am)) return "Extrude failed";
        return "Extruded";
    }
    if (op == "flip_normals") {
        meshFlipNormals(mesh);
        return "Normals flipped";
    }
    return "Unknown mesh operation: " + op;
}

}  // namespace core
