#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/math.h"

namespace core {

// Malha de polígonos (quads, triângulos e n-gons) com arestas duras (creases).
// É a malha "editável": as ferramentas de modelagem trabalham sobre ela.
struct PolyMesh {
    std::vector<Vec3> verts;
    std::vector<std::vector<uint32_t>> faces;       // sentido anti-horário visto de fora
    std::vector<std::pair<uint32_t, uint32_t>> creases;  // arestas duras (a < b)

    bool empty() const { return verts.empty() || faces.empty(); }
};

using Profile = std::vector<std::pair<float, float>>;

// ---------- Consultas ----------
Vec3 newellNormal(const PolyMesh& m, const std::vector<uint32_t>& face);  // não normalizada (|n| = 2*área)
Vec3 faceNormal(const PolyMesh& m, size_t face);
Vec3 faceCenter(const PolyMesh& m, size_t face);
void meshBounds(const PolyMesh& m, Vec3& mn, Vec3& mx);
std::vector<Vec3> vertexNormals(const PolyMesh& m);

// ---------- Geradores (lançam std::runtime_error se os dados forem inválidos) ----------
PolyMesh makeBoxPoly(Vec3 size, int sx, int sy, int sz);
PolyMesh makeSpherePoly(float radius, int segments);  // esfera de quads (cubo projetado)
PolyMesh makeCylinderPoly(float radius, float height, int radial, int heightSegments, bool caps);
PolyMesh makePipePoly(float outerRadius, float innerRadius, float height, int radial, int heightSegments);
PolyMesh makeConePoly(float radius, float height, int radial, bool cap);
PolyMesh makeTorusPoly(float majorRadius, float minorRadius, int segments, int sides);
PolyMesh makePlanePoly(float width, float depth, int sx, int sz);
// Perfil (raio, y) de baixo para cima, girado à volta do eixo Y.
PolyMesh makeLathe(const Profile& profile, int segments);
// Liga seções com o mesmo número de pontos. orient: 0 automático, 1 manter, 2 inverter.
PolyMesh makeLoft(const std::vector<std::vector<Vec3>>& sections, bool closedRings, bool capStart,
                  bool capEnd, int orient);
PolyMesh makeTube(const std::vector<Vec3>& path, const std::vector<float>& radii, int sides, bool caps);
// Polígono 2D (x, y) extrudido ao longo de Z, centrado.
PolyMesh makePrism(const Profile& polygon, float depth);
PolyMesh parseObj(const std::string& text, float scale);

// ---------- Operações ----------
PolyMesh weldVertices(const PolyMesh& m, float eps);
PolyMesh flipFaces(const PolyMesh& m);
PolyMesh triangulate(const PolyMesh& m);
PolyMesh mergeMeshes(const PolyMesh& a, const PolyMesh& b);
PolyMesh transformed(const PolyMesh& m, const Mat4& t);
PolyMesh reflectMesh(const PolyMesh& m, int axis);  // espelho sem soldar
PolyMesh mirrorMesh(const PolyMesh& m, int axis, float mergeDistance);  // original + espelho, soldados

size_t estimateSubdividedFaces(const PolyMesh& m, int levels);
PolyMesh catmullClark(const PolyMesh& m, int levels);
PolyMesh extrudeFaces(const PolyMesh& m, const std::vector<int>& faces, float distance, bool useDir,
                      Vec3 dir, float scale);
PolyMesh insetFaces(const PolyMesh& m, const std::vector<int>& faces, float amount);
// Polygon-face bevel with evenly spaced profile rings. Width is relative to face center.
PolyMesh bevelFaces(const PolyMesh& m, const std::vector<int>& faces, float width, float depth, int segments);
PolyMesh loopCut(const PolyMesh& m, uint32_t a, uint32_t b, int cuts);
// Returns ordered face IDs along the quad edge loop seeded by edge (a,b).
std::vector<int> edgeLoopFaces(const PolyMesh& m, uint32_t a, uint32_t b);
PolyMesh connectVertices(const PolyMesh& m, uint32_t a, uint32_t b);
// Splits an existing topological edge at a normalized position (0..1), inserting
// the new vertex into every incident polygon and splitting matching creases.
PolyMesh splitEdge(const PolyMesh& m, uint32_t a, uint32_t b, float t = 0.5f);
PolyMesh solidify(const PolyMesh& m, float thickness);
PolyMesh smoothMesh(const PolyMesh& m, int iterations, float factor);
PolyMesh creaseByAngle(const PolyMesh& m, float angleDegrees);
PolyMesh setCreases(const PolyMesh& m, const std::vector<std::pair<uint32_t, uint32_t>>& edges, bool enable,
                    bool clearAll);

// Booleana (BSP). op: 0 união, 1 diferença (a - b), 2 interseção. As duas malhas têm de estar
// no mesmo espaço, fechadas e com normais para fora.
PolyMesh booleanOp(const PolyMesh& a, const PolyMesh& b, int op);

}  // namespace core
