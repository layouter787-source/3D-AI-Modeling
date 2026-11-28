#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/mesh.h"

namespace core {

// --- Normals ---
void recomputeSmoothNormals(Mesh& mesh);
void recomputeHardNormals(Mesh& mesh);  // duplicates verts so each face is flat

// --- Topology / density ---
// Midpoint subdivision (1 level). Caps at ~60k verts (uint16 indices).
bool subdivideMesh(Mesh& mesh);
// Ensure all faces are triangles (already are) — no-op safety + weld optional.
bool triangulateMesh(Mesh& mesh);

// Laplacian smooth. iterations 1..20, factor 0..1.
void smoothMesh(Mesh& mesh, int iterations, float factor);

// Weld vertices closer than epsilon. Returns number of verts removed.
int weldVertices(Mesh& mesh, float epsilon);

// Mirror across axis: 0=X, 1=Y, 2=Z. Optionally weld the seam.
void mirrorMesh(Mesh& mesh, int axis, bool weldSeam);

// Extrude all triangles outward along averaged normal by distance.
// If faceIndices empty, extrudes every face. faceIndices are triangle starts (i/3).
bool extrudeFaces(Mesh& mesh, const std::vector<int>& faceIndices, float distance);

// Simple edge bevel approximation: push boundary / all verts along normal by amount,
// then optional subdivide. segments>=1. Not a full Catmull-Clark bevel but usable.
bool bevelMesh(Mesh& mesh, float amount, int segments);

// Merge other into dst (appends verts/indices with offset).
void appendMesh(Mesh& dst, const Mesh& other);

// Extract a subset of faces into a new mesh (for Separate).
// faceIndices are triangle indices (0..numTris-1).
Mesh extractFaces(const Mesh& mesh, const std::vector<int>& faceIndices);
// Remove those faces from mesh (complement of extract).
void removeFaces(Mesh& mesh, const std::vector<int>& faceIndices);

// Stats helper
struct MeshStats {
    int vertices = 0;
    int triangles = 0;
};
MeshStats meshStats(const Mesh& mesh);

}  // namespace core
