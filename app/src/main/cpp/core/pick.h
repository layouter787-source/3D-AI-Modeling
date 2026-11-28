#pragma once

#include "core/math.h"
#include "core/scene.h"

namespace core {

enum class PickComponent { None, Vertex, Edge, Face };

struct PickResult {
    int objectId = -1;
    PickComponent component = PickComponent::None;
    int index = -1;       // vertex index, edge key, or face (triangle) index
    int indexB = -1;      // second vertex for edge
    float t = 0.0f;       // ray parameter
    Vec3 worldPoint{};
};

// Object-level pick (existing behavior).
int pickObject(const Scene& scene, const Ray& ray);

// Fine pick on a specific object (or whole scene if objectId < 0).
// mode: Vertex / Edge / Face — only that component is tested.
PickResult pickComponent(const Scene& scene, const Ray& ray, int objectId, PickComponent mode);

}  // namespace core
