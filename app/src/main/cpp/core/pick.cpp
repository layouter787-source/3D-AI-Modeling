#include "core/pick.h"

#include <algorithm>
#include <cmath>

namespace core {

namespace {

Vec3 toVec(const Vertex& v) { return {v.px, v.py, v.pz}; }

bool rayTriangle(const Ray& ray, Vec3 a, Vec3 b, Vec3 c, float& t, float& u, float& v) {
    const Vec3 e1 = b - a;
    const Vec3 e2 = c - a;
    const Vec3 p = cross(ray.dir, e2);
    const float det = dot(e1, p);
    if (std::fabs(det) < 1e-9f) return false;
    const float inv = 1.0f / det;
    const Vec3 s = ray.origin - a;
    u = dot(s, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const Vec3 q = cross(s, e1);
    v = dot(ray.dir, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = dot(e2, q) * inv;
    return t > 1e-4f;
}

// Closest point on segment AB to ray; returns distance and ray t.
bool raySegmentDist(const Ray& ray, Vec3 a, Vec3 b, float& outDist, float& outT, Vec3& outPt) {
    // Approximate: closest points between ray and segment
    Vec3 u = ray.dir;
    Vec3 v = b - a;
    Vec3 w = ray.origin - a;
    float uu = dot(u, u);
    float uv = dot(u, v);
    float vv = dot(v, v);
    float uw = dot(u, w);
    float vw = dot(v, w);
    float det = uu * vv - uv * uv;
    float s, t;
    if (std::fabs(det) < 1e-12f) {
        s = 0.0f;
        t = (vv > 1e-12f) ? vw / vv : 0.0f;
    } else {
        s = (uv * vw - vv * uw) / det;
        t = (uu * vw - uv * uw) / det;
    }
    t = std::clamp(t, 0.0f, 1.0f);
    if (s < 1e-4f) s = 1e-4f;
    Vec3 ptRay = ray.origin + u * s;
    Vec3 ptSeg = a + v * t;
    outDist = std::sqrt(dot(ptRay - ptSeg, ptRay - ptSeg));
    outT = s;
    outPt = ptSeg;
    return true;
}

}  // namespace

int pickObject(const Scene& scene, const Ray& ray) {
    int bestId = -1;
    float bestT = 1e30f;
    for (const Object& o : scene.objects) {
        const Mat4 model = o.transform.matrix();
        const Mesh& mesh = o.mesh;
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            Vec3 a = transformPoint(model, toVec(mesh.vertices[mesh.indices[i]]));
            Vec3 b = transformPoint(model, toVec(mesh.vertices[mesh.indices[i + 1]]));
            Vec3 c = transformPoint(model, toVec(mesh.vertices[mesh.indices[i + 2]]));
            float t, u, v;
            if (rayTriangle(ray, a, b, c, t, u, v) && t < bestT) {
                bestT = t;
                bestId = o.id;
            }
        }
    }
    return bestId;
}

PickResult pickComponent(const Scene& scene, const Ray& ray, int objectId, PickComponent mode) {
    PickResult best;
    best.t = 1e30f;
    const float vertexThreshold = 0.08f;  // world units (scales with distance slightly)
    const float edgeThreshold = 0.06f;

    for (const Object& o : scene.objects) {
        if (objectId >= 0 && o.id != objectId) continue;
        const Mat4 model = o.transform.matrix();
        const Mesh& mesh = o.mesh;

        if (mode == PickComponent::Face) {
            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                Vec3 a = transformPoint(model, toVec(mesh.vertices[mesh.indices[i]]));
                Vec3 b = transformPoint(model, toVec(mesh.vertices[mesh.indices[i + 1]]));
                Vec3 c = transformPoint(model, toVec(mesh.vertices[mesh.indices[i + 2]]));
                float t, u, v;
                if (rayTriangle(ray, a, b, c, t, u, v) && t < best.t) {
                    best.t = t;
                    best.objectId = o.id;
                    best.component = PickComponent::Face;
                    best.index = static_cast<int>(i / 3);
                    best.worldPoint = ray.origin + ray.dir * t;
                }
            }
        } else if (mode == PickComponent::Vertex) {
            for (size_t vi = 0; vi < mesh.vertices.size(); ++vi) {
                Vec3 wp = transformPoint(model, toVec(mesh.vertices[vi]));
                // Project distance from ray
                Vec3 toV = wp - ray.origin;
                float t = dot(toV, ray.dir);
                if (t < 1e-4f) continue;
                Vec3 closest = ray.origin + ray.dir * t;
                float dist = std::sqrt(dot(wp - closest, wp - closest));
                float thresh = vertexThreshold * std::max(1.0f, t * 0.15f);
                if (dist < thresh && t < best.t) {
                    best.t = t;
                    best.objectId = o.id;
                    best.component = PickComponent::Vertex;
                    best.index = static_cast<int>(vi);
                    best.worldPoint = wp;
                }
            }
        } else if (mode == PickComponent::Edge) {
            // Unique edges from triangles
            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                uint16_t ids[3] = {mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2]};
                for (int e = 0; e < 3; ++e) {
                    int ia = ids[e];
                    int ib = ids[(e + 1) % 3];
                    if (ia > ib) std::swap(ia, ib);
                    Vec3 a = transformPoint(model, toVec(mesh.vertices[static_cast<size_t>(ia)]));
                    Vec3 b = transformPoint(model, toVec(mesh.vertices[static_cast<size_t>(ib)]));
                    float dist, t;
                    Vec3 pt;
                    raySegmentDist(ray, a, b, dist, t, pt);
                    float thresh = edgeThreshold * std::max(1.0f, t * 0.15f);
                    if (dist < thresh && t < best.t) {
                        best.t = t;
                        best.objectId = o.id;
                        best.component = PickComponent::Edge;
                        best.index = ia;
                        best.indexB = ib;
                        best.worldPoint = pt;
                    }
                }
            }
        }
    }
    return best;
}

}  // namespace core
