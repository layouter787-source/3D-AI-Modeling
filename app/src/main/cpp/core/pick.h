#pragma once

#include "core/math.h"
#include "core/scene.h"

namespace core {

enum class PickComponent { None, Vertex, Edge, Face };

struct PickResult {
    int objectId = -1;
    PickComponent component = PickComponent::None;
    int index = -1;
    int indexB = -1;
    float t = 0.0f;
    Vec3 worldPoint{};
};

// Fine pick on a specific object (or whole scene if objectId < 0).
PickResult pickComponent(const Scene& scene, const Ray& ray, int objectId, PickComponent mode);

}  // namespace core
