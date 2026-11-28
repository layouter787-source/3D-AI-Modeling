#include "core/polymesh.h"
#include "core/mesh.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <utility>

using namespace core;

static std::map<std::pair<uint32_t,uint32_t>, int> edgeUse(const PolyMesh& mesh) {
    std::map<std::pair<uint32_t,uint32_t>, int> edges;
    for (const auto& face : mesh.faces) {
        assert(face.size() >= 3);
        for (uint32_t v : face) assert(v < mesh.verts.size());
        for (size_t i = 0; i < face.size(); ++i) {
            uint32_t a = face[i], b = face[(i + 1) % face.size()];
            if (a > b) std::swap(a,b);
            ++edges[{a,b}];
        }
    }
    return edges;
}

static void assertFinite(const PolyMesh& mesh) {
    for (const Vec3& v : mesh.verts)
        assert(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z));
}

int main() {
    PolyMesh box = makeBoxPoly({2.0f,2.0f,2.0f}, 1, 1, 1);
    assert(!box.empty() && box.faces.size() == 6 && box.verts.size() == 8);
    for (const auto& edge : edgeUse(box)) assert(edge.second == 2);

    const std::vector<int> edgeLoop = edgeLoopFaces(box, 0u, 1u);
    assert(!edgeLoop.empty());
    for (int faceId : edgeLoop) assert(faceId >= 0 && static_cast<size_t>(faceId) < box.faces.size());
    assert(edgeLoopFaces(box, 0u, 999u).empty());

    PolyMesh split = splitEdge(box, 0u, 1u, 0.5f);
    assert(split.verts.size() == box.verts.size() + 1);
    assert(split.faces.size() == box.faces.size());
    size_t splitFaceCount = 0;
    for (const auto& face : split.faces) {
        if (face.size() == 5) ++splitFaceCount;
    }
    assert(splitFaceCount == 2); // the edge is shared by two box faces
    for (const auto& edge : edgeUse(split)) assert(edge.second == 2);
    assertFinite(split);
    assert(splitEdge(box, 0u, 999u, 0.5f).empty());
    assert(splitEdge(box, 0u, 1u, std::numeric_limits<float>::quiet_NaN()).empty());
    PolyMesh directedSplit = splitEdge(box, 1u, 0u, 0.25f);
    assert(directedSplit.verts.size() == box.verts.size() + 1);
    const Vec3 expectedDirected = lerp(box.verts[1], box.verts[0], 0.25f);
    assert(length(directedSplit.verts.back() - expectedDirected) < 1e-5f);


    PolyMesh subdiv1 = catmullClark(box, 1);
    assert(subdiv1.faces.size() == 24 && subdiv1.verts.size() == 26);
    for (const auto& edge : edgeUse(subdiv1)) assert(edge.second == 2);
    PolyMesh creasedBox = box;
    creasedBox.creases.push_back({0u, 1u});
    PolyMesh creaseSub = catmullClark(creasedBox, 1);
    assert(!creaseSub.creases.empty());
    for (const auto& crease : creaseSub.creases) {
        assert(crease.first < creaseSub.verts.size() && crease.second < creaseSub.verts.size());
        assert(crease.first != crease.second);
    }

    PolyMesh twoFaceExtrusion = extrudeFaces(box, {0, 1}, 0.2f, false, {}, 1.0f);
    for (const auto& edge : edgeUse(twoFaceExtrusion)) assert(edge.second == 2);
    assertFinite(twoFaceExtrusion);

    PolyMesh subdiv2 = catmullClark(box, 2);
    assert(subdiv2.faces.size() == 96);
    assertFinite(subdiv2);
    for (const auto& edge : edgeUse(subdiv2)) assert(edge.second == 2);

    PolyMesh extruded = extrudeFaces(box, {0}, 0.25f, false, {}, 1.0f);
    assert(extruded.faces.size() == box.faces.size() + 4);
    for (const auto& edge : edgeUse(extruded)) assert(edge.second == 2);
    assertFinite(extruded);

    PolyMesh inset = insetFaces(box, {0}, 0.2f);
    assert(inset.faces.size() == box.faces.size() + 4);
    for (const auto& edge : edgeUse(inset)) assert(edge.second == 2);

    PolyMesh bevel = extrudeFaces(inset, {0}, 0.1f, false, {}, 1.0f);
    for (const auto& edge : edgeUse(bevel)) assert(edge.second == 2);
    assertFinite(bevel);

    PolyMesh plane = makePlanePoly(2.0f, 2.0f, 1, 1);
    PolyMesh planeSub = catmullClark(plane, 1);
    size_t boundaryEdges = 0;
    for (const auto& edge : edgeUse(planeSub)) { assert(edge.second <= 2); if (edge.second == 1) ++boundaryEdges; }
    assert(boundaryEdges == 8); // boundary is retained as an open surface, not capped

    PolyMesh malformed = box;
    malformed.faces[0][0] = 999999u;
    assert(catmullClark(malformed, 1).empty());
    assert(estimateSubdividedFaces(malformed, 3) < 250001);

    PolyMesh dense;
    dense.verts.resize(100000, Vec3{});
    dense.faces.resize(100000, std::vector<uint32_t>{0,1,2,3});
    assert(estimateSubdividedFaces(dense, 3) == 250001);
    assert(catmullClark(dense, 3).empty());

    Mesh render = buildRenderMesh(subdiv2, 35.0f);
    assert(!render.vertices.empty() && !render.indices.empty());
    for (uint32_t index : render.indices) assert(index < render.vertices.size());
    std::cout << "Mesh regression suite passed: subdivision, boundary preservation, extrusion, inset/bevel, malformed and large meshes.\\n";
    return 0;
}
