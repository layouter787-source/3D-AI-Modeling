#pragma once

#include <string>
#include <vector>

#include "core/mesh.h"
#include "core/scene.h"

namespace core {

// Minimal glTF 2.0 binary (GLB) export of one mesh + baseColor/metallic/roughness.
// Returns empty vector on failure.
std::vector<uint8_t> exportMeshToGlb(const Mesh& mesh, const Color& color, float metallic,
                                     float roughness, const char* name);

// Import first mesh from a GLB buffer. Returns false on failure.
bool importMeshFromGlb(const uint8_t* data, size_t size, Mesh& outMesh, Color* outColor,
                       float* outMetallic, float* outRoughness);

// File helpers (Android app uses internal storage paths passed from Kotlin).
bool writeGlbFile(const std::string& path, const std::vector<uint8_t>& bytes);
bool readFileBytes(const std::string& path, std::vector<uint8_t>& out);

}  // namespace core
