#include "core/mesh.h"

namespace core {

Mesh makeCube(float size) {
    const float h = size * 0.5f;

    struct Face {
        float n[3];
        float v[4][3];
    };

    // Cada face tem os vértices em sentido anti-horário visto de fora.
    const Face faces[6] = {
        {{0, 0, 1},  {{-h, -h, h},  {h, -h, h},   {h, h, h},   {-h, h, h}}},
        {{0, 0, -1}, {{h, -h, -h},  {-h, -h, -h}, {-h, h, -h}, {h, h, -h}}},
        {{1, 0, 0},  {{h, -h, h},   {h, -h, -h},  {h, h, -h},  {h, h, h}}},
        {{-1, 0, 0}, {{-h, -h, -h}, {-h, -h, h},  {-h, h, h},  {-h, h, -h}}},
        {{0, 1, 0},  {{-h, h, h},   {h, h, h},    {h, h, -h},  {-h, h, -h}}},
        {{0, -1, 0}, {{-h, -h, -h}, {h, -h, -h},  {h, -h, h},  {-h, -h, h}}},
    };

    Mesh mesh;
    mesh.vertices.reserve(24);
    mesh.indices.reserve(36);

    for (const Face& f : faces) {
        const uint16_t base = static_cast<uint16_t>(mesh.vertices.size());
        for (int i = 0; i < 4; ++i) {
            mesh.vertices.push_back({f.v[i][0], f.v[i][1], f.v[i][2],
                                     f.n[0], f.n[1], f.n[2]});
        }
        const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
        for (uint16_t i : idx) {
            mesh.indices.push_back(static_cast<uint16_t>(base + i));
        }
    }
    return mesh;
}

}  // namespace core
