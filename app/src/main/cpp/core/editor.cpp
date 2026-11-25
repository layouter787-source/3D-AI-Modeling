#include "core/editor.h"
#include "core/polymesh.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <tuple>
#include <utility>

namespace core {

namespace {

constexpr size_t kMaxHistory = 200;
constexpr size_t kMaxObjects = 500;

bool finite3(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

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
    MeshCommand(int id, Mesh before, Mesh after, PolyMesh beforeTopology, PolyMesh afterTopology)
        : id_(id), before_(std::move(before)), after_(std::move(after)),
          beforeTopology_(std::move(beforeTopology)), afterTopology_(std::move(afterTopology)) {}
    void apply(Scene& s) override {
        if (Object* o = s.find(id_)) { o->mesh = after_; o->topology = afterTopology_; }
    }
    void revert(Scene& s) override {
        if (Object* o = s.find(id_)) { o->mesh = before_; o->topology = beforeTopology_; }
    }
private:
    int id_;
    Mesh before_, after_;
    PolyMesh beforeTopology_, afterTopology_;
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

}  // namespace

int Editor::addPrimitive(Primitive type, float size, Vec3 position) {
    if (scene_.objects.size() >= kMaxObjects) return -1;
    if (!std::isfinite(size) || !finite3(position)) return -1;
    size = std::clamp(size, 0.01f, 100.0f);

    Object object;
    object.id = scene_.nextId++;
    object.type = type;
    object.size = size;
    object.topology = makePrimitivePolyMesh(type, size);
    object.mesh = buildRenderMesh(object.topology, primitiveSmoothAngle(type));
    object.transform.position = position;
    object.name = std::string(primitiveName(type)) + "_" + std::to_string(object.id);

    const int id = object.id;
    execute(std::make_unique<AddCommand>(std::move(object)));
    return id;
}

int Editor::addLoftMesh(const std::vector<float>& profiles, int segments, Vec3 position) {
    if (scene_.objects.size() >= kMaxObjects || profiles.size() < 12 || profiles.size() > 96 ||
        profiles.size() % 4 != 0 || segments < 8 || segments > 32 || !finite3(position)) return -1;
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
    object.name="loft_mesh_"+std::to_string(object.id); object.mesh=std::move(mesh);
    object.topology=std::move(topology);
    object.transform.position=position;
    const int id=object.id;
    execute(std::make_unique<AddCommand>(std::move(object)));
    return id;
}

int Editor::duplicateObject(int id, Vec3 offset) {
    if (scene_.objects.size() >= kMaxObjects || !finite3(offset)) return -1;
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
    Mesh afterMesh = buildRenderMesh(afterTopology, primitiveSmoothAngle(object->type));
    Mesh beforeMesh = object->mesh;
    execute(std::make_unique<MeshCommand>(id, std::move(beforeMesh), std::move(afterMesh),
                                          std::move(beforeTopology), std::move(afterTopology)));
    return true;
}

bool Editor::meshOperation(int id, const std::string& operation, const std::vector<int>& faces,
                            int parameter, float value) {
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
    if (operation == "subdivide") {
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
    } else if (operation == "smooth") {
        result = smoothMesh(source, std::clamp(parameter, 1, 100), std::clamp(value, 0.0f, 1.0f));
    } else if (operation == "solidify") {
        result = solidify(source, std::clamp(value, -20.0f, 20.0f));
    } else if (operation == "mirror") {
        result = mirrorMesh(source, std::clamp(parameter, 0, 2), 1e-4f);
    } else if (operation == "loop_cut") {
        if (faces.size() != 2 || faces[0] < 0 || faces[1] < 0 ||
            static_cast<size_t>(faces[0]) >= source.verts.size() ||
            static_cast<size_t>(faces[1]) >= source.verts.size()) return false;
        result = loopCut(source, static_cast<uint32_t>(faces[0]), static_cast<uint32_t>(faces[1]),
                         std::clamp(parameter, 1, 32));

    } else if (operation == "triangulate") {
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
    Mesh after = buildRenderMesh(result, primitiveSmoothAngle(object->type));
    if (after.vertices.empty() || after.indices.empty() || after.vertices.size() > 500000 ||
        after.indices.size() > 1500000) return false;
    Mesh beforeMesh = object->mesh;
    PolyMesh beforeTopology = object->topology;
    execute(std::make_unique<MeshCommand>(id, std::move(beforeMesh), std::move(after),
                                          std::move(beforeTopology), std::move(result)));
    return true;
}

std::string Editor::describeMesh(int id, int offset, int limit) const {
    const Object* object = scene_.find(id);
    if (object == nullptr) return "{\\"error\\":\\"Object not found\\"}";
    offset = std::max(0, offset);
    limit = std::clamp(limit, 1, 1200);
    const PolyMesh& topology = object->topology;
    const bool hasTopology = !topology.empty();
    const Mesh& render = object->mesh;
    const size_t vertexCount = hasTopology ? topology.verts.size() : render.vertices.size();
    const size_t faceCount = hasTopology ? topology.faces.size() : render.indices.size()/3;
    const size_t vStart = std::min(static_cast<size_t>(offset), vertexCount);
    const size_t fStart = std::min(static_cast<size_t>(offset), faceCount);
    const size_t vEnd = std::min(vertexCount, vStart + static_cast<size_t>(limit));
    const size_t fEnd = std::min(faceCount, fStart + static_cast<size_t>(limit));
    std::string out = "{\\"object_id\\":" + std::to_string(id) +
        ",\\"topology\\":" + (hasTopology ? "true" : "false") +
        ",\\"vertex_count\\":" + std::to_string(vertexCount) +
        ",\\"face_count\\":" + std::to_string(faceCount) +
        ",\\"offset\\":" + std::to_string(offset) + ",\\"vertices\\":[";
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
    out += "],\\"faces\\":[";
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

void Editor::clearScene() {
    scene_ = Scene{};
    undo_.clear();
    redo_.clear();
}

void Editor::clearHistory() {
    undo_.clear();
    redo_.clear();
}

bool Editor::undo() {
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
                      R"JSON({"id":%d,"type":"%s","size":%.3f,"vertices":%zu,"triangles":%zu,"position":[%.3f,%.3f,%.3f],"rotation":[%.1f,%.1f,%.1f],"scale":[%.3f,%.3f,%.3f],"color":[%.2f,%.2f,%.2f]})JSON",
                      o.id, primitiveName(o.type), o.size, o.mesh.vertices.size(),
                      o.mesh.indices.size() / 3, o.transform.position.x,
                      o.transform.position.y, o.transform.position.z, o.transform.rotationDeg.x,
                      o.transform.rotationDeg.y, o.transform.rotationDeg.z, o.transform.scale.x,
                      o.transform.scale.y, o.transform.scale.z, o.color.r, o.color.g, o.color.b);
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
