#include "core/scene.h"
#include "core/polymesh.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace core {

namespace {

void orientFacesOutward(PolyMesh& mesh) {
    Vec3 center;
    for (const Vec3& vertex : mesh.verts) center += vertex;
    if (!mesh.verts.empty()) center = center / static_cast<float>(mesh.verts.size());
    for (auto& face : mesh.faces) {
        Vec3 faceCenter;
        for (uint32_t index : face) faceCenter += mesh.verts[index];
        if (!face.empty()) faceCenter = faceCenter / static_cast<float>(face.size());
        if (dot(newellNormal(mesh, face), faceCenter - center) < 0.0f) {
            std::reverse(face.begin(), face.end());
        }
    }
}

PolyMesh makeIcosahedronPoly(float radius, int subdivisions) {
    const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
    PolyMesh mesh;
    const Vec3 base[] = {
        {-1,t,0},{1,t,0},{-1,-t,0},{1,-t,0},
        {0,-1,t},{0,1,t},{0,-1,-t},{0,1,-t},
        {t,0,-1},{t,0,1},{-t,0,-1},{-t,0,1}
    };
    for (const Vec3& vertex : base) mesh.verts.push_back(normalize(vertex) * radius);
    mesh.faces = {
        {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
        {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
        {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
        {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}
    };
    orientFacesOutward(mesh);

    for (int level = 0; level < std::clamp(subdivisions, 0, 4); ++level) {
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> midpointCache;
        auto midpoint = [&](uint32_t a, uint32_t b) {
            const auto key = std::minmax(a, b);
            const std::pair<uint32_t, uint32_t> pair{key.first, key.second};
            const auto found = midpointCache.find(pair);
            if (found != midpointCache.end()) return found->second;
            const uint32_t id = static_cast<uint32_t>(mesh.verts.size());
            mesh.verts.push_back(normalize(mesh.verts[a] + mesh.verts[b]) * radius);
            midpointCache.emplace(pair, id);
            return id;
        };
        std::vector<std::vector<uint32_t>> faces;
        faces.reserve(mesh.faces.size() * 4);
        for (const auto& face : mesh.faces) {
            if (face.size() != 3) continue;
            const uint32_t a = face[0], b = face[1], c = face[2];
            const uint32_t ab = midpoint(a,b), bc = midpoint(b,c), ca = midpoint(c,a);
            faces.push_back({a,ab,ca});
            faces.push_back({b,bc,ab});
            faces.push_back({c,ca,bc});
            faces.push_back({ab,bc,ca});
        }
        mesh.faces = std::move(faces);
    }
    return mesh;
}

}  // namespace

const char* primitiveName(Primitive p) {
    switch (p) {
        case Primitive::Cube: return "cube";
        case Primitive::Sphere: return "sphere";
        case Primitive::Cylinder: return "cylinder";
        case Primitive::Cone: return "cone";
        case Primitive::Torus: return "torus";
        case Primitive::Plane: return "plane";
        case Primitive::Pyramid: return "pyramid";
        case Primitive::Capsule: return "capsule";
        case Primitive::Octahedron: return "octahedron";
        case Primitive::HexPrism: return "hex_prism";
        case Primitive::CustomMesh: return "custom_mesh";
        case Primitive::TriPrism: return "tri_prism";
        case Primitive::PentPrism: return "pent_prism";
        case Primitive::OctPrism: return "oct_prism";
        case Primitive::DecPrism: return "dec_prism";
        case Primitive::DodecPrism: return "dodec_prism";
        case Primitive::Tetrahedron: return "tetrahedron";
        case Primitive::Icosahedron: return "icosahedron";
        case Primitive::IcoSphere: return "icosphere";
        case Primitive::Pipe: return "pipe";
    }
    return "cube";
}

PolyMesh makePrimitivePolyMesh(Primitive p, float size, int detail) {
    PolyMesh poly;
    detail = std::clamp(detail, 1, 64);
    const int radial = std::clamp(detail * 2, 8, 128);
    const int vertical = std::clamp(detail / 4, 1, 32);
    switch (p) {
        case Primitive::Cube:
            poly = makeBoxPoly({size, size, size}, detail, detail, detail); break;
        case Primitive::Sphere:
            poly = makeSpherePoly(size * 0.5f, radial); break;
        case Primitive::Cylinder:
            poly = makeCylinderPoly(size * 0.5f, size, radial, vertical, true); break;
        case Primitive::Cone:
            poly = makeConePoly(size * 0.5f, size, radial, true); break;
        case Primitive::Torus:
            poly = makeTorusPoly(size * 0.35f, size * 0.15f, radial, std::clamp(detail, 6, 64)); break;
        case Primitive::Plane:
            poly = makePlanePoly(size, size, detail, detail); break;
        case Primitive::Pyramid: {
            const float h=size*0.5f;
            poly.verts={{-h,-h,-h},{h,-h,-h},{h,-h,h},{-h,-h,h},{0,h,0}};
            poly.faces={{0,2,1},{0,3,2},{0,1,4},{1,2,4},{2,3,4},{3,0,4}};
            break;
        }
        case Primitive::Capsule: {
            const float r=size*0.25f, stem=std::max(0.01f,size-2.0f*r);
            PolyMesh body=makeCylinderPoly(r,stem,radial,vertical,true);
            PolyMesh lower=transformed(makeSpherePoly(r,std::max(8,radial/2)),translate(0,-stem*0.5f,0));
            PolyMesh upper=transformed(makeSpherePoly(r,std::max(8,radial/2)),translate(0,stem*0.5f,0));
            poly=mergeMeshes(mergeMeshes(body,lower),upper); break;
        }
        case Primitive::Octahedron:
            poly.verts={{0,size*0.5f,0},{size*0.5f,0,0},{0,0,size*0.5f},{-size*0.5f,0,0},{0,0,-size*0.5f},{0,-size*0.5f,0}};
            poly.faces={{0,1,2},{0,2,3},{0,3,4},{0,4,1},{5,2,1},{5,3,2},{5,4,3},{5,1,4}};
            break;
        case Primitive::HexPrism:
            poly=makeCylinderPoly(size*0.5f,size,6,vertical,true); break;
        case Primitive::CustomMesh:
            poly=makeBoxPoly({size,size,size},detail,detail,detail); break;
        case Primitive::TriPrism:
            poly = makeCylinderPoly(size * 0.5f, size, 3, vertical, true); break;
        case Primitive::PentPrism:
            poly = makeCylinderPoly(size * 0.5f, size, 5, vertical, true); break;
        case Primitive::OctPrism:
            poly = makeCylinderPoly(size * 0.5f, size, 8, vertical, true); break;
        case Primitive::DecPrism:
            poly = makeCylinderPoly(size * 0.5f, size, 10, vertical, true); break;
        case Primitive::DodecPrism:
            poly = makeCylinderPoly(size * 0.5f, size, 12, vertical, true); break;
        case Primitive::Tetrahedron: {
            const float h = size * 0.5f;
            poly.verts = {{h,h,h},{-h,-h,h},{-h,h,-h},{h,-h,-h}};
            poly.faces = {{0,2,1},{0,1,3},{0,3,2},{1,2,3}};
            break;
        }
        case Primitive::Icosahedron:
            poly = makeIcosahedronPoly(size * 0.5f, 0); break;
        case Primitive::IcoSphere: {
            const int levels = detail <= 4 ? 0 : detail <= 12 ? 1 : detail <= 24 ? 2 : detail <= 40 ? 3 : 4;
            poly = makeIcosahedronPoly(size * 0.5f, levels);
            break;
        }
        case Primitive::Pipe:
            poly = makePipePoly(size * 0.5f, size * 0.32f, size, radial, vertical); break;
    }
    return poly;
}


float primitiveSmoothAngle(Primitive p) {
    switch (p) {
        case Primitive::Sphere:
        case Primitive::Torus:
        case Primitive::Capsule:
        case Primitive::IcoSphere: return 180.0f;
        case Primitive::Plane: return 0.0f;
        default: return 35.0f;
    }
}

Mesh makePrimitiveMesh(Primitive p, float size) {
    return buildRenderMesh(makePrimitivePolyMesh(p, size), primitiveSmoothAngle(p));
}

namespace {

Mat4 rotationMatrix(const Transform& t) {
    return multiply(rotateZ(radians(t.rotationDeg.z)),
                    multiply(rotateY(radians(t.rotationDeg.y)), rotateX(radians(t.rotationDeg.x))));
}

Vec3 toVec(const Vertex& v) { return {v.px, v.py, v.pz}; }

// Möller-Trumbore, de dois lados.
bool rayTriangle(const Ray& ray, Vec3 a, Vec3 b, Vec3 c, float& t) {
    const Vec3 e1 = b - a;
    const Vec3 e2 = c - a;
    const Vec3 p = cross(ray.dir, e2);
    const float det = dot(e1, p);
    if (std::fabs(det) < 1e-9f) return false;
    const float inv = 1.0f / det;
    const Vec3 s = ray.origin - a;
    const float u = dot(s, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const Vec3 q = cross(s, e1);
    const float v = dot(ray.dir, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = dot(e2, q) * inv;
    return t > 1e-4f;
}

}  // namespace

Mat4 Transform::matrix() const {
    return multiply(translate(position.x, position.y, position.z),
                    multiply(rotationMatrix(*this), scaleMatrix(scale)));
}

void normalMatrix(const Transform& t, float out[9]) {
    const Mat4 r = rotationMatrix(t);
    const float s[3] = {std::max(std::fabs(t.scale.x), 1e-4f), std::max(std::fabs(t.scale.y), 1e-4f),
                        std::max(std::fabs(t.scale.z), 1e-4f)};
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            out[col * 3 + row] = r.m[col * 4 + row] / s[col];
        }
    }
}

Object* Scene::find(int id) {
    const int i = indexOf(id);
    return i >= 0 ? &objects[static_cast<size_t>(i)] : nullptr;
}

const Object* Scene::find(int id) const {
    const int i = indexOf(id);
    return i >= 0 ? &objects[static_cast<size_t>(i)] : nullptr;
}

int Scene::indexOf(int id) const {
    for (size_t i = 0; i < objects.size(); ++i) {
        if (objects[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

int pickObject(const Scene& scene, const Ray& ray) {
    int bestId = -1;
    float bestT = 1e30f;
    for (const Object& o : scene.objects) {
        const Mat4 model = o.transform.matrix();
        const Mesh& mesh = o.mesh;
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const Vec3 a = transformPoint(model, toVec(mesh.vertices[mesh.indices[i]]));
            const Vec3 b = transformPoint(model, toVec(mesh.vertices[mesh.indices[i + 1]]));
            const Vec3 c = transformPoint(model, toVec(mesh.vertices[mesh.indices[i + 2]]));
            float t = 0.0f;
            if (rayTriangle(ray, a, b, c, t) && t < bestT) {
                bestT = t;
                bestId = o.id;
            }
        }
    }
    return bestId;
}

}  // namespace core
