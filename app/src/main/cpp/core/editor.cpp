#include "core/editor.h"

#include <algorithm>
#include <stdexcept>

#include "core/jsonout.h"

namespace core {

namespace {

constexpr size_t kMaxHistory = 60;

std::string colorJson(const Color& c) { return jvec(Vec3{c.r, c.g, c.b}); }

const char* lightTypeName(LightType t) {
    switch (t) {
        case LightType::Directional: return "directional";
        case LightType::Point: return "point";
        case LightType::Spot: return "spot";
    }
    return "directional";
}

}  // namespace

void refreshRender(Object& o) {
    if (!o.poly || o.poly->empty()) {
        o.render = std::make_shared<const Mesh>();
    } else {
        o.render = std::make_shared<const Mesh>(buildRenderMesh(*o.poly, o.smoothAngle));
    }
    ++o.meshVersion;
}

void setPoly(Object& o, PolyMesh poly) {
    o.poly = std::make_shared<const PolyMesh>(std::move(poly));
    refreshRender(o);
}

Object& addObjectTo(Scene& s, const std::string& kind, PolyMesh poly, const std::string& name,
                    Vec3 position) {
    Object o;
    o.id = s.nextId++;
    o.kind = kind;
    o.name = name.empty() ? kind + "_" + std::to_string(o.id) : name;
    o.transform.position = position;
    setPoly(o, std::move(poly));
    s.objects.push_back(std::move(o));
    s.selectedId = s.objects.back().id;
    return s.objects.back();
}

void eraseObject(Scene& s, int id) {
    for (Object& o : s.objects) {
        if (o.parent == id) o.parent = -1;
    }
    s.objects.erase(std::remove_if(s.objects.begin(), s.objects.end(),
                                   [id](const Object& o) { return o.id == id; }),
                    s.objects.end());
    if (s.selectedId == id) s.selectedId = -1;
}

PolyMesh makePrimitivePoly(const std::string& type, Vec3 dims, int segments) {
    PolyMesh m;
    if (type == "box" || type == "cube") {
        const int n = segments > 0 ? segments : 1;
        m = makeBoxPoly({1.0f, 1.0f, 1.0f}, n, n, n);
    } else if (type == "sphere") {
        m = makeSpherePoly(0.5f, segments > 0 ? segments : 8);
    } else if (type == "cylinder") {
        m = makeCylinderPoly(0.5f, 1.0f, segments > 0 ? segments : 24, 1, true);
    } else if (type == "cone") {
        m = makeConePoly(0.5f, 1.0f, segments > 0 ? segments : 24, true);
    } else if (type == "torus") {
        const int n = segments > 0 ? segments : 32;
        m = makeTorusPoly(0.35f, 0.15f, n, std::max(6, n / 2));
    } else if (type == "plane") {
        const int n = segments > 0 ? segments : 1;
        m = makePlanePoly(1.0f, 1.0f, n, n);
    } else {
        throw std::runtime_error("unknown primitive type: " + type);
    }
    for (Vec3& v : m.verts) v = Vec3{v.x * dims.x, v.y * dims.y, v.z * dims.z};
    return m;
}

std::string describeObject(const Scene& scene, const Object& o, bool detailed) {
    Vec3 mn, mx;
    objectBounds(o, mn, mx);
    JObj j;
    j.add("id", std::to_string(o.id))
        .add("name", jsonQuote(o.name))
        .add("kind", jsonQuote(o.kind))
        .add("parent", std::to_string(o.parent))
        .add("visible", jbool(o.visible))
        .add("position", jvec(o.transform.position))
        .add("rotation_degrees", jvec(o.transform.rotationDeg))
        .add("scale", jvec(o.transform.scale))
        .add("size", jvec(mx - mn))
        .add("faces", std::to_string(o.poly ? o.poly->faces.size() : 0));
    if (detailed) {
        JObj mat;
        mat.add("base_color", colorJson(o.material.base))
            .add("metallic", jnum(o.material.metallic))
            .add("roughness", jnum(o.material.roughness))
            .add("clearcoat", jnum(o.material.clearcoat))
            .add("emissive", colorJson(o.material.emissive))
            .add("emissive_strength", jnum(o.material.emissiveStrength))
            .add("opacity", jnum(o.material.opacity));
        JObj phys;
        phys.add("enabled", jbool(o.physics.enabled))
            .add("dynamic", jbool(o.physics.dynamic))
            .add("mass", jnum(o.physics.mass))
            .add("restitution", jnum(o.physics.restitution))
            .add("friction", jnum(o.physics.friction));
        j.add("vertices", std::to_string(o.poly ? o.poly->verts.size() : 0))
            .add("creased_edges", std::to_string(o.poly ? o.poly->creases.size() : 0))
            .add("bounds_min", jvec(mn))
            .add("bounds_max", jvec(mx))
            .add("smooth_angle", jnum(o.smoothAngle))
            .add("double_sided", jbool(o.doubleSided))
            .add("material", mat.str())
            .add("physics", phys.str());
        int children = 0;
        for (const Object& c : scene.objects) {
            if (c.parent == o.id) ++children;
        }
        j.add("children", std::to_string(children));
    }
    return j.str();
}

Editor::Editor() {
    Light sun;
    sun.id = scene_.nextId++;
    sun.name = "sun";
    sun.type = LightType::Directional;
    sun.direction = normalize(Vec3{-0.4f, -0.8f, -0.6f});
    sun.intensity = 3.0f;
    scene_.lights.push_back(sun);
}

bool Editor::transact(const std::function<bool(Scene&)>& fn) {
    Scene before = scene_;
    bool ok = false;
    try {
        ok = fn(scene_);
    } catch (...) {
        scene_ = std::move(before);
        throw;
    }
    if (!ok) {
        scene_ = std::move(before);
        return false;
    }
    undo_.push_back(std::move(before));
    if (undo_.size() > kMaxHistory) undo_.erase(undo_.begin());
    redo_.clear();
    return true;
}

bool Editor::undo() {
    if (undo_.empty()) return false;
    redo_.push_back(std::move(scene_));
    scene_ = std::move(undo_.back());
    undo_.pop_back();
    fixSelection();
    return true;
}

bool Editor::redo() {
    if (redo_.empty()) return false;
    undo_.push_back(std::move(scene_));
    scene_ = std::move(redo_.back());
    redo_.pop_back();
    fixSelection();
    return true;
}

void Editor::select(int id) {
    if (id < 0) {
        scene_.selectedId = -1;
    } else if (scene_.find(id) != nullptr) {
        scene_.selectedId = id;
    }
}

int Editor::pick(const Ray& ray) {
    scene_.selectedId = pickObject(scene_, ray);
    return scene_.selectedId;
}

int Editor::createPrimitive(const std::string& type, Vec3 dims, int segments, Vec3 position) {
    int id = -1;
    transact([&](Scene& s) {
        id = addObjectTo(s, type == "cube" ? "box" : type, makePrimitivePoly(type, dims, segments), "", position).id;
        return true;
    });
    return id;
}

std::string Editor::describeScene(int offset, int limit) const {
    offset = std::max(0, offset);
    limit = std::max(1, std::min(limit, 200));
    std::vector<std::string> objects;
    const size_t end = std::min(scene_.objects.size(), static_cast<size_t>(offset) + static_cast<size_t>(limit));
    for (size_t i = static_cast<size_t>(offset); i < end; ++i) {
        objects.push_back(describeObject(scene_, scene_.objects[i], false));
    }
    std::vector<std::string> lights;
    for (const Light& l : scene_.lights) {
        JObj j;
        j.add("id", std::to_string(l.id))
            .add("name", jsonQuote(l.name))
            .add("type", jsonQuote(lightTypeName(l.type)))
            .add("position", jvec(l.position))
            .add("direction", jvec(l.direction))
            .add("color", colorJson(l.color))
            .add("intensity", jnum(l.intensity))
            .add("range", jnum(l.range))
            .add("spot_angle_deg", jnum(l.spotAngleDeg))
            .add("enabled", jbool(l.enabled));
        lights.push_back(j.str());
    }
    JObj env;
    env.add("sky_top", colorJson(scene_.env.skyTop))
        .add("sky_horizon", colorJson(scene_.env.skyHorizon))
        .add("ground", colorJson(scene_.env.ground))
        .add("ambient", jnum(scene_.env.ambient))
        .add("exposure", jnum(scene_.env.exposure));
    JObj root;
    root.add("total_objects", std::to_string(scene_.objects.size()))
        .add("offset", std::to_string(offset))
        .add("objects", jlist(objects))
        .add("lights", jlist(lights))
        .add("environment", env.str())
        .add("selected", std::to_string(scene_.selectedId));
    return root.str();
}

void Editor::fixSelection() {
    if (scene_.selectedId >= 0 && scene_.find(scene_.selectedId) == nullptr) scene_.selectedId = -1;
}

}  // namespace core
