#include "core/mesh.h"

#include <algorithm>
#include <cmath>

namespace core {

Mesh buildRenderMesh(const PolyMesh& p, float smoothAngleDegrees) {
    Mesh out;
    const size_t F = p.faces.size();

    std::vector<Vec3> fn(F);
    std::vector<float> area(F);
    size_t corners = 0;
    size_t tris = 0;
    for (size_t f = 0; f < F; ++f) {
        const Vec3 n = newellNormal(p, p.faces[f]);
        const float len = length(n);
        area[f] = len;
        fn[f] = len > 1e-12f ? n / len : Vec3{0.0f, 1.0f, 0.0f};
        corners += p.faces[f].size();
        if (p.faces[f].size() >= 3) tris += p.faces[f].size() - 2;
    }

    std::vector<std::vector<uint32_t>> vf(p.verts.size());
    for (size_t f = 0; f < F; ++f) {
        for (uint32_t v : p.faces[f]) vf[v].push_back(static_cast<uint32_t>(f));
    }

    const bool flat = smoothAngleDegrees <= 0.001f;
    const float cosT = std::cos(radians(std::max(0.0f, std::min(smoothAngleDegrees, 180.0f))));

    out.vertices.reserve(corners);
    out.indices.reserve(tris * 3);
    for (size_t f = 0; f < F; ++f) {
        const auto& face = p.faces[f];
        const size_t n = face.size();
        if (n < 3) continue;
        const uint32_t base = static_cast<uint32_t>(out.vertices.size());
        for (size_t k = 0; k < n; ++k) {
            const uint32_t v = face[k];
            Vec3 nn = fn[f];
            if (!flat) {
                Vec3 sum;
                for (uint32_t g : vf[v]) {
                    if (g == f || dot(fn[f], fn[g]) >= cosT - 1e-6f) sum += fn[g] * area[g];
                }
                nn = normalize(sum);
                if (length(nn) < 0.5f) nn = fn[f];
            }
            const Vec3& pos = p.verts[v];
            out.vertices.push_back({pos.x, pos.y, pos.z, nn.x, nn.y, nn.z});
        }
        for (size_t k = 1; k + 1 < n; ++k) {
            out.indices.push_back(base);
            out.indices.push_back(base + static_cast<uint32_t>(k));
            out.indices.push_back(base + static_cast<uint32_t>(k) + 1);
        }
    }
    return out;
}

}  // namespace core
