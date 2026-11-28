#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/mesh.h"

namespace core {

void recomputeSmoothNormals(Mesh& mesh);
void recomputeHardNormals(Mesh& mesh);
bool subdivideMesh(Mesh& mesh);
bool triangulateMesh(Mesh& mesh);
void smoothMesh(Mesh& mesh, int iterations, float factor);
int weldVertices(Mesh& mesh, float epsilon);
void mirrorMesh(Mesh& mesh, int axis, bool weldSeam);
bool extrudeFaces(Mesh& mesh, const std::vector<int>& faceIndices, float distance);
bool bevelMesh(Mesh& mesh, float amount, int segments);
void appendMesh(Mesh& dst, const Mesh& other);
Mesh extractFaces(const Mesh& mesh, const std::vector<int>& faceIndices);
void removeFaces(Mesh& mesh, const std::vector<int>& faceIndices);

// Topology (video: Create face / Connect / Target merge)
// create_face: adds a triangle from 3 vertex indices (object-local).
bool createFace(Mesh& mesh, int v0, int v1, int v2);
// connect: adds an edge by splitting faces that contain both endpoints, or
// inserts a diagonal when the two verts share a face; otherwise creates a thin bridge tri if possible.
bool connectVertices(Mesh& mesh, int v0, int v1);
// target_merge: move all verts in sources onto target position, then weld.
bool targetMerge(Mesh& mesh, int targetVertex, const std::vector<int>& sourceVertices);

struct MeshStats {
    int vertices = 0;
    int triangles = 0;
};
MeshStats meshStats(const Mesh& mesh);

}  // namespace core
