#include "core/editor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

#include "core/mesh_ops.h"

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

class MeshCommand : public Command {
public:
    MeshCommand(int id, Mesh before, Mesh after)
        : id_(id), before_(std::move(before)), after_(std::move(after)) {}
    void apply(Scene& scene) override {
        if (Object* o = scene.find(id_)) o->mesh = after_;
    }
    void revert(Scene& scene) override {
        if (Object* o = scene.find(id_)) o->mesh = before_;
    }
private:
    int id_;
    Mesh before_;
    Mesh after_;
};

class ReplaceObjectCommand : public Command {
public:
    ReplaceObjectCommand(Object aBefore, Object bSaved, Object aAfter, bool bDeleted)
        : aBefore_(std::move(aBefore)), bSaved_(std::move(bSaved)),
          aAfter_(std::move(aAfter)), bDeleted_(bDeleted) {}
    void apply(Scene& scene) override {
        if (Object* a = scene.find(aAfter_.id)) *a = aAfter_;
        if (bDeleted_) {
            const int i = scene.indexOf(bSaved_.id);
            if (i >= 0) scene.objects.erase(scene.objects.begin() + i);
        }
        scene.selectedId = aAfter_.id;
    }
    void revert(Scene& scene) override {
        if (Object* a = scene.find(aBefore_.id)) *a = aBefore_;
        if (bDeleted_ && scene.find(bSaved_.id) == nullptr) scene.objects.push_back(bSaved_);
        scene.selectedId = aBefore_.id;
    }
private:
    Object aBefore_, bSaved_, aAfter_;
    bool bDeleted_;
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
    object.mesh = makePrimitiveMesh(type, size);
    object.transform.position = position;
    object.name = std::string(primitiveName(type)) + "_" + std::to_string(object.id);
    const int id = object.id;
    execute(std::make_unique<AddCommand>(std::move(object)));
    return id;
}

int Editor::cloneObject(int id) {
    if (scene_.objects.size() >= kMaxObjects) return -1;
    const Object* src = scene_.find(id);
    if (src == nullptr) return -1;
    Object object = *src;
    object.id = scene_.nextId++;
    object.name = src->name + "_clone_" + std::to_string(object.id);
    object.transform.position.x += 0.15f;
    const int newId = object.id;
    execute(std::make_unique<AddCommand>(std::move(object)));
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
    if (!finite3(transform.position) || !finite3(transform.rotationDeg) || !finite3(transform.scale))
        return false;
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

bool Editor::setColor(int id, Color color) {
    Object* o = scene_.find(id);
    if (o == nullptr) return false;
    if (!std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b)) return false;
    color.r = std::clamp(color.r, 0.0f, 1.0f);
    color.g = std::clamp(color.g, 0.0f, 1.0f);
    color.b = std::clamp(color.b, 0.0f, 1.0f);
    color.metallic = std::clamp(color.metallic, 0.0f, 1.0f);
    color.roughness = std::clamp(color.roughness, 0.04f, 1.0f);
    execute(std::make_unique<ColorCommand>(id, o->color, color));
    return true;
}

bool Editor::meshOp(int id, const std::string& op, float amount, int segments,
                    const std::vector<int>& faceIndices) {
    Object* o = scene_.find(id);
    if (o == nullptr) return false;
    Mesh before = o->mesh;
    Mesh after = before;
    bool ok = true;

    if (op == "soft_normals") {
        recomputeSmoothNormals(after);
    } else if (op == "hard_normals") {
        recomputeHardNormals(after);
    } else if (op == "divide" || op == "subdivide") {
        ok = subdivideMesh(after);
    } else if (op == "triangulate") {
        ok = triangulateMesh(after);
    } else if (op == "smooth") {
        smoothMesh(after, std::max(1, segments), amount > 0.0f ? amount : 0.5f);
    } else if (op == "weld" || op == "merge_vertices") {
        weldVertices(after, amount > 0.0f ? amount : 1e-4f);
    } else if (op == "mirror") {
        mirrorMesh(after, segments, true);
    } else if (op == "extrude") {
        ok = extrudeFaces(after, faceIndices.empty() ? selectedFaces_ : faceIndices,
                          amount != 0.0f ? amount : 0.1f);
    } else if (op == "bevel") {
        ok = bevelMesh(after, amount > 0.0f ? amount : 0.05f, std::max(1, segments));
    } else if (op == "create_face") {
        if (faceIndices.size() < 3) return false;
        ok = createFace(after, faceIndices[0], faceIndices[1], faceIndices[2]);
    } else if (op == "connect") {
        if (faceIndices.size() < 2) return false;
        ok = connectVertices(after, faceIndices[0], faceIndices[1]);
    } else if (op == "target_merge") {
        if (faceIndices.size() < 2) return false;
        std::vector<int> sources(faceIndices.begin() + 1, faceIndices.end());
        ok = targetMerge(after, faceIndices[0], sources);
    } else {
        return false;
    }

    if (!ok) return false;
    execute(std::make_unique<MeshCommand>(id, std::move(before), std::move(after)));
    return true;
}

int Editor::combineObjects(int idA, int idB) {
    Object* a = scene_.find(idA);
    Object* b = scene_.find(idB);
    if (a == nullptr || b == nullptr || idA == idB) return -1;
    Object aBefore = *a;
    Object bSaved = *b;
    Object aAfter = *a;
    Mesh worldB = b->mesh;
    const Mat4 mb = b->transform.matrix();
    for (auto& v : worldB.vertices) {
        Vec3 p = transformPoint(mb, {v.px, v.py, v.pz});
        v.px = p.x; v.py = p.y; v.pz = p.z;
    }
    Mesh worldA = a->mesh;
    const Mat4 ma = a->transform.matrix();
    for (auto& v : worldA.vertices) {
        Vec3 p = transformPoint(ma, {v.px, v.py, v.pz});
        v.px = p.x; v.py = p.y; v.pz = p.z;
    }
    appendMesh(worldA, worldB);
    aAfter.mesh = std::move(worldA);
    aAfter.transform = Transform{};
    aAfter.transform.position = a->transform.position;
    execute(std::make_unique<ReplaceObjectCommand>(std::move(aBefore), std::move(bSaved),
                                                   std::move(aAfter), true));
    return idA;
}

int Editor::separateFaces(int id, const std::vector<int>& faceIndices) {
    if (scene_.objects.size() >= kMaxObjects) return -1;
    Object* o = scene_.find(id);
    if (o == nullptr) return -1;
    const std::vector<int>& faces = faceIndices.empty() ? selectedFaces_ : faceIndices;
    if (faces.empty()) return -1;
    Mesh extracted = extractFaces(o->mesh, faces);
    if (extracted.indices.empty()) return -1;
    Mesh remaining = o->mesh;
    removeFaces(remaining, faces);
    Object neu;
    neu.id = scene_.nextId++;
    neu.type = o->type;
    neu.size = o->size;
    neu.mesh = std::move(extracted);
    neu.transform = o->transform;
    neu.color = o->color;
    neu.name = o->name + "_part_" + std::to_string(neu.id);
    Mesh before = o->mesh;
    execute(std::make_unique<MeshCommand>(id, before, remaining));
    const int newId = neu.id;
    execute(std::make_unique<AddCommand>(std::move(neu)));
    return newId;
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
    char buf[768];
    bool first = true;
    for (const Object& o : scene_.objects) {
        if (!first) out += ",";
        first = false;
        const auto st = meshStats(o.mesh);
        std::snprintf(
            buf, sizeof(buf),
            R"JSON({"id":%d,"type":"%s","size":%.3f,"position":[%.3f,%.3f,%.3f],"rotation":[%.1f,%.1f,%.1f],"scale":[%.3f,%.3f,%.3f],"color":[%.2f,%.2f,%.2f],"metallic":%.2f,"roughness":%.2f,"vertices":%d,"triangles":%d})JSON",
            o.id, primitiveName(o.type), o.size, o.transform.position.x, o.transform.position.y,
            o.transform.position.z, o.transform.rotationDeg.x, o.transform.rotationDeg.y,
            o.transform.rotationDeg.z, o.transform.scale.x, o.transform.scale.y, o.transform.scale.z,
            o.color.r, o.color.g, o.color.b, o.color.metallic, o.color.roughness, st.vertices,
            st.triangles);
        out += buf;
    }
    std::snprintf(buf, sizeof(buf),
                  R"JSON(],"selected":%d,"select_mode":"%s","selected_faces":%zu})JSON",
                  scene_.selectedId,
                  selectMode_ == SelectMode::Face ? "face"
                      : selectMode_ == SelectMode::Edge ? "edge"
                      : selectMode_ == SelectMode::Vertex ? "vertex" : "object",
                  selectedFaces_.size());
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
