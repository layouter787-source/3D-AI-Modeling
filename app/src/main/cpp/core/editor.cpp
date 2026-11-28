#include "core/editor.h"
#include "core/polymesh.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <tuple>
#include <limits>
#include <unordered_set>
#include <unordered_map>
#include <utility>

namespace core {

namespace {

constexpr size_t kMaxHistory = 200;

PolyMesh extractFaceSet(const PolyMesh& source, const std::vector<int>& faceIds,
                        const std::vector<Vec3>* sourceColors = nullptr,
                        std::vector<Vec3>* extractedColors = nullptr) {
    PolyMesh result;
    const uint32_t missing = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> remap(source.verts.size(), missing);
    for (int faceId : faceIds) {
        if (faceId < 0 || static_cast<size_t>(faceId) >= source.faces.size()) continue;
        std::vector<uint32_t> face;
        for (uint32_t sourceVertex : source.faces[static_cast<size_t>(faceId)]) {
            if (sourceVertex >= source.verts.size()) continue;
            if (remap[sourceVertex] == missing) {
                remap[sourceVertex] = static_cast<uint32_t>(result.verts.size());
                result.verts.push_back(source.verts[sourceVertex]);
                if (extractedColors != nullptr) {
                    const Vec3 color = sourceColors != nullptr && sourceVertex < sourceColors->size()
                        ? (*sourceColors)[sourceVertex] : Vec3{1.0f, 1.0f, 1.0f};
                    extractedColors->push_back(color);
                }
            }
            face.push_back(remap[sourceVertex]);
        }
        if (face.size() >= 3) result.faces.push_back(std::move(face));
    }
    for (const auto& crease : source.creases) {
        if (crease.first < remap.size() && crease.second < remap.size() &&
            remap[crease.first] != missing && remap[crease.second] != missing &&
            remap[crease.first] != remap[crease.second]) {
            result.creases.push_back({std::min(remap[crease.first], remap[crease.second]),
                                      std::max(remap[crease.first], remap[crease.second])});
        }
    }
    return result;
}

bool finite3(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

bool rayTriangleHit(const Ray& ray, Vec3 a, Vec3 b, Vec3 c, float& distance) {
    const Vec3 edge1 = b - a;
    const Vec3 edge2 = c - a;
    const Vec3 p = cross(ray.dir, edge2);
    const float determinant = dot(edge1, p);
    if (std::fabs(determinant) < 1e-8f) return false;
    const float inverse = 1.0f / determinant;
    const Vec3 offset = ray.origin - a;
    const float u = dot(offset, p) * inverse;
    if (u < 0.0f || u > 1.0f) return false;
    const Vec3 q = cross(offset, edge1);
    const float v = dot(ray.dir, q) * inverse;
    if (v < 0.0f || u + v > 1.0f) return false;
    distance = dot(edge2, q) * inverse;
    return distance > 1e-4f;
}

class AddCommand : public Command {
public:
    explicit AddCommand(Object object) : object_(std::move(object)) {}

    void apply(Scene& scene) override {
        scene.objects.push_back(object_);
        scene.selectedId = object_.id;
    }

    void revert(Scene& scene) override {
        const int i = scene.indexOf(object_.id);
        if (i >= 0) scene.objects.erase(scene.objects.begin() + i);
        if (scene.selectedId == object_.id) scene.selectedId = -1;
    }

private:
    Object object_;
};

class DeleteCommand : public Command {
public:
    explicit DeleteCommand(int id) : id_(id) {}

    void apply(Scene& scene) override {
        const int i = scene.indexOf(id_);
        if (i < 0) return;
        index_ = static_cast<size_t>(i);
        saved_ = scene.objects[index_];
        hasSaved_ = true;
        scene.objects.erase(scene.objects.begin() + i);
        if (scene.selectedId == id_) scene.selectedId = -1;
    }

    void revert(Scene& scene) override {
        if (!hasSaved_) return;
        const size_t pos = std::min(index_, scene.objects.size());
        scene.objects.insert(scene.objects.begin() + static_cast<std::ptrdiff_t>(pos), saved_);
        scene.selectedId = id_;
    }

private:
    int id_;
    size_t index_ = 0;
    Object saved_;
    bool hasSaved_ = false;
};

class TransformCommand : public Command {
public:
    TransformCommand(int id, Transform before, Transform after)
        : id_(id), before_(before), after_(after) {}

    void apply(Scene& scene) override {
        if (Object* o = scene.find(id_)) o->transform = after_;
    }
    void revert(Scene& scene) override {
        if (Object* o = scene.find(id_)) o->transform = before_;
    }

private:
    int id_;
    Transform before_;
    Transform after_;
};

class MeshCommand : public Command {
public:
    MeshCommand(int id, Mesh before, Mesh after, PolyMesh beforeTopology, PolyMesh afterTopology,
                float beforeSmoothAngle, float afterSmoothAngle,
                std::vector<Vec3> beforeVertexColors, std::vector<Vec3> afterVertexColors)
        : id_(id), before_(std::move(before)), after_(std::move(after)),
          beforeTopology_(std::move(beforeTopology)), afterTopology_(std::move(afterTopology)),
          beforeSmoothAngle_(beforeSmoothAngle), afterSmoothAngle_(afterSmoothAngle),
          beforeVertexColors_(std::move(beforeVertexColors)), afterVertexColors_(std::move(afterVertexColors)) {}
    void apply(Scene& s) override {
        if (Object* o = s.find(id_)) {
            o->mesh = after_; o->topology = afterTopology_;
            o->smoothAngle = afterSmoothAngle_; o->vertexColors = afterVertexColors_; ++o->meshRevision;
        }
    }
    void revert(Scene& s) override {
        if (Object* o = s.find(id_)) {
            o->mesh = before_; o->topology = beforeTopology_;
            o->smoothAngle = beforeSmoothAngle_; o->vertexColors = beforeVertexColors_; ++o->meshRevision;
        }
    }
private:
    int id_;
    Mesh before_, after_;
    PolyMesh beforeTopology_, afterTopology_;
    float beforeSmoothAngle_;
    float afterSmoothAngle_;
    std::vector<Vec3> beforeVertexColors_;
    std::vector<Vec3> afterVertexColors_;
};

class ColorCommand : public Command {
public:
    ColorCommand(int id, Color before, Color after) : id_(id), before_(before), after_(after) {}

    void apply(Scene& scene) override {
        if (Object* o = scene.find(id_)) o->color = after_;
    }
    void revert(Scene& scene) override {
        if (Object* o = scene.find(id_)) o->color = before_;
    }

private:
    int id_;
    Color before_;
    Color after_;
};

class MaterialCommand : public Command {
public:
    MaterialCommand(int id, Material before, Material after)
        : id_(id), before_(before), after_(after) {}

    void apply(Scene& scene) override {
        if (Object* o = scene.find(id_)) o->material = after_;
    }
    void revert(Scene& scene) override {
        if (Object* o = scene.find(id_)) o->material = before_;
    }

private:
    int id_;
    Material before_;
    Material after_;
};

}  // namespace

int Editor::addPrimitive(Primitive type, float size, Vec3 position, int detail) {
    if (!std::isfinite(size) || !finite3(position)) return -1;
    size = std::clamp(size, 0.01f, 100.0f);

    Object object;
    object.id = scene_.nextId++;
    object.type = type;
    object.size = size;
    object.detail = std::clamp(detail, 1, 64);
    object.smoothAngle = primitiveSmoothAngle(type);
    object.topology = makePrimitivePolyMesh(type, size, object.detail);
    object.vertexColors.assign(object.topology.verts.size(), Vec3{1.0f, 1.0f, 1.0f});
    object.mesh = buildRenderMesh(object.topology, object.smoothAngle, object.vertexColors);
    object.transform.position = position;
    object.name = std::string(primitiveName(type)) + "_" + std::to_string(object.id);

    const int id = object.id;
    execute(std::make_unique<AddCommand>(std::move(object)));
    return id;
}

int Editor::addLoftMesh(const std::vector<float>& profiles, int segments, Vec3 position) {
    if (profiles.size() < 12 ||
        profiles.size() % 4 != 0 || segments < 8 || !finite3(position)) return -1;
    const size_t rings = profiles.size() / 4;
    for (size_t i = 0; i < rings; ++i) {
        const float x = profiles[i*4], width = profiles[i*4+1];
        const float cy = profiles[i*4+2], height = profiles[i*4+3];
        if (!std::isfinite(x) || !std::isfinite(width) || !std::isfinite(cy) ||
            !std::isfinite(height) || width <= 0.01f || height <= 0.01f) return -1;
        if (i && x <= profiles[(i-1)*4]) return -1;
    }
    const size_t vertexCount = rings * static_cast<size_t>(segments) + 2;
    if (vertexCount > 16000 || vertexCount > 65535) return -1;
    Mesh mesh;
    mesh.vertices.reserve(vertexCount);
    mesh.indices.reserve((rings-1)*static_cast<size_t>(segments)*6 + static_cast<size_t>(segments)*6);
    for (size_t i=0;i<rings;++i) {
        const float x=profiles[i*4], w=profiles[i*4+1], cy=profiles[i*4+2], h=profiles[i*4+3];
        for(int j=0;j<segments;++j) {
            const float a=2.0f*kPi*static_cast<float>(j)/segments;
            const float c=std::cos(a), sn=std::sin(a);
            Vec3 n=normalize({0.0f,c/std::max(h,0.001f),sn/std::max(w,0.001f)});
            mesh.vertices.push_back({x,cy+h*c,w*sn,n.x,n.y,n.z});
        }
    }
    for(size_t i=0;i+1<rings;++i) for(int j=0;j<segments;++j) {
        const uint16_t a=static_cast<uint16_t>(i*segments+j);
        const uint16_t b=static_cast<uint16_t>(i*segments+(j+1)%segments);
        const uint16_t c=static_cast<uint16_t>((i+1)*segments+j);
        const uint16_t d=static_cast<uint16_t>((i+1)*segments+(j+1)%segments);
        mesh.indices.insert(mesh.indices.end(),{a,b,c,b,d,c});
    }
    // Flat end caps close the shell; normals point along +/-X.
    const uint16_t leftCenter=static_cast<uint16_t>(mesh.vertices.size());
    mesh.vertices.push_back({profiles[0],profiles[2],0,-1,0,0});
    const size_t last=(rings-1)*static_cast<size_t>(segments);
    const uint16_t rightCenter=static_cast<uint16_t>(mesh.vertices.size());
    mesh.vertices.push_back({profiles[(rings-1)*4],profiles[(rings-1)*4+2],0,1,0,0});
    for(int j=0;j<segments;++j) {
        const uint16_t l0=static_cast<uint16_t>(j), l1=static_cast<uint16_t>((j+1)%segments);
        const uint16_t r0=static_cast<uint16_t>(last+j), r1=static_cast<uint16_t>(last+(j+1)%segments);
        mesh.indices.insert(mesh.indices.end(),{leftCenter,l1,l0,rightCenter,r0,r1});
    }
    // Preserve the loft's quad rings as editable topology. The render mesh is a separate
    // triangulated copy generated only for OpenGL.
    PolyMesh topology;
    topology.verts.reserve(rings * static_cast<size_t>(segments) + 2);
    for (size_t i = 0; i < rings; ++i) {
        const float x=profiles[i*4], w=profiles[i*4+1], cy=profiles[i*4+2], h=profiles[i*4+3];
        for (int j=0;j<segments;++j) {
            const float a=2.0f*kPi*static_cast<float>(j)/segments;
            topology.verts.push_back({x,cy+h*std::cos(a),w*std::sin(a)});
        }
    }
    auto vid=[&](size_t ring,int j){return static_cast<uint32_t>(ring*static_cast<size_t>(segments)+static_cast<size_t>((j+segments)%segments));};
    for(size_t i=0;i+1<rings;++i) for(int j=0;j<segments;++j)
        topology.faces.push_back({vid(i,j),vid(i,j+1),vid(i+1,j+1),vid(i+1,j)});
    std::vector<uint32_t> left,right;
    for(int j=segments;j-- >0;) left.push_back(vid(0,j));
    for(int j=0;j<segments;++j) right.push_back(vid(rings-1,j));
    topology.faces.push_back(std::move(left));
    topology.faces.push_back(std::move(right));
    mesh=buildRenderMesh(topology,35.0f);

    Object object;
    object.id=scene_.nextId++; object.type=Primitive::CustomMesh; object.size=1.0f;
    object.name="loft_mesh_"+std::to_string(object.id); object.mesh=std::move(mesh); object.smoothAngle=35.0f;
    object.topology=std::move(topology);
    object.transform.position=position;
    const int id=object.id;
    execute(std::make_unique<AddCommand>(std::move(object)));
    return id;
}

int Editor::duplicateObject(int id, Vec3 offset) {
    if (!finite3(offset)) return -1;
    const Object* source = scene_.find(id);
    if (source == nullptr) return -1;

    Object duplicate = *source;
    duplicate.id = scene_.nextId++;
    duplicate.name = std::string(primitiveName(duplicate.type)) + "_" + std::to_string(duplicate.id);
    duplicate.transform.position = duplicate.transform.position + offset;
    if (!finite3(duplicate.transform.position)) return -1;

    const int newId = duplicate.id;
    execute(std::make_unique<AddCommand>(std::move(duplicate)));
    return newId;
}

bool Editor::deleteObject(int id) {
    if (scene_.find(id) == nullptr) return false;
    execute(std::make_unique<DeleteCommand>(id));
    return true;
}

bool Editor::setTransform(int id, const Transform& transform) {
    Object* o = scene_.find(id);
    if (o == nullptr) return false;
    if (!finite3(transform.position) || !finite3(transform.rotationDeg) || !finite3(transform.scale)) {
        return false;
    }
    Transform after = transform;
    after.scale.x = std::clamp(after.scale.x, 0.001f, 1000.0f);
    after.scale.y = std::clamp(after.scale.y, 0.001f, 1000.0f);
    after.scale.z = std::clamp(after.scale.z, 0.001f, 1000.0f);
    execute(std::make_unique<TransformCommand>(id, o->transform, after));
    return true;
}

bool Editor::moveBy(int id, Vec3 offset) {
    const Object* o = scene_.find(id);
    if (o == nullptr) return false;
    Transform t = o->transform;
    t.position = t.position + offset;
    return setTransform(id, t);
}

bool Editor::rotateBy(int id, Vec3 degrees) {
    const Object* o = scene_.find(id);
    if (o == nullptr) return false;
    Transform t = o->transform;
    t.rotationDeg = t.rotationDeg + degrees;
    return setTransform(id, t);
}

bool Editor::scaleBy(int id, Vec3 factor) {
    const Object* o = scene_.find(id);
    if (o == nullptr) return false;
    Transform t = o->transform;
    t.scale = {t.scale.x * factor.x, t.scale.y * factor.y, t.scale.z * factor.z};
    return setTransform(id, t);
}

bool Editor::sculptMesh(int id, Vec3 center, float radius, Vec3 offset) {
    Object* object = scene_.find(id);
    if (!object || !finite3(center) || !finite3(offset) || !std::isfinite(radius) ||
        radius < 0.001f || radius > 1000.0f) return false;

    PolyMesh beforeTopology = object->topology;
    if (beforeTopology.empty()) {
        for (const Vertex& v : object->mesh.vertices) beforeTopology.verts.push_back({v.px,v.py,v.pz});
        for (size_t i=0;i+2<object->mesh.indices.size();i+=3)
            beforeTopology.faces.push_back({object->mesh.indices[i],object->mesh.indices[i+1],object->mesh.indices[i+2]});
        beforeTopology = weldVertices(beforeTopology, 1e-5f);
    }
    PolyMesh afterTopology = beforeTopology;
    bool changed = false;
    for (Vec3& vertex : afterTopology.verts) {
        const float distance = length(vertex - center);
        if (distance >= radius) continue;
        const float t = std::clamp(1.0f - distance / radius, 0.0f, 1.0f);
        const float weight = t * t * (3.0f - 2.0f * t);
        vertex += offset * weight;
        changed = changed || weight > 0.0001f;
    }
    if (!changed) return false;
    Mesh afterMesh = buildRenderMesh(afterTopology, object->smoothAngle, object->vertexColors);
    Mesh beforeMesh = object->mesh;
    execute(std::make_unique<MeshCommand>(id, std::move(beforeMesh), std::move(afterMesh),
                                          std::move(beforeTopology), std::move(afterTopology),
                                          object->smoothAngle, object->smoothAngle, object->vertexColors, object->vertexColors));
    return true;
}

bool Editor::meshOperation(int id, const std::string& operation, const std::vector<int>& faces,
                            int parameter, float value, int otherObjectId) {
    Object* object = scene_.find(id);
    if (object == nullptr || object->mesh.vertices.empty() || object->mesh.indices.size() < 3 ||
        !std::isfinite(value)) return false;

    // O renderer guarda vértices duplicados por canto. Reconstruímos topologia triangular
    // e soldamos posições coincidentes antes de operar sobre faces/arestas.
    PolyMesh source = object->topology;
    if (source.empty()) {
        source.verts.reserve(object->mesh.vertices.size());
        for (const Vertex& v : object->mesh.vertices) source.verts.push_back({v.px, v.py, v.pz});
        for (size_t i = 0; i + 2 < object->mesh.indices.size(); i += 3) {
            const uint32_t a = object->mesh.indices[i], b = object->mesh.indices[i + 1],
                           c = object->mesh.indices[i + 2];
            if (a >= source.verts.size() || b >= source.verts.size() || c >= source.verts.size()) return false;
            source.faces.push_back({a, b, c});
        }
        source = weldVertices(source, 1e-5f);
    }
    if (source.empty() || source.faces.size() > 250000 || source.verts.size() > 250000) return false;

    PolyMesh result;
    std::vector<Vec3> afterColors;
    float afterSmoothAngle = object->smoothAngle;
    if (operation == "soft_normals") {
        result = source;
        afterSmoothAngle = 180.0f;
    } else if (operation == "hard_normals") {
        result = source;
        afterSmoothAngle = 35.0f;
    } else if (operation == "normal_angle") {
        result = source;
        afterSmoothAngle = std::clamp(value, 0.0f, 180.0f);
    } else if (operation == "boolean_union" || operation == "boolean_difference" || operation == "boolean_intersection") {
        const Object* operand = scene_.find(otherObjectId);
        if (!operand || operand->id == object->id || operand->topology.empty()) return false;
        const int op = operation == "boolean_union" ? 0 : (operation == "boolean_difference" ? 1 : 2);
        // Boolean operands are converted into the selected object's local space.
        const Mat4 inverseSelected = inverseAffine(object->transform.matrix());
        const Mat4 operandWorld = operand->transform.matrix();
        PolyMesh other = operand->topology;
        for (Vec3& vertex : other.verts) vertex = transformPoint(inverseSelected, transformPoint(operandWorld, vertex));
        result = booleanOp(source, other, op);
    } else if (operation == "delete_faces") {
        if (faces.empty() || faces.size() >= source.faces.size()) return false;
        std::unordered_set<int> removed;
        for (int face : faces) {
            if (face < 0 || static_cast<size_t>(face) >= source.faces.size()) return false;
            removed.insert(face);
        }
        std::vector<int> remaining;
        remaining.reserve(source.faces.size() - removed.size());
        for (size_t i = 0; i < source.faces.size(); ++i) {
            if (removed.count(static_cast<int>(i)) == 0) remaining.push_back(static_cast<int>(i));
        }
        result = extractFaceSet(source, remaining, &object->vertexColors, &afterColors);
    } else if (operation == "create_face" || operation == "draw_face") {
        if (faces.size() < 3 || faces.size() > 128) return false;
        std::unordered_set<int> unique;
        std::vector<uint32_t> face;
        face.reserve(faces.size());
        for (int vertex : faces) {
            if (vertex < 0 || static_cast<size_t>(vertex) >= source.verts.size() ||
                !unique.insert(vertex).second) return false;
            face.push_back(static_cast<uint32_t>(vertex));
        }
        if (length(newellNormal(source, face)) < 1e-7f) return false;
        result = source;
        result.faces.push_back(std::move(face));
    } else if (operation == "merge_vertices") {
        if (faces.size() < 2 || faces.size() > 256) return false;
        std::unordered_set<int> unique;
        Vec3 target;
        for (int vertex : faces) {
            if (vertex < 0 || static_cast<size_t>(vertex) >= source.verts.size() ||
                !unique.insert(vertex).second) return false;
            target += source.verts[static_cast<size_t>(vertex)];
        }
        target = target / static_cast<float>(unique.size());
        result = source;
        for (int vertex : unique) result.verts[static_cast<size_t>(vertex)] = target;
        result = weldVertices(result, 1e-5f);
    } else if (operation == "target_merge") {
        if (faces.size() != 2 || faces[0] < 0 || faces[1] < 0 ||
            static_cast<size_t>(faces[0]) >= source.verts.size() ||
            static_cast<size_t>(faces[1]) >= source.verts.size() || faces[0] == faces[1]) return false;
        result = source;
        result.verts[static_cast<size_t>(faces[0])] = result.verts[static_cast<size_t>(faces[1])];
        result = weldVertices(result, 1e-5f);
    } else if (operation == "clone_faces") {
        if (faces.empty() || faces.size() >= source.faces.size()) return false;
        std::unordered_map<uint32_t, uint32_t> remap;
        Vec3 normal;
        std::unordered_set<int> uniqueFaces;
        for (int faceId : faces) {
            if (faceId < 0 || static_cast<size_t>(faceId) >= source.faces.size() ||
                !uniqueFaces.insert(faceId).second) return false;
            normal += newellNormal(source, source.faces[static_cast<size_t>(faceId)]);
        }
        normal = normalize(normal);
        if (length(normal) < 0.5f) return false;
        result = source;
        const float distance = std::clamp(value == 0.0f ? 0.1f : value, -100.0f, 100.0f);
        for (int faceId : faces) {
            std::vector<uint32_t> clone;
            for (uint32_t sourceVertex : source.faces[static_cast<size_t>(faceId)]) {
                auto found = remap.find(sourceVertex);
                if (found == remap.end()) {
                    const uint32_t newId = static_cast<uint32_t>(result.verts.size());
                    result.verts.push_back(source.verts[sourceVertex] + normal * distance);
                    found = remap.emplace(sourceVertex, newId).first;
                }
                clone.push_back(found->second);
            }
            result.faces.push_back(std::move(clone));
        }
    } else if (operation == "subdivide") {
        const int levels = std::clamp(parameter, 1, 3);
        if (estimateSubdividedFaces(source, levels) > 180000) return false;
        result = catmullClark(source, levels);
    } else if (operation == "extrude") {
        if (faces.empty() || faces.size() > source.faces.size()) return false;
        std::vector<int> valid;
        valid.reserve(faces.size());
        for (int face : faces) {
            if (face < 0 || static_cast<size_t>(face) >= source.faces.size()) return false;
            valid.push_back(face);
        }
        result = extrudeFaces(source, valid, std::clamp(value, -100.0f, 100.0f), true, Vec3{}, 1.0f);
    } else if (operation == "inset") {
        if (faces.empty() || faces.size() > source.faces.size()) return false;
        for (int face : faces) if (face < 0 || static_cast<size_t>(face) >= source.faces.size()) return false;
        result = insetFaces(source, faces, std::clamp(value, 0.0001f, 100.0f));
    } else if (operation == "bevel") {
        if (faces.empty() || faces.size() > source.faces.size()) return false;
        for (int face : faces) if (face < 0 || static_cast<size_t>(face) >= source.faces.size()) return false;
        const float amount = std::clamp(std::abs(value), 0.0001f, 0.95f);
        // Face bevel: cria o anel de transição com inset e levanta/baixa a face interna.
        // A operação é uma só para o histórico, portanto Undo restaura a malha original.
        PolyMesh beveled = insetFaces(source, faces, amount);
        result = extrudeFaces(beveled, faces, value, false, Vec3{}, 1.0f);
    } else if (operation == "smooth") {
        result = smoothMesh(source, std::clamp(parameter, 1, 100), std::clamp(value, 0.0f, 1.0f));
    } else if (operation == "solidify") {
        result = solidify(source, std::clamp(value, -20.0f, 20.0f));
    } else if (operation == "mirror") {
        result = mirrorMesh(source, std::clamp(parameter, 0, 2), 1e-4f);
    } else if (operation == "loop_cut" || operation == "connect") {
        if (faces.size() != 2 || faces[0] < 0 || faces[1] < 0 ||
            static_cast<size_t>(faces[0]) >= source.verts.size() ||
            static_cast<size_t>(faces[1]) >= source.verts.size()) return false;
        result = loopCut(source, static_cast<uint32_t>(faces[0]), static_cast<uint32_t>(faces[1]),
                         std::clamp(parameter, 1, 32));

    } else if (operation == "triangulate" || operation == "divide") {
        result = triangulate(source);
    } else if (operation == "flip_normals") {
        result = flipFaces(source);
    } else if (operation == "weld") {
        result = weldVertices(source, std::clamp(value, 1e-6f, 1.0f));
    } else if (operation == "crease_by_angle") {
        result = creaseByAngle(source, std::clamp(value, 0.0f, 180.0f));
    } else {
        return false;
    }

    if (result.empty() || result.verts.size() > 250000 || result.faces.size() > 250000) return false;
    if (afterColors.size() != result.verts.size()) {
        afterColors.assign(result.verts.size(), Vec3{1.0f, 1.0f, 1.0f});
        const size_t retained = std::min(object->vertexColors.size(), afterColors.size());
        for (size_t i = 0; i < retained; ++i) afterColors[i] = object->vertexColors[i];
    }
    Mesh after = buildRenderMesh(result, afterSmoothAngle, afterColors);
    if (after.vertices.empty() || after.indices.empty() || after.vertices.size() > 500000 ||
        after.indices.size() > 1500000) return false;
    Mesh beforeMesh = object->mesh;
    PolyMesh beforeTopology = object->topology;
    execute(std::make_unique<MeshCommand>(id, std::move(beforeMesh), std::move(after),
                                          std::move(beforeTopology), std::move(result),
                                          object->smoothAngle, afterSmoothAngle, object->vertexColors, std::move(afterColors)));
    return true;
}

int Editor::detachFaces(int id, const std::vector<int>& faces) {
    Object* object = scene_.find(id);
    if (object == nullptr || faces.empty() || object->topology.empty() ||
        faces.size() >= object->topology.faces.size()) return -1;

    std::unordered_set<int> selected;
    for (int face : faces) {
        if (face < 0 || static_cast<size_t>(face) >= object->topology.faces.size() ||
            !selected.insert(face).second) return -1;
    }

    std::vector<int> detachIds(selected.begin(), selected.end());
    std::vector<int> remainIds;
    for (size_t i = 0; i < object->topology.faces.size(); ++i) {
        if (selected.count(static_cast<int>(i)) == 0) remainIds.push_back(static_cast<int>(i));
    }
    std::vector<Vec3> detachedColors;
    std::vector<Vec3> remainingColors;
    PolyMesh detachedTopology = extractFaceSet(object->topology, detachIds, &object->vertexColors, &detachedColors);
    PolyMesh remainingTopology = extractFaceSet(object->topology, remainIds, &object->vertexColors, &remainingColors);
    if (detachedTopology.empty() || remainingTopology.empty()) return -1;

    const Mesh beforeMesh = object->mesh;
    const PolyMesh beforeTopology = object->topology;
    Mesh remainingMesh = buildRenderMesh(remainingTopology, object->smoothAngle, remainingColors);
    Mesh detachedMesh = buildRenderMesh(detachedTopology, object->smoothAngle, detachedColors);
    if (remainingMesh.indices.empty() || detachedMesh.indices.empty()) return -1;

    const float smoothAngle = object->smoothAngle;
    Object detached = *object;
    detached.id = scene_.nextId++;
    detached.name = object->name + "_detached_" + std::to_string(detached.id);
    detached.mesh = std::move(detachedMesh);
    detached.topology = std::move(detachedTopology);
    detached.vertexColors = detachedColors;
    detached.meshRevision = 1;

    execute(std::make_unique<MeshCommand>(id, beforeMesh, std::move(remainingMesh),
                                          beforeTopology, std::move(remainingTopology),
                                          smoothAngle, smoothAngle, object->vertexColors, remainingColors));
    const int newId = detached.id;
    execute(std::make_unique<AddCommand>(std::move(detached)));
    return newId;
}

bool Editor::combineObjects(int id, int otherObjectId) {
    Object* target = scene_.find(id);
    const Object* operand = scene_.find(otherObjectId);
    if (target == nullptr || operand == nullptr || id == otherObjectId ||
        target->topology.empty() || operand->topology.empty()) return false;

    PolyMesh other = operand->topology;
    const Mat4 inverseTarget = inverseAffine(target->transform.matrix());
    const Mat4 operandWorld = operand->transform.matrix();
    for (Vec3& vertex : other.verts) {
        vertex = transformPoint(inverseTarget, transformPoint(operandWorld, vertex));
    }
    PolyMesh combined = mergeMeshes(target->topology, other);
    std::vector<Vec3> combinedColors = target->vertexColors;
    combinedColors.insert(combinedColors.end(), operand->vertexColors.begin(), operand->vertexColors.end());
    if (combinedColors.size() != combined.verts.size()) combinedColors.resize(combined.verts.size(), Vec3{1.0f,1.0f,1.0f});
    if (combined.empty() || combined.verts.size() > 250000 || combined.faces.size() > 250000) return false;
    Mesh combinedMesh = buildRenderMesh(combined, target->smoothAngle, combinedColors);
    if (combinedMesh.indices.empty() || combinedMesh.vertices.size() > 500000 ||
        combinedMesh.indices.size() > 1500000) return false;

    const Mesh beforeMesh = target->mesh;
    const PolyMesh beforeTopology = target->topology;
    const float smoothAngle = target->smoothAngle;
    execute(std::make_unique<MeshCommand>(id, beforeMesh, std::move(combinedMesh),
                                          beforeTopology, std::move(combined),
                                          smoothAngle, smoothAngle, target->vertexColors, std::move(combinedColors)));
    execute(std::make_unique<DeleteCommand>(otherObjectId));
    return true;
}

std::vector<int> Editor::faceShell(int id, int startFace) const {
    std::vector<int> result;
    const Object* object = scene_.find(id);
    if (object == nullptr || object->topology.empty() || startFace < 0 ||
        static_cast<size_t>(startFace) >= object->topology.faces.size()) return result;

    using EdgeKey = std::pair<uint32_t, uint32_t>;
    std::map<EdgeKey, std::vector<int>> edgeFaces;
    const auto& faces = object->topology.faces;
    for (size_t faceId = 0; faceId < faces.size(); ++faceId) {
        const auto& face = faces[faceId];
        for (size_t corner = 0; corner < face.size(); ++corner) {
            uint32_t a = face[corner];
            uint32_t b = face[(corner + 1) % face.size()];
            if (a > b) std::swap(a, b);
            edgeFaces[{a, b}].push_back(static_cast<int>(faceId));
        }
    }

    std::vector<char> visited(faces.size(), 0);
    std::vector<int> stack{startFace};
    visited[static_cast<size_t>(startFace)] = 1;
    while (!stack.empty()) {
        const int current = stack.back();
        stack.pop_back();
        result.push_back(current);
        const auto& face = faces[static_cast<size_t>(current)];
        for (size_t corner = 0; corner < face.size(); ++corner) {
            uint32_t a = face[corner];
            uint32_t b = face[(corner + 1) % face.size()];
            if (a > b) std::swap(a, b);
            const auto found = edgeFaces.find({a, b});
            if (found == edgeFaces.end()) continue;
            for (int adjacent : found->second) {
                if (!visited[static_cast<size_t>(adjacent)]) {
                    visited[static_cast<size_t>(adjacent)] = 1;
                    stack.push_back(adjacent);
                }
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

bool Editor::beginVertexPaintStroke(int id) {
    if (paintStrokeObjectId_ >= 0) endVertexPaintStroke();
    Object* object = scene_.find(id);
    if (object == nullptr || object->topology.empty()) return false;
    if (object->vertexColors.size() != object->topology.verts.size()) {
        object->vertexColors.assign(object->topology.verts.size(), Vec3{1.0f, 1.0f, 1.0f});
        object->mesh = buildRenderMesh(object->topology, object->smoothAngle, object->vertexColors);
        ++object->meshRevision;
    }
    paintStrokeObjectId_ = id;
    paintStrokeBeforeMesh_ = object->mesh;
    paintStrokeBeforeTopology_ = object->topology;
    paintStrokeBeforeColors_ = object->vertexColors;
    paintStrokeBeforeSmoothAngle_ = object->smoothAngle;
    paintStrokeChanged_ = false;
    return true;
}

bool Editor::paintVertexAt(int id, const Ray& ray, float radius, Vec3 color, float opacity) {
    Object* object = scene_.find(id);
    if (object == nullptr || paintStrokeObjectId_ != id || object->topology.empty() ||
        !finite3(ray.origin) || !finite3(ray.dir) || !finite3(color) ||
        !std::isfinite(radius) || !std::isfinite(opacity) || radius < 0.001f) return false;

    radius = std::clamp(radius, 0.001f, 100.0f);
    opacity = std::clamp(opacity, 0.0f, 1.0f);
    color.x = std::clamp(color.x, 0.0f, 1.0f);
    color.y = std::clamp(color.y, 0.0f, 1.0f);
    color.z = std::clamp(color.z, 0.0f, 1.0f);

    const Mat4 model = object->transform.matrix();
    float nearest = std::numeric_limits<float>::max();
    for (size_t i = 0; i + 2 < object->mesh.indices.size(); i += 3) {
        const uint32_t ia = object->mesh.indices[i];
        const uint32_t ib = object->mesh.indices[i + 1];
        const uint32_t ic = object->mesh.indices[i + 2];
        if (ia >= object->mesh.vertices.size() || ib >= object->mesh.vertices.size() ||
            ic >= object->mesh.vertices.size()) continue;
        const Vertex& va = object->mesh.vertices[ia];
        const Vertex& vb = object->mesh.vertices[ib];
        const Vertex& vc = object->mesh.vertices[ic];
        const Vec3 a = transformPoint(model, {va.px, va.py, va.pz});
        const Vec3 b = transformPoint(model, {vb.px, vb.py, vb.pz});
        const Vec3 d = transformPoint(model, {vc.px, vc.py, vc.pz});
        float hitDistance = 0.0f;
        if (rayTriangleHit(ray, a, b, d, hitDistance) && hitDistance < nearest) nearest = hitDistance;
    }
    if (!std::isfinite(nearest) || nearest == std::numeric_limits<float>::max()) return false;

    const Vec3 worldHit = ray.origin + ray.dir * nearest;
    const Vec3 localHit = transformPoint(inverseAffine(model), worldHit);
    bool changed = false;
    for (size_t i = 0; i < object->topology.verts.size(); ++i) {
        const float distance = length(object->topology.verts[i] - localHit);
        if (distance >= radius) continue;
        const float t = std::clamp(1.0f - distance / radius, 0.0f, 1.0f);
        const float falloff = t * t * (3.0f - 2.0f * t);
        const float alpha = opacity * falloff;
        if (alpha <= 0.001f) continue;
        const Vec3 before = object->vertexColors[i];
        const Vec3 after = before * (1.0f - alpha) + color * alpha;
        if (length(after - before) > 0.0001f) {
            object->vertexColors[i] = after;
            changed = true;
        }
    }
    if (!changed) return false;
    object->mesh = buildRenderMesh(object->topology, object->smoothAngle, object->vertexColors);
    ++object->meshRevision;
    paintStrokeChanged_ = true;
    return true;
}

void Editor::endVertexPaintStroke() {
    if (paintStrokeObjectId_ < 0) return;
    const int id = paintStrokeObjectId_;
    Object* object = scene_.find(id);
    if (object != nullptr && paintStrokeChanged_) {
        Mesh afterMesh = object->mesh;
        PolyMesh afterTopology = object->topology;
        std::vector<Vec3> afterColors = object->vertexColors;
        const float afterSmoothAngle = object->smoothAngle;
        execute(std::make_unique<MeshCommand>(
            id, std::move(paintStrokeBeforeMesh_), std::move(afterMesh),
            std::move(paintStrokeBeforeTopology_), std::move(afterTopology),
            paintStrokeBeforeSmoothAngle_, afterSmoothAngle,
            std::move(paintStrokeBeforeColors_), std::move(afterColors)
        ));
    }
    paintStrokeObjectId_ = -1;
    paintStrokeChanged_ = false;
    paintStrokeBeforeMesh_ = Mesh{};
    paintStrokeBeforeTopology_ = PolyMesh{};
    paintStrokeBeforeColors_.clear();
}

std::string Editor::describeMesh(int id, int offset, int limit) const {
    const Object* object = scene_.find(id);
    if (object == nullptr) return "{\"error\":\"Object not found\"}";
    offset = std::max(0, offset);
    limit = std::max(limit, 1);
    const PolyMesh& topology = object->topology;
    const bool hasTopology = !topology.empty();
    const Mesh& render = object->mesh;
    const size_t vertexCount = hasTopology ? topology.verts.size() : render.vertices.size();
    const size_t faceCount = hasTopology ? topology.faces.size() : render.indices.size()/3;
    const size_t vStart = std::min(static_cast<size_t>(offset), vertexCount);
    const size_t fStart = std::min(static_cast<size_t>(offset), faceCount);
    const size_t vEnd = std::min(vertexCount, vStart + static_cast<size_t>(limit));
    const size_t fEnd = std::min(faceCount, fStart + static_cast<size_t>(limit));
    std::string out = "{\"object_id\":" + std::to_string(id) +
        ",\"topology\":" + (hasTopology ? "true" : "false") +
        ",\"vertex_count\":" + std::to_string(vertexCount) +
        ",\"face_count\":" + std::to_string(faceCount) +
        ",\"offset\":" + std::to_string(offset) + ",\"vertices\":[";
    char buf[192];
    for (size_t i = vStart; i < vEnd; ++i) {
        if (i > vStart) out += ",";
        if (hasTopology) {
            const Vec3& v = topology.verts[i];
            std::snprintf(buf, sizeof(buf), "[%.6f,%.6f,%.6f]", v.x, v.y, v.z);
        } else {
            const Vertex& v = render.vertices[i];
            std::snprintf(buf, sizeof(buf), "[%.6f,%.6f,%.6f]", v.px, v.py, v.pz);
        }
        out += buf;
    }
    out += "],\"faces\":[";
    for (size_t i = fStart; i < fEnd; ++i) {
        if (i > fStart) out += ",";
        if (hasTopology) {
            out += "[";
            const auto& face = topology.faces[i];
            for (size_t k = 0; k < face.size(); ++k) {
                if (k) out += ",";
                out += std::to_string(face[k]);
            }
            out += "]";
        } else {
            const size_t k = i * 3;
            std::snprintf(buf, sizeof(buf), "[%u,%u,%u]", render.indices[k], render.indices[k+1], render.indices[k+2]);
            out += buf;
        }
    }
    out += "]}";
    return out;
}

bool Editor::snapToGround(int id) {
    const Object* object = scene_.find(id);
    if (object == nullptr || object->mesh.vertices.empty()) return false;
    const Mat4 model = object->transform.matrix();
    float minimumY = 1.0e30f;
    for (const Vertex& vertex : object->mesh.vertices) {
        const Vec3 world = transformPoint(model, Vec3{vertex.px, vertex.py, vertex.pz});
        if (!finite3(world)) return false;
        minimumY = std::min(minimumY, world.y);
    }
    Transform transform = object->transform;
    transform.position.y -= minimumY;
    return setTransform(id, transform);
}

bool Editor::snapToGrid(int id, float step) {
    const Object* object = scene_.find(id);
    if (object == nullptr || !std::isfinite(step) || step <= 0.0f) return false;
    step = std::clamp(step, 0.001f, 100.0f);
    Transform transform = object->transform;
    transform.position.x = std::round(transform.position.x / step) * step;
    transform.position.y = std::round(transform.position.y / step) * step;
    transform.position.z = std::round(transform.position.z / step) * step;
    return setTransform(id, transform);
}

bool Editor::setColor(int id, Color color) {
    Object* o = scene_.find(id);
    if (o == nullptr) return false;
    if (!std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b)) return false;
    color.r = std::clamp(color.r, 0.0f, 1.0f);
    color.g = std::clamp(color.g, 0.0f, 1.0f);
    color.b = std::clamp(color.b, 0.0f, 1.0f);
    execute(std::make_unique<ColorCommand>(id, o->color, color));
    return true;
}

bool Editor::setMaterial(int id, Material material) {
    Object* o = scene_.find(id);
    if (o == nullptr) return false;
    if (!std::isfinite(material.metallic) || !std::isfinite(material.roughness) ||
        !std::isfinite(material.clearcoat) || !std::isfinite(material.emission)) return false;
    material.metallic = std::clamp(material.metallic, 0.0f, 1.0f);
    material.roughness = std::clamp(material.roughness, 0.04f, 1.0f);
    material.clearcoat = std::clamp(material.clearcoat, 0.0f, 1.0f);
    material.emission = std::clamp(material.emission, 0.0f, 8.0f);
    execute(std::make_unique<MaterialCommand>(id, o->material, material));
    return true;
}

std::vector<float> Editor::getVertexColors(int id) const {
    const Object* object = scene_.find(id);
    if (object == nullptr || object->topology.empty()) return {};
    std::vector<float> result;
    result.reserve(object->vertexColors.size() * 3);
    for (const Vec3& color : object->vertexColors) {
        result.push_back(color.x);
        result.push_back(color.y);
        result.push_back(color.z);
    }
    return result;
}

bool Editor::setVertexColors(int id, const std::vector<float>& rgb) {
    Object* object = scene_.find(id);
    if (object == nullptr || object->topology.empty() ||
        rgb.size() != object->topology.verts.size() * 3) return false;
    std::vector<Vec3> colors(object->topology.verts.size());
    for (size_t i = 0; i < colors.size(); ++i) {
        const float r = rgb[i * 3];
        const float g = rgb[i * 3 + 1];
        const float b = rgb[i * 3 + 2];
        if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b)) return false;
        colors[i] = {std::clamp(r, 0.0f, 1.0f),
                     std::clamp(g, 0.0f, 1.0f),
                     std::clamp(b, 0.0f, 1.0f)};
    }
    Mesh beforeMesh = object->mesh;
    Mesh afterMesh = buildRenderMesh(object->topology, object->smoothAngle, colors);
    if (afterMesh.indices.empty()) return false;
    execute(std::make_unique<MeshCommand>(
        id, std::move(beforeMesh), std::move(afterMesh),
        object->topology, object->topology,
        object->smoothAngle, object->smoothAngle,
        object->vertexColors, colors
    ));
    return true;
}

void Editor::clearScene() {
    paintStrokeObjectId_ = -1;
    paintStrokeChanged_ = false;
    paintStrokeBeforeMesh_ = Mesh{};
    paintStrokeBeforeTopology_ = PolyMesh{};
    paintStrokeBeforeColors_.clear();
    scene_ = Scene{};
    undo_.clear();
    redo_.clear();
}

void Editor::clearHistory() {
    undo_.clear();
    redo_.clear();
}

bool Editor::undo() {
    if (paintStrokeObjectId_ >= 0) endVertexPaintStroke();
    if (undo_.empty()) return false;
    std::unique_ptr<Command> command = std::move(undo_.back());
    undo_.pop_back();
    command->revert(scene_);
    redo_.push_back(std::move(command));
    fixSelection();
    return true;
}

bool Editor::redo() {
    if (redo_.empty()) return false;
    std::unique_ptr<Command> command = std::move(redo_.back());
    redo_.pop_back();
    command->apply(scene_);
    undo_.push_back(std::move(command));
    fixSelection();
    return true;
}

void Editor::select(int id) {
    if (id < 0 || scene_.find(id) != nullptr) scene_.selectedId = id < 0 ? -1 : id;
}

int Editor::pick(const Ray& ray) {
    scene_.selectedId = pickObject(scene_, ray);
    return scene_.selectedId;
}

std::string Editor::describeScene() const {
    std::string out = "{\"objects\":[";
    char buf[512];
    bool first = true;
    for (const Object& o : scene_.objects) {
        if (!first) out += ",";
        first = false;
        std::snprintf(buf, sizeof(buf),
                      R"JSON({"id":%d,"type":"%s","size":%.3f,"detail":%d,"smooth_angle":%.1f,"vertices":%zu,"triangles":%zu,"position":[%.3f,%.3f,%.3f],"rotation":[%.1f,%.1f,%.1f],"scale":[%.3f,%.3f,%.3f],"color":[%.2f,%.2f,%.2f],"material":{"metallic":%.2f,"roughness":%.2f,"clearcoat":%.2f,"emission":%.2f}})JSON",
                      o.id, primitiveName(o.type), o.size, o.detail, o.smoothAngle, o.mesh.vertices.size(),
                      o.mesh.indices.size() / 3, o.transform.position.x,
                      o.transform.position.y, o.transform.position.z, o.transform.rotationDeg.x,
                      o.transform.rotationDeg.y, o.transform.rotationDeg.z, o.transform.scale.x,
                      o.transform.scale.y, o.transform.scale.z, o.color.r, o.color.g, o.color.b,
                      o.material.metallic, o.material.roughness, o.material.clearcoat, o.material.emission);
        out += buf;
    }
    std::snprintf(buf, sizeof(buf), R"JSON(],"selected":%d})JSON", scene_.selectedId);
    out += buf;
    return out;
}

void Editor::execute(std::unique_ptr<Command> command) {
    command->apply(scene_);
    undo_.push_back(std::move(command));
    redo_.clear();
    if (undo_.size() > kMaxHistory) undo_.erase(undo_.begin());
}

void Editor::fixSelection() {
    if (scene_.selectedId >= 0 && scene_.find(scene_.selectedId) == nullptr) scene_.selectedId = -1;
}

}  // namespace core
