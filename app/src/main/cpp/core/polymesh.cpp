#include "core/polymesh.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace core {

namespace {

uint32_t u32(size_t v) { return static_cast<uint32_t>(v); }

int clampInt(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

uint64_t edgeKey(uint32_t a, uint32_t b) {
    if (a > b) std::swap(a, b);
    return (static_cast<uint64_t>(a) << 32) | b;
}

uint64_t dirKey(uint32_t a, uint32_t b) { return (static_cast<uint64_t>(a) << 32) | b; }

std::pair<uint32_t, uint32_t> sortedPair(uint32_t a, uint32_t b) {
    return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
}

uint32_t keyLo(uint64_t k) { return static_cast<uint32_t>(k >> 32); }
uint32_t keyHi(uint64_t k) { return static_cast<uint32_t>(k & 0xffffffffu); }

// Adiciona uma face sem vértices repetidos seguidos (também de forma cíclica).
void addFace(PolyMesh& m, const std::vector<uint32_t>& f) {
    std::vector<uint32_t> g;
    g.reserve(f.size());
    for (uint32_t v : f) {
        if (g.empty() || g.back() != v) g.push_back(v);
    }
    while (g.size() > 1 && g.front() == g.back()) g.pop_back();
    if (g.size() >= 3) m.faces.push_back(std::move(g));
}

// Grelha de quads: origin + u*i/nu + v*j/nv. Sai para fora quando u x v aponta para fora.
void addGrid(PolyMesh& m, Vec3 origin, Vec3 u, Vec3 v, int nu, int nv) {
    const uint32_t base = u32(m.verts.size());
    for (int j = 0; j <= nv; ++j) {
        for (int i = 0; i <= nu; ++i) {
            m.verts.push_back(origin + u * (static_cast<float>(i) / static_cast<float>(nu)) +
                              v * (static_cast<float>(j) / static_cast<float>(nv)));
        }
    }
    auto id = [&](int i, int j) { return base + static_cast<uint32_t>(j * (nu + 1) + i); };
    for (int j = 0; j < nv; ++j) {
        for (int i = 0; i < nu; ++i) {
            m.faces.push_back({id(i, j), id(i + 1, j), id(i + 1, j + 1), id(i, j + 1)});
        }
    }
}

// Inverte todas as faces se as normais apontarem, em média, para dentro.
void autoOrient(PolyMesh& m) {
    if (m.verts.empty() || m.faces.empty()) return;
    Vec3 c;
    for (const Vec3& v : m.verts) c += v;
    c = c / static_cast<float>(m.verts.size());
    double score = 0.0;
    for (const auto& f : m.faces) {
        Vec3 fc;
        for (uint32_t v : f) fc += m.verts[v];
        fc = fc / static_cast<float>(f.size());
        score += dot(newellNormal(m, f), fc - c);
    }
    if (score < 0.0) {
        for (auto& f : m.faces) std::reverse(f.begin(), f.end());
    }
}

Vec3 perpendicular(Vec3 t) {
    const Vec3 axis = std::fabs(t.y) < 0.9f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
    return normalize(cross(axis, t));
}

}  // namespace

// ---------------------------------------------------------------- Consultas

Vec3 newellNormal(const PolyMesh& m, const std::vector<uint32_t>& face) {
    Vec3 n;
    const size_t count = face.size();
    for (size_t k = 0; k < count; ++k) {
        const Vec3& a = m.verts[face[k]];
        const Vec3& b = m.verts[face[(k + 1) % count]];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return n;
}

Vec3 faceNormal(const PolyMesh& m, size_t face) { return normalize(newellNormal(m, m.faces[face])); }

Vec3 faceCenter(const PolyMesh& m, size_t face) {
    Vec3 c;
    for (uint32_t v : m.faces[face]) c += m.verts[v];
    return c / static_cast<float>(m.faces[face].size());
}

void meshBounds(const PolyMesh& m, Vec3& mn, Vec3& mx) {
    if (m.verts.empty()) {
        mn = Vec3{};
        mx = Vec3{};
        return;
    }
    mn = mx = m.verts[0];
    for (const Vec3& v : m.verts) {
        mn = vmin(mn, v);
        mx = vmax(mx, v);
    }
}

std::vector<Vec3> vertexNormals(const PolyMesh& m) {
    std::vector<Vec3> n(m.verts.size());
    for (const auto& f : m.faces) {
        const Vec3 fn = newellNormal(m, f);
        for (uint32_t v : f) n[v] += fn;
    }
    for (Vec3& v : n) v = normalize(v);
    return n;
}

// ---------------------------------------------------------------- Operações simples

PolyMesh weldVertices(const PolyMesh& m, float eps) {
    eps = std::max(eps, 1e-7f);
    std::map<std::tuple<long long, long long, long long>, uint32_t> grid;
    std::vector<uint32_t> remap(m.verts.size());
    PolyMesh out;
    for (size_t i = 0; i < m.verts.size(); ++i) {
        const Vec3& p = m.verts[i];
        const auto key = std::make_tuple(std::llround(p.x / eps), std::llround(p.y / eps), std::llround(p.z / eps));
        auto it = grid.find(key);
        if (it == grid.end()) {
            it = grid.emplace(key, u32(out.verts.size())).first;
            out.verts.push_back(p);
        }
        remap[i] = it->second;
    }
    for (const auto& f : m.faces) {
        std::vector<uint32_t> g;
        g.reserve(f.size());
        for (uint32_t v : f) g.push_back(remap[v]);
        addFace(out, g);
    }
    std::unordered_set<uint64_t> seen;
    for (const auto& c : m.creases) {
        const uint32_t a = remap[c.first];
        const uint32_t b = remap[c.second];
        if (a != b && seen.insert(edgeKey(a, b)).second) out.creases.push_back(sortedPair(a, b));
    }
    return out;
}

PolyMesh flipFaces(const PolyMesh& m) {
    PolyMesh out = m;
    for (auto& f : out.faces) std::reverse(f.begin(), f.end());
    return out;
}

PolyMesh triangulate(const PolyMesh& m) {
    PolyMesh out;
    out.verts = m.verts;
    out.creases = m.creases;
    for (const auto& f : m.faces) {
        for (size_t k = 1; k + 1 < f.size(); ++k) out.faces.push_back({f[0], f[k], f[k + 1]});
    }
    return out;
}

PolyMesh mergeMeshes(const PolyMesh& a, const PolyMesh& b) {
    PolyMesh out = a;
    const uint32_t offset = u32(a.verts.size());
    out.verts.insert(out.verts.end(), b.verts.begin(), b.verts.end());
    for (const auto& f : b.faces) {
        std::vector<uint32_t> g;
        g.reserve(f.size());
        for (uint32_t v : f) g.push_back(v + offset);
        out.faces.push_back(std::move(g));
    }
    for (const auto& c : b.creases) out.creases.push_back({c.first + offset, c.second + offset});
    return out;
}

PolyMesh transformed(const PolyMesh& m, const Mat4& t) {
    PolyMesh out = m;
    for (Vec3& v : out.verts) v = transformPoint(t, v);
    return out;
}

PolyMesh reflectMesh(const PolyMesh& m, int axis) {
    PolyMesh out = m;
    for (Vec3& v : out.verts) axisRef(v, axis) = -axisVal(v, axis);
    for (auto& f : out.faces) std::reverse(f.begin(), f.end());
    return out;
}

PolyMesh mirrorMesh(const PolyMesh& m, int axis, float mergeDistance) {
    return weldVertices(mergeMeshes(m, reflectMesh(m, axis)), mergeDistance);
}

// ---------------------------------------------------------------- Geradores

PolyMesh makeBoxPoly(Vec3 size, int sx, int sy, int sz) {
    sx = clampInt(sx, 1, 64);
    sy = clampInt(sy, 1, 64);
    sz = clampInt(sz, 1, 64);
    const float hx = size.x * 0.5f, hy = size.y * 0.5f, hz = size.z * 0.5f;
    PolyMesh m;
    addGrid(m, {-hx, -hy, hz}, {2 * hx, 0, 0}, {0, 2 * hy, 0}, sx, sy);    // +z
    addGrid(m, {hx, -hy, -hz}, {-2 * hx, 0, 0}, {0, 2 * hy, 0}, sx, sy);   // -z
    addGrid(m, {hx, -hy, hz}, {0, 0, -2 * hz}, {0, 2 * hy, 0}, sz, sy);    // +x
    addGrid(m, {-hx, -hy, -hz}, {0, 0, 2 * hz}, {0, 2 * hy, 0}, sz, sy);   // -x
    addGrid(m, {-hx, hy, hz}, {2 * hx, 0, 0}, {0, 0, -2 * hz}, sx, sz);    // +y
    addGrid(m, {-hx, -hy, -hz}, {2 * hx, 0, 0}, {0, 0, 2 * hz}, sx, sz);   // -y
    return weldVertices(m, 1e-5f * std::max({1.0f, size.x, size.y, size.z}));
}

PolyMesh makeSpherePoly(float radius, int segments) {
    segments = clampInt(segments, 1, 64);
    PolyMesh m = makeBoxPoly({2.0f, 2.0f, 2.0f}, segments, segments, segments);
    for (Vec3& p : m.verts) {
        const float x2 = p.x * p.x, y2 = p.y * p.y, z2 = p.z * p.z;
        const float nx = p.x * std::sqrt(std::max(0.0f, 1.0f - y2 * 0.5f - z2 * 0.5f + y2 * z2 / 3.0f));
        const float ny = p.y * std::sqrt(std::max(0.0f, 1.0f - z2 * 0.5f - x2 * 0.5f + z2 * x2 / 3.0f));
        const float nz = p.z * std::sqrt(std::max(0.0f, 1.0f - x2 * 0.5f - y2 * 0.5f + x2 * y2 / 3.0f));
        p = Vec3{nx, ny, nz} * radius;
    }
    return m;
}

PolyMesh makeCylinderPoly(float radius, float height, int radial, int heightSegments, bool caps) {
    radial = clampInt(radial, 3, 256);
    heightSegments = clampInt(heightSegments, 1, 128);
    const float h = height * 0.5f;
    PolyMesh m;
    for (int j = 0; j <= heightSegments; ++j) {
        const float y = -h + height * static_cast<float>(j) / static_cast<float>(heightSegments);
        for (int i = 0; i < radial; ++i) {
            const float t = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(radial);
            m.verts.push_back({radius * std::cos(t), y, radius * std::sin(t)});
        }
    }
    auto id = [&](int i, int j) { return u32(static_cast<size_t>(j) * radial + static_cast<size_t>(i % radial)); };
    for (int j = 0; j < heightSegments; ++j) {
        for (int i = 0; i < radial; ++i) {
            m.faces.push_back({id(i, j), id(i, j + 1), id(i + 1, j + 1), id(i + 1, j)});
        }
    }
    if (caps) {
        std::vector<uint32_t> top, bottom;
        for (int i = radial - 1; i >= 0; --i) top.push_back(id(i, heightSegments));
        for (int i = 0; i < radial; ++i) bottom.push_back(id(i, 0));
        m.faces.push_back(top);
        m.faces.push_back(bottom);
    }
    return m;
}

PolyMesh makePipePoly(float outerRadius, float innerRadius, float height, int radial, int heightSegments) {
    radial = clampInt(radial, 3, 256);
    heightSegments = clampInt(heightSegments, 1, 128);
    outerRadius = std::max(outerRadius, 0.001f);
    innerRadius = std::clamp(innerRadius, outerRadius * 0.05f, outerRadius * 0.95f);
    const float halfHeight = height * 0.5f;
    PolyMesh mesh;
    const uint32_t ringSize = static_cast<uint32_t>(radial);
    const uint32_t outerBottom = 0;
    const uint32_t innerBottom = ringSize * static_cast<uint32_t>(heightSegments + 1);

    // Outer and inner walls use independent rings so the wall has real thickness.
    for (int level = 0; level <= heightSegments; ++level) {
        const float y = -halfHeight + height * static_cast<float>(level) / static_cast<float>(heightSegments);
        for (int i = 0; i < radial; ++i) {
            const float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(radial);
            const float cs = std::cos(angle);
            const float sn = std::sin(angle);
            mesh.verts.push_back({outerRadius * cs, y, outerRadius * sn});
        }
    }
    for (int level = 0; level <= heightSegments; ++level) {
        const float y = -halfHeight + height * static_cast<float>(level) / static_cast<float>(heightSegments);
        for (int i = 0; i < radial; ++i) {
            const float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(radial);
            const float cs = std::cos(angle);
            const float sn = std::sin(angle);
            mesh.verts.push_back({innerRadius * cs, y, innerRadius * sn});
        }
    }

    auto outer = [&](int level, int i) {
        return outerBottom + static_cast<uint32_t>(level) * ringSize + static_cast<uint32_t>((i + radial) % radial);
    };
    auto inner = [&](int level, int i) {
        return innerBottom + static_cast<uint32_t>(level) * ringSize + static_cast<uint32_t>((i + radial) % radial);
    };
    for (int level = 0; level < heightSegments; ++level) {
        for (int i = 0; i < radial; ++i) {
            const int next = (i + 1) % radial;
            // The outer wall faces away from the axis; the inner wall faces into the bore.
            mesh.faces.push_back({outer(level, i), outer(level + 1, i), outer(level + 1, next), outer(level, next)});
            mesh.faces.push_back({inner(level, i), inner(level, next), inner(level + 1, next), inner(level + 1, i)});
        }
    }
    for (int i = 0; i < radial; ++i) {
        const int next = (i + 1) % radial;
        mesh.faces.push_back({outer(0, i), outer(0, next), inner(0, next), inner(0, i)});
        mesh.faces.push_back({outer(heightSegments, i), inner(heightSegments, i), inner(heightSegments, next), outer(heightSegments, next)});
    }
    return mesh;
}

PolyMesh makeConePoly(float radius, float height, int radial, bool cap) {
    radial = clampInt(radial, 3, 256);
    const float h = height * 0.5f;
    PolyMesh m;
    m.verts.push_back({0.0f, h, 0.0f});
    for (int i = 0; i < radial; ++i) {
        const float t = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(radial);
        m.verts.push_back({radius * std::cos(t), -h, radius * std::sin(t)});
    }
    for (int i = 0; i < radial; ++i) {
        m.faces.push_back({u32(1 + i), 0u, u32(1 + (i + 1) % radial)});
    }
    if (cap) {
        std::vector<uint32_t> base;
        for (int i = 0; i < radial; ++i) base.push_back(u32(1 + i));
        m.faces.push_back(base);
    }
    return m;
}

PolyMesh makeTorusPoly(float majorRadius, float minorRadius, int segments, int sides) {
    segments = clampInt(segments, 3, 256);
    sides = clampInt(sides, 3, 128);
    PolyMesh m;
    for (int i = 0; i < segments; ++i) {
        const float u = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
        for (int j = 0; j < sides; ++j) {
            const float v = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(sides);
            const float ring = majorRadius + minorRadius * std::cos(v);
            m.verts.push_back({ring * std::cos(u), minorRadius * std::sin(v), ring * std::sin(u)});
        }
    }
    auto id = [&](int i, int j) { return u32(static_cast<size_t>(i % segments) * sides + static_cast<size_t>(j % sides)); };
    for (int i = 0; i < segments; ++i) {
        for (int j = 0; j < sides; ++j) {
            m.faces.push_back({id(i, j), id(i, j + 1), id(i + 1, j + 1), id(i + 1, j)});
        }
    }
    return m;
}

PolyMesh makePlanePoly(float width, float depth, int sx, int sz) {
    PolyMesh m;
    addGrid(m, {-width * 0.5f, 0.0f, depth * 0.5f}, {width, 0, 0}, {0, 0, -depth}, clampInt(sx, 1, 256),
            clampInt(sz, 1, 256));
    return m;
}

PolyMesh makeLathe(const Profile& profile, int segments) {
    if (profile.size() < 2) throw std::runtime_error("profile needs at least 2 points");
    segments = clampInt(segments, 3, 256);
    PolyMesh m;
    std::vector<std::vector<uint32_t>> ring(profile.size());
    for (size_t k = 0; k < profile.size(); ++k) {
        const float r = profile[k].first;
        const float y = profile[k].second;
        if (r <= 1e-6f) {
            ring[k].push_back(u32(m.verts.size()));
            m.verts.push_back({0.0f, y, 0.0f});
        } else {
            for (int i = 0; i < segments; ++i) {
                const float t = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
                ring[k].push_back(u32(m.verts.size()));
                m.verts.push_back({r * std::cos(t), y, r * std::sin(t)});
            }
        }
    }
    auto at = [&](size_t k, int i) -> uint32_t {
        const auto& r = ring[k];
        return r.size() == 1 ? r[0] : r[static_cast<size_t>(i) % r.size()];
    };
    for (size_t k = 0; k + 1 < profile.size(); ++k) {
        if (ring[k].size() == 1 && ring[k + 1].size() == 1) continue;
        for (int i = 0; i < segments; ++i) {
            addFace(m, {at(k, i), at(k + 1, i), at(k + 1, i + 1), at(k, i + 1)});
        }
    }
    return m;
}

PolyMesh makeLoft(const std::vector<std::vector<Vec3>>& sections, bool closedRings, bool capStart,
                  bool capEnd, int orient) {
    if (sections.size() < 2) throw std::runtime_error("loft needs at least 2 sections");
    const size_t n = sections[0].size();
    if (n < (closedRings ? 3u : 2u)) throw std::runtime_error("each section needs more points");
    for (const auto& s : sections) {
        if (s.size() != n) throw std::runtime_error("all sections must have the same number of points");
    }
    PolyMesh m;
    for (const auto& s : sections) {
        for (const Vec3& p : s) m.verts.push_back(p);
    }
    auto id = [&](size_t s, size_t i) { return u32(s * n + i); };
    const size_t segs = closedRings ? n : n - 1;
    for (size_t s = 0; s + 1 < sections.size(); ++s) {
        for (size_t i = 0; i < segs; ++i) {
            addFace(m, {id(s, i), id(s, (i + 1) % n), id(s + 1, (i + 1) % n), id(s + 1, i)});
        }
    }
    if (closedRings) {
        if (capStart) {
            std::vector<uint32_t> f;
            for (size_t i = n; i-- > 0;) f.push_back(id(0, i));
            addFace(m, f);
        }
        if (capEnd) {
            std::vector<uint32_t> f;
            for (size_t i = 0; i < n; ++i) f.push_back(id(sections.size() - 1, i));
            addFace(m, f);
        }
    }
    Vec3 mn, mx;
    meshBounds(m, mn, mx);
    const Vec3 ext = mx - mn;
    const float extent = std::max({ext.x, ext.y, ext.z, 1e-3f});
    PolyMesh w = weldVertices(m, 1e-6f * extent);
    if (orient == 2) {
        w = flipFaces(w);
    } else if (orient == 0) {
        autoOrient(w);
    }
    return w;
}

PolyMesh makeTube(const std::vector<Vec3>& path, const std::vector<float>& radii, int sides, bool caps) {
    if (path.size() < 2) throw std::runtime_error("path needs at least 2 points");
    sides = clampInt(sides, 3, 128);
    std::vector<std::vector<Vec3>> sections;
    Vec3 normal;
    for (size_t k = 0; k < path.size(); ++k) {
        const Vec3 prev = path[k == 0 ? 0 : k - 1];
        const Vec3 next = path[k + 1 < path.size() ? k + 1 : k];
        const Vec3 t = normalize(next - prev);
        if (length(t) < 0.5f) throw std::runtime_error("path has repeated points");
        if (k == 0) {
            normal = perpendicular(t);
        } else {
            normal = normal - t * dot(normal, t);
            normal = length(normal) < 1e-5f ? perpendicular(t) : normalize(normal);
        }
        const Vec3 binormal = cross(t, normal);
        const float radius = radii.size() == 1 ? radii[0] : radii[std::min(k, radii.size() - 1)];
        std::vector<Vec3> ring;
        for (int i = 0; i < sides; ++i) {
            const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(sides);
            ring.push_back(path[k] + (normal * std::cos(a) + binormal * std::sin(a)) * radius);
        }
        sections.push_back(std::move(ring));
    }
    return makeLoft(sections, true, caps, caps, 0);
}

PolyMesh makePrism(const Profile& polygon, float depth) {
    if (polygon.size() < 3) throw std::runtime_error("polygon needs at least 3 points");
    Profile p = polygon;
    double area = 0.0;
    for (size_t i = 0; i < p.size(); ++i) {
        const auto& a = p[i];
        const auto& b = p[(i + 1) % p.size()];
        area += static_cast<double>(a.first) * b.second - static_cast<double>(b.first) * a.second;
    }
    if (area < 0.0) std::reverse(p.begin(), p.end());
    const size_t n = p.size();
    const float h = depth * 0.5f;
    PolyMesh m;
    for (size_t i = 0; i < n; ++i) m.verts.push_back({p[i].first, p[i].second, -h});  // 0..n-1 atrás
    for (size_t i = 0; i < n; ++i) m.verts.push_back({p[i].first, p[i].second, h});   // n..2n-1 frente
    std::vector<uint32_t> front, back;
    for (size_t i = 0; i < n; ++i) front.push_back(u32(n + i));
    for (size_t i = n; i-- > 0;) back.push_back(u32(i));
    m.faces.push_back(front);
    m.faces.push_back(back);
    for (size_t i = 0; i < n; ++i) {
        const size_t j = (i + 1) % n;
        m.faces.push_back({u32(i), u32(j), u32(n + j), u32(n + i)});
    }
    return m;
}

PolyMesh parseObj(const std::string& text, float scale) {
    PolyMesh m;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.size() < 2) continue;
        if (line[0] == 'v' && line[1] == ' ') {
            std::istringstream ls(line.substr(2));
            float x = 0.0f, y = 0.0f, z = 0.0f;
            ls >> x >> y >> z;
            m.verts.push_back({x * scale, y * scale, z * scale});
            if (m.verts.size() > 1000000) throw std::runtime_error("OBJ has too many vertices");
        } else if (line[0] == 'f' && line[1] == ' ') {
            std::istringstream ls(line.substr(2));
            std::string tok;
            std::vector<uint32_t> face;
            while (ls >> tok) {
                long idx = std::strtol(tok.c_str(), nullptr, 10);
                if (idx < 0) {
                    idx = static_cast<long>(m.verts.size()) + idx;
                } else {
                    idx -= 1;
                }
                if (idx < 0 || static_cast<size_t>(idx) >= m.verts.size()) {
                    throw std::runtime_error("OBJ face index out of range");
                }
                face.push_back(u32(static_cast<size_t>(idx)));
            }
            if (face.size() >= 3) m.faces.push_back(std::move(face));
            if (m.faces.size() > 1000000) throw std::runtime_error("OBJ has too many faces");
        }
    }
    if (m.faces.empty()) throw std::runtime_error("OBJ has no faces");
    return m;
}

// ---------------------------------------------------------------- Subdivisão Catmull-Clark

namespace {

PolyMesh catmullClarkOnce(const PolyMesh& m) {
    const size_t V = m.verts.size();
    const size_t F = m.faces.size();

    std::vector<Vec3> facePt(F);
    for (size_t f = 0; f < F; ++f) facePt[f] = faceCenter(m, f);

    struct Edge {
        uint32_t a = 0, b = 0;
        int faces = 0;
        int f0 = -1, f1 = -1;
        bool crease = false;
    };
    std::vector<Edge> edges;
    std::unordered_map<uint64_t, size_t> edgeIndex;
    std::unordered_set<uint64_t> creaseSet;
    for (const auto& c : m.creases) creaseSet.insert(edgeKey(c.first, c.second));

    std::vector<std::vector<size_t>> vEdges(V);
    std::vector<std::vector<size_t>> vFaces(V);
    for (size_t f = 0; f < F; ++f) {
        const auto& face = m.faces[f];
        const size_t n = face.size();
        for (size_t k = 0; k < n; ++k) {
            const uint32_t a = face[k];
            const uint32_t b = face[(k + 1) % n];
            vFaces[a].push_back(f);
            const uint64_t key = edgeKey(a, b);
            auto it = edgeIndex.find(key);
            size_t ei = 0;
            if (it == edgeIndex.end()) {
                ei = edges.size();
                edgeIndex.emplace(key, ei);
                Edge e;
                e.a = std::min(a, b);
                e.b = std::max(a, b);
                e.crease = creaseSet.count(key) > 0;
                edges.push_back(e);
                vEdges[a].push_back(ei);
                vEdges[b].push_back(ei);
            } else {
                ei = it->second;
            }
            Edge& e = edges[ei];
            if (e.faces == 0) e.f0 = static_cast<int>(f);
            else if (e.faces == 1) e.f1 = static_cast<int>(f);
            e.faces++;
        }
    }

    PolyMesh out;
    out.verts.resize(V + F + edges.size());

    // pontos das arestas
    for (size_t ei = 0; ei < edges.size(); ++ei) {
        const Edge& e = edges[ei];
        const Vec3 A = m.verts[e.a], B = m.verts[e.b];
        if (e.faces == 2 && !e.crease) {
            out.verts[V + F + ei] = (A + B + facePt[static_cast<size_t>(e.f0)] + facePt[static_cast<size_t>(e.f1)]) * 0.25f;
        } else {
            out.verts[V + F + ei] = (A + B) * 0.5f;
        }
    }
    // pontos das faces
    for (size_t f = 0; f < F; ++f) out.verts[V + f] = facePt[f];

    // vértices originais
    for (size_t v = 0; v < V; ++v) {
        const Vec3 P = m.verts[v];
        const auto& ve = vEdges[v];
        if (ve.empty()) {
            out.verts[v] = P;
            continue;
        }
        std::vector<size_t> sharp;
        for (size_t ei : ve) {
            if (edges[ei].faces != 2 || edges[ei].crease) sharp.push_back(ei);
        }
        if (sharp.empty()) {
            const size_t n = ve.size();
            if (n < 3 || vFaces[v].empty()) {
                out.verts[v] = P;
                continue;
            }
            Vec3 favg;
            for (size_t f : vFaces[v]) favg += facePt[f];
            favg = favg / static_cast<float>(vFaces[v].size());
            Vec3 r;
            for (size_t ei : ve) r += (m.verts[edges[ei].a] + m.verts[edges[ei].b]) * 0.5f;
            r = r / static_cast<float>(n);
            out.verts[v] = (favg + r * 2.0f + P * static_cast<float>(n - 3)) / static_cast<float>(n);
        } else if (sharp.size() == 2) {
            auto other = [&](size_t ei) {
                const Edge& e = edges[ei];
                return m.verts[e.a == static_cast<uint32_t>(v) ? e.b : e.a];
            };
            out.verts[v] = (other(sharp[0]) + other(sharp[1]) + P * 6.0f) / 8.0f;
        } else {
            out.verts[v] = P;
        }
    }

    // faces novas: um quad por canto
    for (size_t f = 0; f < F; ++f) {
        const auto& face = m.faces[f];
        const size_t n = face.size();
        for (size_t k = 0; k < n; ++k) {
            const uint32_t v = face[k];
            const uint32_t next = face[(k + 1) % n];
            const uint32_t prev = face[(k + n - 1) % n];
            const uint32_t eNext = u32(V + F + edgeIndex.at(edgeKey(v, next)));
            const uint32_t ePrev = u32(V + F + edgeIndex.at(edgeKey(prev, v)));
            out.faces.push_back({v, eNext, u32(V + f), ePrev});
        }
    }

    // as arestas duras continuam duras (cada metade)
    for (size_t ei = 0; ei < edges.size(); ++ei) {
        if (!edges[ei].crease) continue;
        const uint32_t p = u32(V + F + ei);
        out.creases.push_back(sortedPair(edges[ei].a, p));
        out.creases.push_back(sortedPair(edges[ei].b, p));
    }
    return out;
}

}  // namespace

size_t estimateSubdividedFaces(const PolyMesh& m, int levels) {
    if (levels <= 0) return m.faces.size();
    constexpr size_t kSaturation = 250001;
    size_t corners = 0;
    for (const auto& face : m.faces) {
        if (face.size() > kSaturation - std::min(corners, kSaturation)) return kSaturation;
        corners += face.size();
        if (corners >= kSaturation) return kSaturation;
    }
    size_t total = corners;
    for (int i = 1; i < std::min(levels, 8); ++i) {
        if (total > kSaturation / 4) return kSaturation;
        total *= 4;
    }
    return total;
}

PolyMesh catmullClark(const PolyMesh& m, int levels) {
    if (m.empty() || m.verts.size() > 250000 || m.faces.size() > 250000) return {};
    for (const Vec3& v : m.verts)
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) return {};
    for (const auto& face : m.faces) {
        if (face.size() < 3 || face.size() > 4096) return {};
        for (uint32_t index : face) if (index >= m.verts.size()) return {};
    }
    levels = std::clamp(levels, 0, 3);
    if (estimateSubdividedFaces(m, levels) > 180000) return {};
    PolyMesh cur = m;
    for (int i = 0; i < levels; ++i) {
        cur = catmullClarkOnce(cur);
        if (cur.empty() || cur.verts.size() > 250000 || cur.faces.size() > 250000) return {};
    }
    return cur;
}

// ---------------------------------------------------------------- Extrusão, inset, loop cut

PolyMesh extrudeFaces(const PolyMesh& m, const std::vector<int>& sel, float distance, bool useDir,
                      Vec3 dir, float scale) {
    std::vector<char> isSel(m.faces.size(), 0);
    for (int f : sel) {
        if (f >= 0 && static_cast<size_t>(f) < m.faces.size()) isSel[static_cast<size_t>(f)] = 1;
    }
    std::unordered_map<uint32_t, Vec3> accumulated;
    std::unordered_set<uint64_t> directed;
    for (size_t f = 0; f < m.faces.size(); ++f) {
        if (!isSel[f]) continue;
        const auto& face = m.faces[f];
        const Vec3 n = newellNormal(m, face);
        for (size_t k = 0; k < face.size(); ++k) {
            accumulated[face[k]] += n;
            directed.insert(dirKey(face[k], face[(k + 1) % face.size()]));
        }
    }
    if (accumulated.empty()) throw std::runtime_error("no valid faces selected");

    Vec3 center;
    for (const auto& kv : accumulated) center += m.verts[kv.first];
    center = center / static_cast<float>(accumulated.size());

    PolyMesh out = m;
    std::unordered_map<uint32_t, uint32_t> remap;
    for (const auto& kv : accumulated) {
        const Vec3 d = useDir ? normalize(dir) : normalize(kv.second);
        Vec3 p = m.verts[kv.first] + d * distance;
        if (scale != 1.0f) p = center + (p - center) * scale;
        remap[kv.first] = u32(out.verts.size());
        out.verts.push_back(p);
    }
    for (size_t f = 0; f < m.faces.size(); ++f) {
        if (!isSel[f]) continue;
        const auto& face = m.faces[f];
        const size_t n = face.size();
        std::vector<uint32_t> top(n);
        for (size_t k = 0; k < n; ++k) top[k] = remap[face[k]];
        for (size_t k = 0; k < n; ++k) {
            const uint32_t u = face[k];
            const uint32_t v = face[(k + 1) % n];
            if (directed.count(dirKey(v, u)) == 0) addFace(out, {u, v, remap[v], remap[u]});
        }
        out.faces[f] = top;
    }
    return out;
}

PolyMesh insetFaces(const PolyMesh& m, const std::vector<int>& sel, float amount) {
    amount = std::max(0.0f, std::min(amount, 0.95f));
    PolyMesh out = m;
    for (int fi : sel) {
        if (fi < 0 || static_cast<size_t>(fi) >= m.faces.size()) continue;
        const size_t f = static_cast<size_t>(fi);
        const auto& face = m.faces[f];
        const size_t n = face.size();
        const Vec3 c = faceCenter(m, f);
        std::vector<uint32_t> inner(n);
        for (size_t k = 0; k < n; ++k) {
            inner[k] = u32(out.verts.size());
            out.verts.push_back(lerp(m.verts[face[k]], c, amount));
        }
        for (size_t k = 0; k < n; ++k) {
            addFace(out, {face[k], face[(k + 1) % n], inner[(k + 1) % n], inner[k]});
        }
        out.faces[f] = inner;
    }
    return out;
}

PolyMesh connectVertices(const PolyMesh& m, uint32_t a, uint32_t b) {
    if (a >= m.verts.size() || b >= m.verts.size() || a == b) {
        throw std::runtime_error("invalid vertex pair");
    }
    PolyMesh out;
    out.verts = m.verts;
    out.creases = m.creases;
    bool connected = false;

    for (const auto& face : m.faces) {
        auto ia = std::find(face.begin(), face.end(), a);
        auto ib = std::find(face.begin(), face.end(), b);
        if (ia == face.end() || ib == face.end()) {
            out.faces.push_back(face);
            continue;
        }
        const size_t ai = static_cast<size_t>(std::distance(face.begin(), ia));
        const size_t bi = static_cast<size_t>(std::distance(face.begin(), ib));
        const size_t count = face.size();
        if ((ai + 1) % count == bi || (bi + 1) % count == ai) {
            out.faces.push_back(face);
            continue;
        }

        std::vector<uint32_t> first;
        for (size_t i = ai;; i = (i + 1) % count) {
            first.push_back(face[i]);
            if (i == bi) break;
        }
        std::vector<uint32_t> second;
        for (size_t i = bi;; i = (i + 1) % count) {
            second.push_back(face[i]);
            if (i == ai) break;
        }
        if (first.size() < 3 || second.size() < 3) {
            out.faces.push_back(face);
            continue;
        }
        addFace(out, first);
        addFace(out, second);
        connected = true;
    }
    if (!connected) throw std::runtime_error("vertices do not share a splittable polygon face");
    return out;
}

PolyMesh splitEdge(const PolyMesh& m, uint32_t a, uint32_t b, float t) {
    if (a == b || a >= m.verts.size() || b >= m.verts.size() ||
        !std::isfinite(t) || m.verts.size() >= 250000) return {};
    t = std::clamp(t, 0.001f, 0.999f);
    const uint64_t key = edgeKey(a, b);
    bool found = false;
    for (const auto& face : m.faces) {
        if (face.size() < 3) return {};
        for (size_t i = 0; i < face.size(); ++i) {
            if (edgeKey(face[i], face[(i + 1) % face.size()]) == key) found = true;
            if (face[i] >= m.verts.size()) return {};
        }
    }
    if (!found) return {};

    PolyMesh out = m;
    const uint32_t inserted = static_cast<uint32_t>(out.verts.size());
    const float ratio = a < b ? t : (1.0f - t);
    out.verts.push_back(lerp(m.verts[a], m.verts[b], ratio));
    for (auto& face : out.faces) {
        std::vector<uint32_t> rebuilt;
        rebuilt.reserve(face.size() + 1);
        for (size_t i = 0; i < face.size(); ++i) {
            const uint32_t p = face[i], q = face[(i + 1) % face.size()];
            rebuilt.push_back(p);
            if (edgeKey(p, q) == key) rebuilt.push_back(inserted);
        }
        face = std::move(rebuilt);
    }

    std::vector<std::pair<uint32_t, uint32_t>> creases;
    creases.reserve(out.creases.size() + 1);
    for (const auto& crease : out.creases) {
        if (edgeKey(crease.first, crease.second) == key) {
            creases.push_back(sortedPair(crease.first, inserted));
            creases.push_back(sortedPair(inserted, crease.second));
        } else {
            creases.push_back(crease);
        }
    }
    out.creases = std::move(creases);
    return out;
}

std::vector<int> edgeLoopFaces(const PolyMesh& m, uint32_t a, uint32_t b) {
    if (a == b || a >= m.verts.size() || b >= m.verts.size()) return {};
    std::unordered_map<uint64_t, std::vector<int>> edgeFaces;
    for (size_t f = 0; f < m.faces.size(); ++f) {
        const auto& face = m.faces[f];
        if (face.size() < 3) return {};
        for (uint32_t v : face) if (v >= m.verts.size()) return {};
        for (size_t k = 0; k < face.size(); ++k)
            edgeFaces[edgeKey(face[k], face[(k + 1) % face.size()])].push_back(static_cast<int>(f));
    }
    const auto start = edgeFaces.find(edgeKey(a, b));
    if (start == edgeFaces.end() || start->second.empty() || start->second.size() > 2) return {};

    auto localEdge = [&](int faceId, uint64_t key) -> int {
        const auto& face = m.faces[static_cast<size_t>(faceId)];
        if (face.size() != 4) return -1;
        for (int k = 0; k < 4; ++k)
            if (edgeKey(face[static_cast<size_t>(k)], face[static_cast<size_t>((k + 1) % 4)]) == key) return k;
        return -1;
    };

    std::vector<int> ordered;
    std::unordered_set<int> visited;
    for (int seed : start->second) {
        int faceId = seed;
        int enter = localEdge(faceId, edgeKey(a, b));
        if (enter < 0) continue;
        while (visited.insert(faceId).second) {
            ordered.push_back(faceId);
            const auto& face = m.faces[static_cast<size_t>(faceId)];
            const uint64_t exitKey = edgeKey(face[static_cast<size_t>((enter + 2) % 4)],
                                             face[static_cast<size_t>((enter + 3) % 4)]);
            const auto adjacent = edgeFaces.find(exitKey);
            if (adjacent == edgeFaces.end() || adjacent->second.size() != 2) break;
            int next = -1;
            for (int candidate : adjacent->second) if (candidate != faceId) next = candidate;
            if (next < 0 || visited.count(next) != 0) break;
            const int nextEnter = localEdge(next, exitKey);
            if (nextEnter < 0) break;
            faceId = next;
            enter = nextEnter;
        }
    }
    return ordered;
}

PolyMesh loopCut(const PolyMesh& m, uint32_t a, uint32_t b, int cuts) {
    cuts = clampInt(cuts, 1, 32);
    const size_t F = m.faces.size();
    std::unordered_map<uint64_t, std::vector<int>> edgeFaces;
    for (size_t f = 0; f < F; ++f) {
        const auto& face = m.faces[f];
        for (size_t k = 0; k < face.size(); ++k) {
            edgeFaces[edgeKey(face[k], face[(k + 1) % face.size()])].push_back(static_cast<int>(f));
        }
    }
    const auto start = edgeFaces.find(edgeKey(a, b));
    if (start == edgeFaces.end()) throw std::runtime_error("edge not found");
    int f0 = -1;
    for (int f : start->second) {
        if (m.faces[static_cast<size_t>(f)].size() == 4) {
            f0 = f;
            break;
        }
    }
    if (f0 < 0) throw std::runtime_error("the edge has no quad face");

    auto localIndex = [&](int f, uint64_t key) -> int {
        const auto& face = m.faces[static_cast<size_t>(f)];
        for (int k = 0; k < 4; ++k) {
            if (edgeKey(face[static_cast<size_t>(k)], face[static_cast<size_t>((k + 1) % 4)]) == key) return k;
        }
        return -1;
    };

    // Anel de faces: para cada face, k = índice local de uma das duas arestas paralelas cortadas.
    std::unordered_map<int, int> ring;
    const int k0 = localIndex(f0, edgeKey(a, b));
    ring[f0] = k0;
    for (int dir = 0; dir < 2; ++dir) {
        int f = f0;
        int enterK = dir == 0 ? k0 : (k0 + 2) % 4;
        while (true) {
            const auto& face = m.faces[static_cast<size_t>(f)];
            const int exitK = (enterK + 2) % 4;
            const uint64_t key = edgeKey(face[static_cast<size_t>(exitK)], face[static_cast<size_t>((exitK + 1) % 4)]);
            const auto& adj = edgeFaces.at(key);
            int g = -1;
            for (int cand : adj) {
                if (cand != f) g = cand;
            }
            if (g < 0 || adj.size() != 2 || m.faces[static_cast<size_t>(g)].size() != 4 || ring.count(g) > 0) break;
            const int kg = localIndex(g, key);
            ring[g] = kg;
            f = g;
            enterK = kg;
        }
    }

    PolyMesh out = m;
    std::unordered_map<uint64_t, std::vector<uint32_t>> cutVerts;  // do vértice menor para o maior
    auto ensureCuts = [&](uint32_t p, uint32_t q) {
        const uint64_t key = edgeKey(p, q);
        if (cutVerts.count(key) > 0) return;
        const uint32_t lo = std::min(p, q), hi = std::max(p, q);
        std::vector<uint32_t> ids;
        for (int i = 1; i <= cuts; ++i) {
            ids.push_back(u32(out.verts.size()));
            out.verts.push_back(lerp(m.verts[lo], m.verts[hi], static_cast<float>(i) / static_cast<float>(cuts + 1)));
        }
        cutVerts[key] = ids;
    };
    auto cutAt = [&](uint32_t p, uint32_t q, int i) -> uint32_t {
        const auto& ids = cutVerts.at(edgeKey(p, q));
        return p < q ? ids[static_cast<size_t>(i - 1)] : ids[static_cast<size_t>(cuts - i)];
    };
    for (const auto& kv : ring) {
        const auto& face = m.faces[static_cast<size_t>(kv.first)];
        const size_t k = static_cast<size_t>(kv.second);
        ensureCuts(face[k], face[(k + 1) % 4]);
        ensureCuts(face[(k + 2) % 4], face[(k + 3) % 4]);
    }

    std::vector<std::vector<uint32_t>> extra;
    for (size_t f = 0; f < F; ++f) {
        const auto& face = m.faces[f];
        const auto it = ring.find(static_cast<int>(f));
        if (it != ring.end()) {
            const size_t k = static_cast<size_t>(it->second);
            const uint32_t q0 = face[k], q1 = face[(k + 1) % 4], q2 = face[(k + 2) % 4], q3 = face[(k + 3) % 4];
            auto A = [&](int i) -> uint32_t { return i == 0 ? q0 : (i == cuts + 1 ? q1 : cutAt(q0, q1, i)); };
            auto B = [&](int i) -> uint32_t { return i == 0 ? q3 : (i == cuts + 1 ? q2 : cutAt(q3, q2, i)); };
            for (int i = 0; i <= cuts; ++i) {
                std::vector<uint32_t> quad = {A(i), A(i + 1), B(i + 1), B(i)};
                if (i == 0) {
                    out.faces[f] = quad;
                } else {
                    extra.push_back(quad);
                }
            }
        } else {
            // face fora do anel que partilha uma aresta cortada: recebe os novos vértices
            std::vector<uint32_t> g;
            bool changed = false;
            for (size_t k = 0; k < face.size(); ++k) {
                const uint32_t p = face[k], q = face[(k + 1) % face.size()];
                g.push_back(p);
                if (cutVerts.count(edgeKey(p, q)) > 0) {
                    for (int i = 1; i <= cuts; ++i) g.push_back(cutAt(p, q, i));
                    changed = true;
                }
            }
            if (changed) out.faces[f] = g;
        }
    }
    out.faces.insert(out.faces.end(), extra.begin(), extra.end());

    // arestas duras cortadas viram cadeias de arestas duras
    std::vector<std::pair<uint32_t, uint32_t>> newCreases;
    for (const auto& c : m.creases) {
        const uint64_t key = edgeKey(c.first, c.second);
        const auto it = cutVerts.find(key);
        if (it == cutVerts.end()) {
            newCreases.push_back(c);
            continue;
        }
        uint32_t prev = std::min(c.first, c.second);
        for (uint32_t id : it->second) {
            newCreases.push_back(sortedPair(prev, id));
            prev = id;
        }
        newCreases.push_back(sortedPair(prev, std::max(c.first, c.second)));
    }
    out.creases = newCreases;
    return out;
}

PolyMesh solidify(const PolyMesh& m, float thickness) {
    const size_t V = m.verts.size();
    const std::vector<Vec3> normals = vertexNormals(m);
    PolyMesh out = m;
    for (size_t i = 0; i < V; ++i) out.verts.push_back(m.verts[i] - normals[i] * thickness);
    std::unordered_set<uint64_t> directed;
    for (const auto& f : m.faces) {
        for (size_t k = 0; k < f.size(); ++k) directed.insert(dirKey(f[k], f[(k + 1) % f.size()]));
    }
    for (const auto& f : m.faces) {
        std::vector<uint32_t> g;
        for (size_t k = f.size(); k-- > 0;) g.push_back(f[k] + u32(V));
        out.faces.push_back(g);
    }
    for (const auto& f : m.faces) {
        for (size_t k = 0; k < f.size(); ++k) {
            const uint32_t u = f[k], v = f[(k + 1) % f.size()];
            if (directed.count(dirKey(v, u)) == 0) addFace(out, {v, u, u + u32(V), v + u32(V)});
        }
    }
    for (const auto& c : m.creases) out.creases.push_back({c.first + u32(V), c.second + u32(V)});
    return out;
}

PolyMesh smoothMesh(const PolyMesh& m, int iterations, float factor) {
    iterations = clampInt(iterations, 1, 50);
    factor = std::max(0.0f, std::min(factor, 1.0f));
    const size_t V = m.verts.size();
    std::unordered_map<uint64_t, int> edgeCount;
    for (const auto& f : m.faces) {
        for (size_t k = 0; k < f.size(); ++k) edgeCount[edgeKey(f[k], f[(k + 1) % f.size()])]++;
    }
    std::vector<char> locked(V, 0);
    std::vector<std::vector<uint32_t>> nbr(V);
    for (const auto& kv : edgeCount) {
        const uint32_t a = keyLo(kv.first), b = keyHi(kv.first);
        nbr[a].push_back(b);
        nbr[b].push_back(a);
        if (kv.second != 2) locked[a] = locked[b] = 1;
    }
    for (const auto& c : m.creases) locked[c.first] = locked[c.second] = 1;
    PolyMesh out = m;
    for (int it = 0; it < iterations; ++it) {
        std::vector<Vec3> next = out.verts;
        for (size_t v = 0; v < V; ++v) {
            if (locked[v] || nbr[v].empty()) continue;
            Vec3 avg;
            for (uint32_t n : nbr[v]) avg += out.verts[n];
            avg = avg / static_cast<float>(nbr[v].size());
            next[v] = lerp(out.verts[v], avg, factor);
        }
        out.verts = next;
    }
    return out;
}

PolyMesh creaseByAngle(const PolyMesh& m, float angleDegrees) {
    PolyMesh out = m;
    const float cosT = std::cos(radians(angleDegrees));
    std::unordered_map<uint64_t, std::vector<size_t>> ef;
    for (size_t f = 0; f < m.faces.size(); ++f) {
        const auto& face = m.faces[f];
        for (size_t k = 0; k < face.size(); ++k) ef[edgeKey(face[k], face[(k + 1) % face.size()])].push_back(f);
    }
    std::unordered_set<uint64_t> existing;
    for (const auto& c : m.creases) existing.insert(edgeKey(c.first, c.second));
    for (const auto& kv : ef) {
        if (kv.second.size() != 2 || existing.count(kv.first) > 0) continue;
        const Vec3 n0 = faceNormal(m, kv.second[0]);
        const Vec3 n1 = faceNormal(m, kv.second[1]);
        if (dot(n0, n1) < cosT) out.creases.push_back({keyLo(kv.first), keyHi(kv.first)});
    }
    return out;
}

PolyMesh setCreases(const PolyMesh& m, const std::vector<std::pair<uint32_t, uint32_t>>& edges, bool enable,
                    bool clearAll) {
    PolyMesh out = m;
    if (clearAll) {
        out.creases.clear();
        return out;
    }
    std::unordered_set<uint64_t> valid;
    for (const auto& f : m.faces) {
        for (size_t k = 0; k < f.size(); ++k) valid.insert(edgeKey(f[k], f[(k + 1) % f.size()]));
    }
    std::unordered_set<uint64_t> current;
    for (const auto& c : out.creases) current.insert(edgeKey(c.first, c.second));
    for (const auto& e : edges) {
        const uint64_t key = edgeKey(e.first, e.second);
        if (valid.count(key) == 0) throw std::runtime_error("edge does not exist");
        if (enable) {
            if (current.insert(key).second) out.creases.push_back(sortedPair(e.first, e.second));
        } else if (current.erase(key) > 0) {
            out.creases.erase(std::remove_if(out.creases.begin(), out.creases.end(),
                                             [&](const std::pair<uint32_t, uint32_t>& c) {
                                                 return edgeKey(c.first, c.second) == key;
                                             }),
                              out.creases.end());
        }
    }
    return out;
}

}  // namespace core
