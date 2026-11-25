#include "core/editor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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
    object.mesh = makePrimitiveMesh(type, size);
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
    Object object;
    object.id=scene_.nextId++; object.type=Primitive::CustomMesh; object.size=1.0f;
    object.name="loft_mesh_"+std::to_string(object.id); object.mesh=std::move(mesh);
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
