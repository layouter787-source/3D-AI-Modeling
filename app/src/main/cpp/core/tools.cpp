#include "core/tools.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <initializer_list>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/json.h"
#include "core/jsonout.h"
#include "core/polymesh.h"

namespace core {

namespace {

struct ToolError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct ToolOutput {
    bool ok = true;
    std::string message;
    std::string data;
};

ToolOutput okOut(std::string message, std::string data = "") {
    ToolOutput o;
    o.ok = true;
    o.message = std::move(message);
    o.data = std::move(data);
    return o;
}

Vec3 vec3Of(const Json* v, const char* key) {
    if (v == nullptr || !v->isArray() || v->a.size() != 3) {
        throw ToolError(std::string(key) + " must be an array of 3 numbers");
    }
    Vec3 r;
    float* out[3] = {&r.x, &r.y, &r.z};
    for (size_t i = 0; i < 3; ++i) {
        if (!v->a[i].isNumber() || !std::isfinite(v->a[i].n)) {
            throw ToolError(std::string(key) + " has an invalid number");
        }
        *out[i] = static_cast<float>(v->a[i].n);
    }
    return r;
}

// Argumentos de uma chamada, com validação. Qualquer erro vira uma resposta de falha para a IA.
struct Args {
    const Json& j;

    const Json* find(const char* key) const { return j.get(key); }

    bool has(const char* key) const {
        const Json* v = find(key);
        return v != nullptr && v->type != Json::Type::Null;
    }

    double num(const char* key) const {
        const Json* v = find(key);
        if (v == nullptr || !v->isNumber() || !std::isfinite(v->n)) {
            throw ToolError(std::string("missing or invalid number: ") + key);
        }
        return v->n;
    }
    double num(const char* key, double fallback) const { return has(key) ? num(key) : fallback; }
    float f(const char* key, float fallback) const { return static_cast<float>(num(key, fallback)); }
    int integer(const char* key) const { return static_cast<int>(std::llround(num(key))); }
    int integer(const char* key, int fallback) const { return has(key) ? integer(key) : fallback; }

    bool boolean(const char* key, bool fallback) const {
        const Json* v = find(key);
        if (v == nullptr || !v->isBool()) return fallback;
        return v->b;
    }

    std::string str(const char* key) const {
        const Json* v = find(key);
        if (v == nullptr || !v->isString()) throw ToolError(std::string("missing or invalid string: ") + key);
        return v->s;
    }
    std::string str(const char* key, const std::string& fallback) const {
        return has(key) ? str(key) : fallback;
    }

    Vec3 vec3(const char* key) const { return vec3Of(find(key), key); }
    Vec3 vec3(const char* key, Vec3 fallback) const { return has(key) ? vec3(key) : fallback; }

    std::vector<int> ints(const char* key) const {
        const Json* v = find(key);
        if (v == nullptr || !v->isArray()) throw ToolError(std::string(key) + " must be an array of integers");
        std::vector<int> out;
        out.reserve(v->a.size());
        for (const Json& x : v->a) {
            if (!x.isNumber() || !std::isfinite(x.n)) throw ToolError(std::string(key) + " has an invalid integer");
            out.push_back(static_cast<int>(std::llround(x.n)));
        }
        return out;
    }

    const Json& array(const char* key) const {
        const Json* v = find(key);
        if (v == nullptr || !v->isArray()) throw ToolError(std::string(key) + " must be an array");
        return *v;
    }
};

// ---------------------------------------------------------------- Esquemas dos parâmetros

struct Param {
    std::string name;
    std::string schema;
    bool required = false;
};

std::string withDesc(const std::string& head, const std::string& description) {
    return head + R"J(,"description":)J" + jsonQuote(description) + "}";
}

Param pNum(const char* n, const char* d, bool req = false) {
    return {n, withDesc(R"J({"type":"number")J", d), req};
}
Param pInt(const char* n, const char* d, bool req = false) {
    return {n, withDesc(R"J({"type":"integer")J", d), req};
}
Param pStr(const char* n, const char* d, bool req = false) {
    return {n, withDesc(R"J({"type":"string")J", d), req};
}
Param pBool(const char* n, const char* d, bool req = false) {
    return {n, withDesc(R"J({"type":"boolean")J", d), req};
}
Param pEnum(const char* n, const char* d, std::initializer_list<const char*> values, bool req = false) {
    std::string list;
    for (const char* v : values) {
        if (!list.empty()) list += ",";
        list += jsonQuote(v);
    }
    return {n, withDesc(R"J({"type":"string","enum":[)J" + list + "]", d), req};
}
Param pVec3(const char* n, const char* d, bool req = false) {
    return {n, withDesc(R"J({"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3)J", d), req};
}
Param pInts(const char* n, const char* d, bool req = false) {
    return {n, withDesc(R"J({"type":"array","items":{"type":"integer"})J", d), req};
}
Param pArr(const char* n, const char* d, const char* items, bool req = false) {
    return {n, withDesc(std::string(R"J({"type":"array","items":)J") + items, d), req};
}

const char* kVec3Items = R"J({"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3})J";
const char* kVec2Items = R"J({"type":"array","items":{"type":"number"},"minItems":2,"maxItems":2})J";
const char* kIntPairItems = R"J({"type":"array","items":{"type":"integer"},"minItems":2,"maxItems":2})J";
const char* kSectionItems =
    R"J({"type":"array","items":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3}})J";
const char* kStationItems =
    R"J({"type":"object","properties":{"x":{"type":"number","description":"position along the body axis (X)"},"width":{"type":"number","description":"full width (Z)"},"height":{"type":"number","description":"full height (Y)"},"center_y":{"type":"number"},"center_z":{"type":"number"},"power":{"type":"number","description":"2 = ellipse, 3-5 = rounded box"}},"required":["x","width","height"]})J";

using ToolFn = std::function<ToolOutput(Editor&, const Args&)>;
using MeshFn = std::function<PolyMesh(const PolyMesh&, const Args&)>;

struct Tool {
    std::string name;
    std::string description;
    std::vector<Param> params;
    bool mutating = false;
    ToolFn fn;
};

// ---------------------------------------------------------------- Auxiliares

Object& getObject(Scene& s, int id) {
    Object* o = s.find(id);
    if (o == nullptr) throw ToolError("object not found: " + std::to_string(id));
    return *o;
}

Object& obj(Scene& s, const Args& a, const char* key = "object_id") { return getObject(s, a.integer(key)); }

const PolyMesh& polyOf(const Object& o) {
    if (!o.poly) throw ToolError("object has no mesh");
    return *o.poly;
}

std::string idData(int id) { return JObj().add("object_id", std::to_string(id)).str(); }

std::string stats(const PolyMesh& m) {
    return std::to_string(m.verts.size()) + " vertices, " + std::to_string(m.faces.size()) + " faces";
}

int axisOf(const std::string& s) {
    if (s == "x") return 0;
    if (s == "y") return 1;
    if (s == "z") return 2;
    throw ToolError("axis must be x, y or z");
}

Color toColor(Vec3 v) {
    Color c;
    c.r = std::max(0.0f, std::min(v.x, 1.0f));
    c.g = std::max(0.0f, std::min(v.y, 1.0f));
    c.b = std::max(0.0f, std::min(v.z, 1.0f));
    return c;
}

float clamp01(float v) { return std::max(0.0f, std::min(v, 1.0f)); }

uint64_t edgeKeyOf(uint32_t a, uint32_t b) {
    if (a > b) std::swap(a, b);
    return (static_cast<uint64_t>(a) << 32) | b;
}

Profile readProfile(const Json& arr, const char* what) {
    if (arr.a.size() > 5000) throw ToolError(std::string(what) + " has too many points");
    Profile p;
    for (const Json& pt : arr.a) {
        if (!pt.isArray() || pt.a.size() != 2 || !pt.a[0].isNumber() || !pt.a[1].isNumber() ||
            !std::isfinite(pt.a[0].n) || !std::isfinite(pt.a[1].n)) {
            throw ToolError(std::string(what) + " points must be [a, b] number pairs");
        }
        p.push_back({static_cast<float>(pt.a[0].n), static_cast<float>(pt.a[1].n)});
    }
    return p;
}

std::vector<Vec3> readPoints(const Json& arr, const char* what) {
    if (arr.a.size() > 20000) throw ToolError(std::string(what) + " has too many points");
    std::vector<Vec3> out;
    for (const Json& pt : arr.a) out.push_back(vec3Of(&pt, what));
    return out;
}

std::vector<int> faceSelection(const Args& a, const PolyMesh& m) {
    std::vector<int> out;
    if (a.boolean("all_faces", false)) {
        out.resize(m.faces.size());
        std::iota(out.begin(), out.end(), 0);
        return out;
    }
    out = a.ints("faces");
    if (out.empty()) throw ToolError("no faces selected");
    for (int f : out) {
        if (f < 0 || static_cast<size_t>(f) >= m.faces.size()) {
            throw ToolError("face index out of range: " + std::to_string(f));
        }
    }
    return out;
}

// Parâmetros comuns às ferramentas que criam objetos.
std::vector<Param> withCreation(std::vector<Param> specific) {
    specific.push_back(pVec3("position", "World position of the new object, default [0,0,0]"));
    specific.push_back(pVec3("rotation_degrees", "Euler XYZ rotation in degrees"));
    specific.push_back(pStr("name", "Name for the object"));
    specific.push_back(pVec3("color", "Base color [r,g,b], each 0..1"));
    specific.push_back(pNum("smooth_angle", "Shading: edges sharper than this angle (degrees) look hard. 0 = flat, 180 = fully smooth. Default 45"));
    return specific;
}

Object& createFrom(Scene& s, const Args& a, const std::string& kind, PolyMesh poly) {
    Object& o = addObjectTo(s, kind, std::move(poly), a.str("name", ""), a.vec3("position", Vec3{}));
    if (a.has("rotation_degrees")) o.transform.rotationDeg = a.vec3("rotation_degrees");
    if (a.has("color")) o.material.base = toColor(a.vec3("color"));
    if (a.has("smooth_angle")) {
        o.smoothAngle = std::max(0.0f, std::min(a.f("smooth_angle", 45.0f), 180.0f));
        refreshRender(o);
    }
    return o;
}

ToolOutput created(const Object& o) {
    return okOut("Created '" + o.name + "' (" + (o.poly ? stats(*o.poly) : std::string("empty")) + ")", idData(o.id));
}

void applyPreset(Material& m, const std::string& p, bool hasColor, Color c) {
    m = Material();
    if (p == "car_paint") {
        m.base = hasColor ? c : Color{0.55f, 0.02f, 0.02f};
        m.metallic = 0.6f;
        m.roughness = 0.35f;
        m.clearcoat = 1.0f;
    } else if (p == "chrome") {
        m.base = Color{0.95f, 0.95f, 0.97f};
        m.metallic = 1.0f;
        m.roughness = 0.05f;
    } else if (p == "glass") {
        m.base = hasColor ? c : Color{0.65f, 0.80f, 0.90f};
        m.roughness = 0.03f;
        m.opacity = 0.25f;
    } else if (p == "rubber") {
        m.base = Color{0.03f, 0.03f, 0.03f};
        m.roughness = 0.9f;
    } else if (p == "plastic") {
        m.base = hasColor ? c : Color{0.10f, 0.10f, 0.10f};
        m.roughness = 0.4f;
    } else if (p == "aluminum") {
        m.base = Color{0.91f, 0.92f, 0.92f};
        m.metallic = 1.0f;
        m.roughness = 0.35f;
    } else if (p == "steel") {
        m.base = Color{0.56f, 0.57f, 0.58f};
        m.metallic = 1.0f;
        m.roughness = 0.4f;
    } else if (p == "gold") {
        m.base = Color{1.0f, 0.77f, 0.34f};
        m.metallic = 1.0f;
        m.roughness = 0.25f;
    } else if (p == "copper") {
        m.base = Color{0.95f, 0.64f, 0.54f};
        m.metallic = 1.0f;
        m.roughness = 0.3f;
    } else if (p == "wood") {
        m.base = hasColor ? c : Color{0.45f, 0.28f, 0.14f};
        m.roughness = 0.7f;
    } else if (p == "concrete") {
        m.base = Color{0.55f, 0.55f, 0.55f};
        m.roughness = 0.95f;
    } else if (p == "leather") {
        m.base = hasColor ? c : Color{0.25f, 0.12f, 0.07f};
        m.roughness = 0.55f;
    } else if (p == "emissive") {
        m.base = hasColor ? c : Color{1.0f, 1.0f, 1.0f};
        m.emissive = m.base;
        m.emissiveStrength = 3.0f;
    } else if (p == "headlight") {
        m.base = Color{1.0f, 1.0f, 0.95f};
        m.emissive = m.base;
        m.emissiveStrength = 4.0f;
        m.roughness = 0.1f;
    } else {
        throw ToolError("unknown material preset: " + p);
    }
}

// Ruído de valor 3D suave, determinístico.
float hash3(int x, int y, int z, int seed) {
    unsigned h = static_cast<unsigned>(x) * 374761393u + static_cast<unsigned>(y) * 668265263u +
                 static_cast<unsigned>(z) * 2147483647u + static_cast<unsigned>(seed) * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0xffffffu) / static_cast<float>(0xffffff) * 2.0f - 1.0f;
}

float valueNoise(Vec3 p, int seed) {
    const int xi = static_cast<int>(std::floor(p.x)), yi = static_cast<int>(std::floor(p.y)),
              zi = static_cast<int>(std::floor(p.z));
    const float fx = p.x - static_cast<float>(xi), fy = p.y - static_cast<float>(yi), fz = p.z - static_cast<float>(zi);
    auto fade = [](float t) { return t * t * (3.0f - 2.0f * t); };
    const float u = fade(fx), v = fade(fy), w = fade(fz);
    auto mix = [](float a, float b, float t) { return a + (b - a) * t; };
    float c[2][2][2];
    for (int dx = 0; dx < 2; ++dx)
        for (int dy = 0; dy < 2; ++dy)
            for (int dz = 0; dz < 2; ++dz) c[dx][dy][dz] = hash3(xi + dx, yi + dy, zi + dz, seed);
    const float x00 = mix(c[0][0][0], c[1][0][0], u), x10 = mix(c[0][1][0], c[1][1][0], u);
    const float x01 = mix(c[0][0][1], c[1][0][1], u), x11 = mix(c[0][1][1], c[1][1][1], u);
    return mix(mix(x00, x10, v), mix(x01, x11, v), w);
}

PolyMesh deformMesh(const PolyMesh& in, const Args& a) {
    const std::string type = a.str("type");
    const int axis = axisOf(a.str("axis", "y"));
    const float amount = a.f("amount", 0.0f);
    PolyMesh m = in;
    Vec3 mn, mx;
    meshBounds(m, mn, mx);
    const float lo = axisVal(mn, axis), hi = axisVal(mx, axis);
    const float len = std::max(hi - lo, 1e-6f);
    const float mid = (lo + hi) * 0.5f;
    const int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;

    if (type == "bend") {
        const float theta = radians(amount);
        if (std::fabs(theta) < 1e-5f) return m;
        const float R = len / theta;
        for (Vec3& p : m.verts) {
            const float u = axisVal(p, axis) - mid;
            const float b = axisVal(p, a1);
            const float phi = u / R;
            axisRef(p, axis) = (R - b) * std::sin(phi) + mid;
            axisRef(p, a1) = R - (R - b) * std::cos(phi);
        }
    } else if (type == "twist") {
        for (Vec3& p : m.verts) {
            const float ang = radians(amount) * (axisVal(p, axis) - mid) / len;
            const float c = std::cos(ang), s = std::sin(ang);
            const float x = axisVal(p, a1), y = axisVal(p, a2);
            axisRef(p, a1) = x * c - y * s;
            axisRef(p, a2) = x * s + y * c;
        }
    } else if (type == "taper") {
        for (Vec3& p : m.verts) {
            const float t = (axisVal(p, axis) - lo) / len;
            const float k = 1.0f + (amount - 1.0f) * t;
            axisRef(p, a1) *= k;
            axisRef(p, a2) *= k;
        }
    } else if (type == "stretch") {
        for (Vec3& p : m.verts) axisRef(p, axis) = mid + (axisVal(p, axis) - mid) * amount;
    } else if (type == "shear") {
        for (Vec3& p : m.verts) axisRef(p, a1) += amount * (axisVal(p, axis) - mid);
    } else if (type == "spherify") {
        const Vec3 c = (mn + mx) * 0.5f;
        float avg = 0.0f;
        for (const Vec3& p : m.verts) avg += length(p - c);
        avg /= static_cast<float>(std::max<size_t>(1, m.verts.size()));
        const float k = clamp01(amount);
        for (Vec3& p : m.verts) p = lerp(p, c + normalize(p - c) * avg, k);
    } else if (type == "inflate") {
        const std::vector<Vec3> n = vertexNormals(m);
        for (size_t i = 0; i < m.verts.size(); ++i) m.verts[i] += n[i] * amount;
    } else if (type == "noise") {
        const float freq = a.f("frequency", 1.0f);
        const int seed = a.integer("seed", 1);
        const std::vector<Vec3> n = vertexNormals(m);
        for (size_t i = 0; i < m.verts.size(); ++i) m.verts[i] += n[i] * (valueNoise(m.verts[i] * freq, seed) * amount);
    } else {
        throw ToolError("unknown deform type: " + type);
    }
    return m;
}

void detachChildrenCheck(const Scene& s, int id) {
    for (const Object& c : s.objects) {
        if (c.parent == id) throw ToolError("object has children; unparent or join them first");
    }
}

float signedVolume(const PolyMesh& m) {
    double vol = 0.0;
    for (const auto& f : m.faces) {
        for (size_t k = 1; k + 1 < f.size(); ++k) {
            const Vec3 a = m.verts[f[0]], b = m.verts[f[k]], c = m.verts[f[k + 1]];
            vol += dot(a, cross(b, c)) / 6.0;
        }
    }
    return static_cast<float>(vol);
}

ToolOutput simulatePhysics(Scene& s, const Args& a) {
    const float seconds = std::max(0.05f, std::min(a.f("seconds", 2.0f), 10.0f));
    const float gravity = a.f("gravity", 9.81f);

    struct Body {
        Object* o = nullptr;
        Vec3 half, pos, start, vel;
        bool dynamic = true;
        float invMass = 1.0f, restitution = 0.2f, friction = 0.5f;
    };
    std::vector<Body> bodies;
    bool anyDynamic = false;
    for (Object& o : s.objects) {
        if (!o.physics.enabled || !o.visible || o.parent >= 0 || !o.poly || o.poly->empty()) continue;
        Vec3 mn, mx;
        worldBounds(s, o, mn, mx);
        Body b;
        b.o = &o;
        b.half = (mx - mn) * 0.5f;
        b.pos = (mx + mn) * 0.5f;
        b.start = b.pos;
        b.dynamic = o.physics.dynamic;
        b.invMass = b.dynamic ? 1.0f / std::max(o.physics.mass, 0.001f) : 0.0f;
        b.restitution = o.physics.restitution;
        b.friction = o.physics.friction;
        anyDynamic = anyDynamic || b.dynamic;
        bodies.push_back(b);
    }
    if (!anyDynamic) throw ToolError("no dynamic bodies; use set_physics first");
    if (bodies.size() > 120) throw ToolError("too many physics bodies (max 120)");

    const float dt = 1.0f / 120.0f;
    const int steps = static_cast<int>(seconds / dt);
    for (int step = 0; step < steps; ++step) {
        for (Body& b : bodies) {
            if (!b.dynamic) continue;
            b.vel.y -= gravity * dt;
            b.pos += b.vel * dt;
        }
        for (Body& b : bodies) {  // chão em y = 0
            if (!b.dynamic) continue;
            const float minY = b.pos.y - b.half.y;
            if (minY < 0.0f) {
                b.pos.y -= minY;
                if (b.vel.y < 0.0f) {
                    b.vel.y = -b.vel.y * b.restitution;
                    if (b.vel.y < 0.3f) b.vel.y = 0.0f;
                }
                const float damp = std::max(0.0f, 1.0f - b.friction * 4.0f * dt);
                b.vel.x *= damp;
                b.vel.z *= damp;
            }
        }
        for (int pass = 0; pass < 2; ++pass) {
            for (size_t i = 0; i < bodies.size(); ++i) {
                for (size_t j = i + 1; j < bodies.size(); ++j) {
                    Body& A = bodies[i];
                    Body& B = bodies[j];
                    if (!A.dynamic && !B.dynamic) continue;
                    const float ox = A.half.x + B.half.x - std::fabs(A.pos.x - B.pos.x);
                    const float oy = A.half.y + B.half.y - std::fabs(A.pos.y - B.pos.y);
                    const float oz = A.half.z + B.half.z - std::fabs(A.pos.z - B.pos.z);
                    if (ox <= 0.0f || oy <= 0.0f || oz <= 0.0f) continue;
                    int axis = 0;
                    float overlap = ox;
                    if (oy < overlap) {
                        axis = 1;
                        overlap = oy;
                    }
                    if (oz < overlap) {
                        axis = 2;
                        overlap = oz;
                    }
                    const float sign = axisVal(A.pos, axis) >= axisVal(B.pos, axis) ? 1.0f : -1.0f;
                    const float invSum = A.invMass + B.invMass;
                    axisRef(A.pos, axis) += sign * overlap * (A.invMass / invSum);
                    axisRef(B.pos, axis) -= sign * overlap * (B.invMass / invSum);
                    const float vn = (axisVal(A.vel, axis) - axisVal(B.vel, axis)) * sign;
                    if (vn < 0.0f) {
                        const float e = vn < -0.5f ? std::min(A.restitution, B.restitution) : 0.0f;
                        const float jimp = -(1.0f + e) * vn / invSum;
                        axisRef(A.vel, axis) += sign * jimp * A.invMass;
                        axisRef(B.vel, axis) -= sign * jimp * B.invMass;
                    }
                }
            }
        }
    }
    int moved = 0;
    for (Body& b : bodies) {
        if (!b.dynamic) continue;
        b.o->transform.position += b.pos - b.start;
        ++moved;
    }
    return okOut("Simulated " + jnum(seconds) + " s; moved " + std::to_string(moved) + " bodies");
}

// ---------------------------------------------------------------- Registo das ferramentas

std::vector<Tool> buildTools() {
    std::vector<Tool> t;

    auto add = [&t](const char* name, const char* description, std::vector<Param> params, bool mutating,
                    ToolFn fn) {
        Tool tool;
        tool.name = name;
        tool.description = description;
        tool.params = std::move(params);
        tool.mutating = mutating;
        tool.fn = std::move(fn);
        t.push_back(std::move(tool));
    };

    // Ferramenta que transforma a malha editável de um objeto.
    auto addMesh = [&add](const char* name, const char* description, std::vector<Param> params, MeshFn fn) {
        params.insert(params.begin(), pInt("object_id", "Target object id", true));
        add(name, description, std::move(params), true, [fn](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            Object& o = obj(s, a);
            PolyMesh result = fn(polyOf(o), a);
            if (result.empty()) throw ToolError("the operation produced an empty mesh");
            setPoly(o, std::move(result));
            return okOut("OK: " + stats(*o.poly), idData(o.id));
        });
    };

    // ===== Consulta =====
    add("get_scene",
        "Summarize the scene: objects (paged), lights, environment and selection. Call it to learn object ids and sizes.",
        {pInt("offset", "First object index, default 0"), pInt("limit", "Max objects, default 60, max 200")}, false,
        [](Editor& e, const Args& a) {
            return okOut("scene", e.describeScene(a.integer("offset", 0), a.integer("limit", 60)));
        });

    add("get_object", "Full details of one object: transform, bounds, material, physics, counts.",
        {pInt("object_id", "Object id", true)}, false, [](Editor& e, const Args& a) {
            const Scene& s = e.scene();
            const Object* o = s.find(a.integer("object_id"));
            if (o == nullptr) throw ToolError("object not found");
            return okOut("object", describeObject(s, *o, true));
        });

    add("get_mesh_data",
        "Read raw mesh data (local space) in pages: vertex positions or face vertex-index lists.",
        {pInt("object_id", "Object id", true), pEnum("what", "vertices or faces", {"vertices", "faces"}),
         pInt("offset", "Start index, default 0"), pInt("limit", "Max items, default 100, max 500")},
        false, [](Editor& e, const Args& a) {
            const Object* o = e.scene().find(a.integer("object_id"));
            if (o == nullptr || !o->poly) throw ToolError("object not found or has no mesh");
            const PolyMesh& m = *o->poly;
            const std::string what = a.str("what", "vertices");
            const size_t offset = static_cast<size_t>(std::max(0, a.integer("offset", 0)));
            const size_t limit = static_cast<size_t>(std::max(1, std::min(500, a.integer("limit", 100))));
            std::vector<std::string> items;
            size_t total = 0;
            if (what == "vertices") {
                total = m.verts.size();
                for (size_t i = offset; i < total && items.size() < limit; ++i) items.push_back(jvec(m.verts[i]));
            } else if (what == "faces") {
                total = m.faces.size();
                for (size_t i = offset; i < total && items.size() < limit; ++i) {
                    std::vector<std::string> ids;
                    for (uint32_t v : m.faces[i]) ids.push_back(std::to_string(v));
                    items.push_back(jlist(ids));
                }
            } else {
                throw ToolError("what must be vertices or faces");
            }
            return okOut("mesh data", JObj().add("total", std::to_string(total)).add("offset", std::to_string(offset)).add(what, jlist(items)).str());
        });

    add("analyze_mesh",
        "Quality report: face types, open boundary edges, non-manifold edges, degenerate faces, winding consistency and volume sign (negative = normals point inward). Use it to check work.",
        {pInt("object_id", "Object id", true)}, false, [](Editor& e, const Args& a) {
            const Object* o = e.scene().find(a.integer("object_id"));
            if (o == nullptr || !o->poly) throw ToolError("object not found or has no mesh");
            const PolyMesh& m = *o->poly;
            std::unordered_map<uint64_t, int> undirected, directed;
            size_t tris = 0, quads = 0, ngons = 0, degenerate = 0;
            for (const auto& f : m.faces) {
                if (f.size() == 3) ++tris;
                else if (f.size() == 4) ++quads;
                else ++ngons;
                if (length(newellNormal(m, f)) < 1e-10f) ++degenerate;
                for (size_t k = 0; k < f.size(); ++k) {
                    const uint32_t u = f[k], v = f[(k + 1) % f.size()];
                    undirected[edgeKeyOf(u, v)]++;
                    directed[(static_cast<uint64_t>(u) << 32) | v]++;
                }
            }
            size_t boundary = 0, nonManifold = 0, inconsistent = 0;
            for (const auto& kv : undirected) {
                if (kv.second == 1) ++boundary;
                else if (kv.second > 2) ++nonManifold;
            }
            for (const auto& kv : directed) {
                if (kv.second > 1) ++inconsistent;
            }
            Vec3 mn, mx;
            meshBounds(m, mn, mx);
            JObj j;
            j.add("vertices", std::to_string(m.verts.size()))
                .add("faces", std::to_string(m.faces.size()))
                .add("triangles", std::to_string(tris))
                .add("quads", std::to_string(quads))
                .add("ngons", std::to_string(ngons))
                .add("degenerate_faces", std::to_string(degenerate))
                .add("boundary_edges", std::to_string(boundary))
                .add("non_manifold_edges", std::to_string(nonManifold))
                .add("inconsistent_winding_edges", std::to_string(inconsistent))
                .add("signed_volume", jnum(signedVolume(m)))
                .add("closed", jbool(boundary == 0 && nonManifold == 0))
                .add("size", jvec(mx - mn));
            return okOut("analysis", j.str());
        });

    add("select_faces",
        "Find face indices by orientation and/or location (local space). Returns up to 5000 indices. Indices are only valid until the mesh topology changes.",
        {pInt("object_id", "Object id", true), pVec3("normal", "Keep faces whose normal points this way"),
         pNum("angle_deg", "Tolerance for normal, default 30"), pVec3("min", "Box min for face centers"),
         pVec3("max", "Box max for face centers")},
        false, [](Editor& e, const Args& a) {
            const Object* o = e.scene().find(a.integer("object_id"));
            if (o == nullptr || !o->poly) throw ToolError("object not found or has no mesh");
            const PolyMesh& m = *o->poly;
            const bool useN = a.has("normal");
            const Vec3 n = useN ? normalize(a.vec3("normal")) : Vec3{};
            const float cosT = std::cos(radians(a.f("angle_deg", 30.0f)));
            const bool useBox = a.has("min") && a.has("max");
            const Vec3 mn = a.vec3("min", Vec3{}), mx = a.vec3("max", Vec3{});
            std::vector<std::string> ids;
            size_t count = 0;
            for (size_t f = 0; f < m.faces.size(); ++f) {
                if (useN && dot(faceNormal(m, f), n) < cosT) continue;
                if (useBox) {
                    const Vec3 c = faceCenter(m, f);
                    if (c.x < mn.x || c.y < mn.y || c.z < mn.z || c.x > mx.x || c.y > mx.y || c.z > mx.z) continue;
                }
                ++count;
                if (ids.size() < 5000) ids.push_back(std::to_string(f));
            }
            return okOut("found " + std::to_string(count) + " faces",
                         JObj().add("count", std::to_string(count)).add("faces", jlist(ids)).str());
        });

    add("find_vertices",
        "Find vertex indices inside a box (min/max) or a sphere (center/radius), in local space. Max 5000 results.",
        {pInt("object_id", "Object id", true), pVec3("min", "Box min"), pVec3("max", "Box max"),
         pVec3("center", "Sphere center"), pNum("radius", "Sphere radius")},
        false, [](Editor& e, const Args& a) {
            const Object* o = e.scene().find(a.integer("object_id"));
            if (o == nullptr || !o->poly) throw ToolError("object not found or has no mesh");
            const PolyMesh& m = *o->poly;
            const bool box = a.has("min") && a.has("max");
            const bool sphere = a.has("center") && a.has("radius");
            if (!box && !sphere) throw ToolError("give min+max or center+radius");
            const Vec3 mn = a.vec3("min", Vec3{}), mx = a.vec3("max", Vec3{}), c = a.vec3("center", Vec3{});
            const float r = a.f("radius", 0.0f);
            std::vector<std::string> ids;
            size_t count = 0;
            for (size_t i = 0; i < m.verts.size(); ++i) {
                const Vec3& p = m.verts[i];
                bool hit = true;
                if (box) hit = p.x >= mn.x && p.y >= mn.y && p.z >= mn.z && p.x <= mx.x && p.y <= mx.y && p.z <= mx.z;
                if (hit && sphere) hit = length(p - c) <= r;
                if (!hit) continue;
                ++count;
                if (ids.size() < 5000) ids.push_back(std::to_string(i));
            }
            return okOut("found " + std::to_string(count) + " vertices",
                         JObj().add("count", std::to_string(count)).add("vertices", jlist(ids)).str());
        });

    add("measure", "Distance between the world-space centers of two objects.",
        {pInt("a", "First object id", true), pInt("b", "Second object id", true)}, false,
        [](Editor& e, const Args& a) {
            const Scene& s = e.scene();
            const Object* oa = s.find(a.integer("a"));
            const Object* ob = s.find(a.integer("b"));
            if (oa == nullptr || ob == nullptr) throw ToolError("object not found");
            Vec3 amn, amx, bmn, bmx;
            worldBounds(s, *oa, amn, amx);
            worldBounds(s, *ob, bmn, bmx);
            const Vec3 ca = (amn + amx) * 0.5f, cb = (bmn + bmx) * 0.5f;
            return okOut("distance " + jnum(length(ca - cb)),
                         JObj().add("distance", jnum(length(ca - cb))).add("delta", jvec(cb - ca)).str());
        });

    // ===== Criar objetos =====
    add("create_primitive",
        "Create a primitive with clean quad topology (good for subdivide): box, sphere (quad sphere), cylinder, cone, torus, plane. Units are meters, Y is up, the origin is the object's center. A size-1 box has 1 m edges.",
        withCreation({pEnum("type", "Primitive type", {"box", "sphere", "cylinder", "cone", "torus", "plane"}, true),
                      pNum("size", "Uniform size (diameter / edge), default 1"),
                      pVec3("dimensions", "Size per axis [x,y,z]; overrides size. Cylinder/cone: x,z = diameters, y = height"),
                      pInt("segments", "Detail: box/plane = cuts per edge, sphere = quads per cube edge, round shapes = segments around")}),
        true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            const std::string type = a.str("type");
            const float size = a.f("size", 1.0f);
            const Vec3 dims = a.vec3("dimensions", Vec3{size, size, size});
            if (!(dims.x > 0.0f && dims.y > 0.0f && dims.z > 0.0f)) throw ToolError("dimensions must be positive");
            const Object& o = createFrom(s, a, type == "cube" ? "box" : type, makePrimitivePoly(type, dims, a.integer("segments", 0)));
            return created(o);
        });

    add("create_lathe",
        "Revolve a profile around the Y axis (vases, wheels, tires, bottles, pistons). profile = [[radius, y], ...] from bottom to top; a radius of 0 closes the end at the axis.",
        withCreation({pArr("profile", "Profile points [radius, y], bottom to top", kVec2Items, true),
                      pInt("segments", "Segments around, default 24")}),
        true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            const Profile prof = readProfile(a.array("profile"), "profile");
            return created(createFrom(s, a, "lathe", makeLathe(prof, a.integer("segments", 24))));
        });

    add("create_loft",
        "Skin a surface through cross-sections. sections = list of sections, each a list of [x,y,z] points; all sections need the same point count. The classic way to model car bodies, hulls, fuselages. Combine with subdivide for smooth results.",
        withCreation({pArr("sections", "List of sections; each section is a list of [x,y,z] points", kSectionItems, true),
                      pBool("closed", "Each section is a closed ring, default true"),
                      pBool("cap_start", "Close the first section, default true"),
                      pBool("cap_end", "Close the last section, default true"),
                      pEnum("orient", "Normals: auto (default), keep, flip", {"auto", "keep", "flip"})}),
        true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            const Json& arr = a.array("sections");
            std::vector<std::vector<Vec3>> sections;
            size_t total = 0;
            for (const Json& sec : arr.a) {
                if (!sec.isArray()) throw ToolError("each section must be an array of points");
                sections.push_back(readPoints(sec, "section"));
                total += sections.back().size();
            }
            if (total > 60000) throw ToolError("loft is too big");
            const std::string orient = a.str("orient", "auto");
            const int o = orient == "keep" ? 1 : (orient == "flip" ? 2 : 0);
            return created(createFrom(s, a, "loft",
                                      makeLoft(sections, a.boolean("closed", true), a.boolean("cap_start", true),
                                               a.boolean("cap_end", true), o)));
        });

    add("create_body_loft",
        "Create a streamlined body (car, boat, plane, bottle) from stations along the X axis. Each station is a superellipse cross-section {x, width, height, center_y, center_z, power}. Use 6-12 stations, then subdivide. Width is along Z, height along Y; the nose is at the smallest x.",
        withCreation({pArr("stations", "Cross-sections along X", kStationItems, true),
                      pInt("ring_points", "Points per section, default 24 (6..128)"),
                      pBool("cap_ends", "Close both ends, default true")}),
        true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            struct Station {
                float x, w, h, cy, cz, p;
            };
            std::vector<Station> st;
            for (const Json& j : a.array("stations").a) {
                const Args sa{j};
                Station k;
                k.x = static_cast<float>(sa.num("x"));
                k.w = std::max(1e-4f, static_cast<float>(sa.num("width")));
                k.h = std::max(1e-4f, static_cast<float>(sa.num("height")));
                k.cy = sa.f("center_y", 0.0f);
                k.cz = sa.f("center_z", 0.0f);
                k.p = std::max(1.0f, std::min(sa.f("power", 2.0f), 10.0f));
                st.push_back(k);
            }
            if (st.size() < 2 || st.size() > 200) throw ToolError("need between 2 and 200 stations");
            std::sort(st.begin(), st.end(), [](const Station& l, const Station& r) { return l.x < r.x; });
            const int n = std::max(6, std::min(a.integer("ring_points", 24), 128));
            auto sgnpow = [](float v, float ex) { return (v < 0.0f ? -1.0f : 1.0f) * std::pow(std::fabs(v), ex); };
            std::vector<std::vector<Vec3>> sections;
            for (const Station& k : st) {
                std::vector<Vec3> ring;
                const float ex = 2.0f / k.p;
                for (int i = 0; i < n; ++i) {
                    const float ang = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(n);
                    ring.push_back({k.x, k.cy + k.h * 0.5f * sgnpow(std::sin(ang), ex), k.cz + k.w * 0.5f * sgnpow(std::cos(ang), ex)});
                }
                sections.push_back(std::move(ring));
            }
            const bool caps = a.boolean("cap_ends", true);
            return created(createFrom(s, a, "body", makeLoft(sections, true, caps, caps, 0)));
        });

    add("create_tube",
        "Create a pipe along a path (exhausts, cables, handrails, roll cages, tree branches). Optional taper with radius_end.",
        withCreation({pArr("path", "Path points [x,y,z]", kVec3Items, true), pNum("radius", "Radius at the start", true),
                      pNum("radius_end", "Radius at the end (taper), default same as radius"),
                      pInt("sides", "Sides around, default 12"), pBool("caps", "Close the ends, default true")}),
        true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            const std::vector<Vec3> path = readPoints(a.array("path"), "path");
            const float r0 = a.f("radius", 0.05f);
            const float r1 = a.f("radius_end", r0);
            if (!(r0 > 0.0f && r1 > 0.0f)) throw ToolError("radius must be positive");
            std::vector<float> radii;
            for (size_t i = 0; i < path.size(); ++i) {
                const float t = path.size() > 1 ? static_cast<float>(i) / static_cast<float>(path.size() - 1) : 0.0f;
                radii.push_back(r0 + (r1 - r0) * t);
            }
            return created(createFrom(s, a, "tube", makeTube(path, radii, a.integer("sides", 12), a.boolean("caps", true))));
        });

    add("create_prism",
        "Extrude a 2D polygon [[x,y],...] along Z, centered (panels, logos, windows, brackets, floor plans).",
        withCreation({pArr("polygon", "Polygon points [x,y]", kVec2Items, true), pNum("depth", "Thickness along Z", true)}),
        true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            const float depth = a.f("depth", 0.1f);
            if (!(depth > 0.0f)) throw ToolError("depth must be positive");
            return created(createFrom(s, a, "prism", makePrism(readProfile(a.array("polygon"), "polygon"), depth)));
        });

    add("create_mesh",
        "Create an object from raw data: vertices [[x,y,z],...] and faces [[i,j,k,...],...] (counter-clockwise seen from outside). Max 200k vertices.",
        withCreation({pArr("vertices", "Vertex positions", kVec3Items, true),
                      pArr("faces", "Faces as vertex index lists", R"J({"type":"array","items":{"type":"integer"}})J", true)}),
        true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            const Json& vj = a.array("vertices");
            const Json& fj = a.array("faces");
            if (vj.a.size() > 200000 || fj.a.size() > 200000) throw ToolError("mesh is too big");
            PolyMesh m;
            m.verts = readPoints(vj, "vertices");
            for (const Json& f : fj.a) {
                if (!f.isArray() || f.a.size() < 3) throw ToolError("each face needs at least 3 indices");
                std::vector<uint32_t> face;
                for (const Json& i : f.a) {
                    if (!i.isNumber() || i.n < 0 || i.n >= static_cast<double>(m.verts.size())) {
                        throw ToolError("face index out of range");
                    }
                    face.push_back(static_cast<uint32_t>(i.n));
                }
                m.faces.push_back(std::move(face));
            }
            return created(createFrom(s, a, "mesh", std::move(m)));
        });

    add("create_group", "Create an empty group (a pivot) to parent other objects to, e.g. one group per wheel or per car.",
        withCreation({}), true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            return created(createFrom(s, a, "group", PolyMesh()));
        });

    add("import_obj",
        "Create an object from Wavefront OBJ text (vertices 'v' and faces 'f').",
        withCreation({pStr("obj_text", "Contents of the .obj file", true), pNum("scale", "Scale factor, default 1")}),
        true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            const float scale = a.f("scale", 1.0f);
            if (!(scale > 0.0f)) throw ToolError("scale must be positive");
            return created(createFrom(s, a, "imported", parseObj(a.str("obj_text"), scale)));
        });

    // ===== Luzes, ambiente, câmera =====
    add("create_light",
        "Add a light. directional = sun (direction only), point = bulb, spot = headlight/spotlight. At most 8 enabled lights are rendered. Use target to aim it at a point.",
        {pEnum("type", "Light type", {"directional", "point", "spot"}, true), pVec3("position", "World position (point/spot)"),
         pVec3("direction", "Direction the light points (directional/spot)"), pVec3("target", "Point to aim at (overrides direction)"),
         pVec3("color", "Color [r,g,b], default white"), pNum("intensity", "Default: sun 3, point 30, spot 60"),
         pNum("range", "Reach of point/spot lights, default 20"), pNum("spot_angle_deg", "Full cone angle for spot, default 40"),
         pStr("name", "Light name")},
        true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            if (s.lights.size() >= 32) throw ToolError("too many lights");
            const std::string type = a.str("type");
            Light l;
            l.id = s.nextId++;
            l.type = type == "point" ? LightType::Point : (type == "spot" ? LightType::Spot : LightType::Directional);
            if (type != "point" && type != "spot" && type != "directional") throw ToolError("unknown light type");
            l.name = a.str("name", type + "_light_" + std::to_string(l.id));
            l.position = a.vec3("position", Vec3{0.0f, 3.0f, 0.0f});
            l.direction = a.has("target") ? normalize(a.vec3("target") - l.position)
                                          : normalize(a.vec3("direction", Vec3{-0.4f, -0.8f, -0.6f}));
            l.color = toColor(a.vec3("color", Vec3{1.0f, 1.0f, 1.0f}));
            l.intensity = std::max(0.0f, a.f("intensity", type == "directional" ? 3.0f : (type == "point" ? 30.0f : 60.0f)));
            l.range = std::max(0.1f, a.f("range", 20.0f));
            l.spotAngleDeg = std::max(1.0f, std::min(a.f("spot_angle_deg", 40.0f), 170.0f));
            s.lights.push_back(l);
            return okOut("Created light '" + l.name + "'", JObj().add("light_id", std::to_string(l.id)).str());
        });

    add("set_light", "Change a light. Only the given fields change.",
        {pInt("light_id", "Light id (see get_scene)", true), pVec3("position", "World position"), pVec3("direction", "Direction"),
         pVec3("target", "Point to aim at"), pVec3("color", "Color [r,g,b]"), pNum("intensity", "Intensity"),
         pNum("range", "Range"), pNum("spot_angle_deg", "Spot cone angle"), pBool("enabled", "On or off")},
        true, [](Editor& e, const Args& a) {
            Light* l = e.mutableScene().findLight(a.integer("light_id"));
            if (l == nullptr) throw ToolError("light not found");
            if (a.has("position")) l->position = a.vec3("position");
            if (a.has("direction")) l->direction = normalize(a.vec3("direction"));
            if (a.has("target")) l->direction = normalize(a.vec3("target") - l->position);
            if (a.has("color")) l->color = toColor(a.vec3("color"));
            if (a.has("intensity")) l->intensity = std::max(0.0f, a.f("intensity", 1.0f));
            if (a.has("range")) l->range = std::max(0.1f, a.f("range", 20.0f));
            if (a.has("spot_angle_deg")) l->spotAngleDeg = std::max(1.0f, std::min(a.f("spot_angle_deg", 40.0f), 170.0f));
            l->enabled = a.boolean("enabled", l->enabled);
            return okOut("Light updated");
        });

    add("delete_light", "Delete a light.", {pInt("light_id", "Light id", true)}, true, [](Editor& e, const Args& a) {
        Scene& s = e.mutableScene();
        const int id = a.integer("light_id");
        const auto it = std::remove_if(s.lights.begin(), s.lights.end(), [id](const Light& l) { return l.id == id; });
        if (it == s.lights.end()) throw ToolError("light not found");
        s.lights.erase(it, s.lights.end());
        return okOut("Light deleted");
    });

    add("set_environment",
        "Set the sky gradient (also reflected by metals and paint), ground color, ambient strength and exposure.",
        {pVec3("sky_top", "Sky color at the zenith"), pVec3("sky_horizon", "Sky color at the horizon"),
         pVec3("ground", "Ground color below the horizon"), pNum("ambient", "Ambient/reflection strength, default 0.6"),
         pNum("exposure", "Exposure multiplier, default 1")},
        true, [](Editor& e, const Args& a) {
            Environment& env = e.mutableScene().env;
            if (a.has("sky_top")) env.skyTop = toColor(a.vec3("sky_top"));
            if (a.has("sky_horizon")) env.skyHorizon = toColor(a.vec3("sky_horizon"));
            if (a.has("ground")) env.ground = toColor(a.vec3("ground"));
            if (a.has("ambient")) env.ambient = std::max(0.0f, std::min(a.f("ambient", 0.6f), 5.0f));
            if (a.has("exposure")) env.exposure = std::max(0.05f, std::min(a.f("exposure", 1.0f), 10.0f));
            return okOut("Environment updated");
        });

    add("set_camera",
        "Move the viewport camera (orbit around a target). view = a preset: front, back, left, right, top, bottom, iso.",
        {pEnum("view", "Preset view", {"front", "back", "left", "right", "top", "bottom", "iso"}),
         pNum("yaw_deg", "Horizontal angle in degrees"), pNum("pitch_deg", "Vertical angle in degrees (-89..89)"),
         pNum("distance", "Distance from the target"), pVec3("target", "Point to look at")},
        false, [](Editor& e, const Args& a) {
            CameraState& c = e.camera;
            const std::string view = a.str("view", "");
            if (view == "front") { c.yaw = 0.0f; c.pitch = 0.0f; }
            else if (view == "back") { c.yaw = kPi; c.pitch = 0.0f; }
            else if (view == "left") { c.yaw = kPi * 0.5f; c.pitch = 0.0f; }
            else if (view == "right") { c.yaw = -kPi * 0.5f; c.pitch = 0.0f; }
            else if (view == "top") { c.pitch = 1.5f; }
            else if (view == "bottom") { c.pitch = -1.5f; }
            else if (view == "iso") { c.yaw = 0.78f; c.pitch = 0.5f; }
            if (a.has("yaw_deg")) c.yaw = radians(a.f("yaw_deg", 0.0f));
            if (a.has("pitch_deg")) c.pitch = std::max(-1.55f, std::min(radians(a.f("pitch_deg", 0.0f)), 1.55f));
            if (a.has("distance")) c.distance = std::max(0.2f, std::min(a.f("distance", 8.0f), 400.0f));
            if (a.has("target")) c.target = a.vec3("target");
            return okOut("Camera updated");
        });

    add("frame_object", "Point the camera at an object and move back until it fits the view.",
        {pInt("object_id", "Object id", true)}, false, [](Editor& e, const Args& a) {
            const Scene& s = e.scene();
            const Object* o = s.find(a.integer("object_id"));
            if (o == nullptr) throw ToolError("object not found");
            Vec3 mn, mx;
            worldBounds(s, *o, mn, mx);
            const float radius = std::max(0.05f, length(mx - mn) * 0.5f);
            e.camera.target = (mn + mx) * 0.5f;
            e.camera.distance = std::max(0.3f, std::min(radius / std::tan(0.5236f) * 1.15f, 400.0f));
            return okOut("Framed '" + o->name + "'");
        });

    // ===== Objetos: organização =====
    add("delete_object", "Delete an object. Its children become top-level objects.",
        {pInt("object_id", "Object id", true)}, true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            const int id = obj(s, a).id;
            eraseObject(s, id);
            return okOut("Object deleted");
        });

    add("duplicate_object",
        "Duplicate an object count times (also works as a linear array). Copy i is offset by offset*i, rotated by rotation_step*i and scaled by scale_step^i. Copies share the mesh data, so it is cheap.",
        {pInt("object_id", "Object id", true), pInt("count", "Number of copies, default 1, max 200"),
         pVec3("offset", "Position step per copy, default [1,0,0]"), pVec3("rotation_step", "Rotation step per copy in degrees"),
         pVec3("scale_step", "Scale multiplier per copy, default [1,1,1]")},
        true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            const Object src = obj(s, a);
            const int count = std::max(1, std::min(a.integer("count", 1), 200));
            const Vec3 off = a.vec3("offset", Vec3{1.0f, 0.0f, 0.0f});
            const Vec3 rot = a.vec3("rotation_step", Vec3{});
            const Vec3 sc = a.vec3("scale_step", Vec3{1.0f, 1.0f, 1.0f});
            if (s.objects.size() + static_cast<size_t>(count) > 2000) throw ToolError("too many objects");
            std::vector<std::string> ids;
            for (int i = 1; i <= count; ++i) {
                Object c = src;
                c.id = s.nextId++;
                c.name = src.name + "_" + std::to_string(i);
                const float fi = static_cast<float>(i);
                c.transform.position = src.transform.position + off * fi;
                c.transform.rotationDeg = src.transform.rotationDeg + rot * fi;
                c.transform.scale = Vec3{src.transform.scale.x * std::pow(sc.x, fi), src.transform.scale.y * std::pow(sc.y, fi),
                                         src.transform.scale.z * std::pow(sc.z, fi)};
                ids.push_back(std::to_string(c.id));
                s.objects.push_back(std::move(c));
            }
            return okOut("Created " + std::to_string(count) + " copies", JObj().add("object_ids", jlist(ids)).str());
        });

    add("scatter_objects",
        "Scatter copies of an object at random positions inside a box (rocks, trees, bolts, crowd). Deterministic for a given seed.",
        {pInt("object_id", "Object id", true), pInt("count", "Number of copies, max 200", true), pVec3("min", "Box min corner", true),
         pVec3("max", "Box max corner", true), pInt("seed", "Random seed, default 1"), pBool("random_yaw", "Random rotation around Y, default true"),
         pNum("scale_min", "Minimum uniform scale factor, default 1"), pNum("scale_max", "Maximum uniform scale factor, default 1")},
        true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            const Object src = obj(s, a);
            const int count = std::max(1, std::min(a.integer("count"), 200));
            const Vec3 mn = a.vec3("min"), mx = a.vec3("max");
            unsigned state = static_cast<unsigned>(a.integer("seed", 1)) * 2654435761u + 12345u;
            auto rnd = [&state]() {
                state = state * 1664525u + 1013904223u;
                return static_cast<float>((state >> 8) & 0xffffffu) / static_cast<float>(0x1000000);
            };
            const float smin = a.f("scale_min", 1.0f), smax = a.f("scale_max", 1.0f);
            if (s.objects.size() + static_cast<size_t>(count) > 2000) throw ToolError("too many objects");
            std::vector<std::string> ids;
            for (int i = 0; i < count; ++i) {
                Object c = src;
                c.id = s.nextId++;
                c.name = src.name + "_s" + std::to_string(i + 1);
                c.transform.position = Vec3{mn.x + (mx.x - mn.x) * rnd(), mn.y + (mx.y - mn.y) * rnd(), mn.z + (mx.z - mn.z) * rnd()};
                if (a.boolean("random_yaw", true)) c.transform.rotationDeg.y = 360.0f * rnd();
                const float k = smin + (smax - smin) * rnd();
                c.transform.scale = src.transform.scale * k;
                ids.push_back(std::to_string(c.id));
                s.objects.push_back(std::move(c));
            }
            return okOut("Scattered " + std::to_string(count) + " copies", JObj().add("object_ids", jlist(ids)).str());
        });

    add("rename_object", "Rename an object.", {pInt("object_id", "Object id", true), pStr("name", "New name", true)}, true,
        [](Editor& e, const Args& a) {
            obj(e.mutableScene(), a).name = a.str("name");
            return okOut("Renamed");
        });

    add("set_visibility", "Show or hide an object.", {pInt("object_id", "Object id", true), pBool("visible", "Visible", true)}, true,
        [](Editor& e, const Args& a) {
            obj(e.mutableScene(), a).visible = a.boolean("visible", true);
            return okOut("Visibility updated");
        });

    add("set_parent",
        "Make an object a child of another (its transform becomes relative to the parent). parent_id = -1 removes the parent.",
        {pInt("object_id", "Child id", true), pInt("parent_id", "Parent id, or -1", true)}, true,
        [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            Object& o = obj(s, a);
            const int parent = a.integer("parent_id");
            if (parent >= 0) {
                if (s.find(parent) == nullptr) throw ToolError("parent not found");
                int cur = parent;
                for (int depth = 0; cur >= 0 && depth < 64; ++depth) {
                    if (cur == o.id) throw ToolError("that would create a parenting cycle");
                    const Object* p = s.find(cur);
                    cur = p ? p->parent : -1;
                }
            }
            o.parent = parent;
            return okOut("Parent updated");
        });

    add("select_object", "Select an object in the editor (-1 clears the selection).",
        {pInt("object_id", "Object id or -1", true)}, false, [](Editor& e, const Args& a) {
            e.select(a.integer("object_id"));
            return okOut("Selection updated");
        });

    // ===== Transformações =====
    add("set_transform", "Set the local transform of an object. Only the given fields change.",
        {pInt("object_id", "Object id", true), pVec3("position", "Position [x,y,z]"),
         pVec3("rotation_degrees", "Euler XYZ rotation in degrees"), pVec3("scale", "Scale [sx,sy,sz], each 0.001..1000")},
        true, [](Editor& e, const Args& a) {
            Object& o = obj(e.mutableScene(), a);
            if (a.has("position")) o.transform.position = a.vec3("position");
            if (a.has("rotation_degrees")) o.transform.rotationDeg = a.vec3("rotation_degrees");
            if (a.has("scale")) {
                const Vec3 sc = a.vec3("scale");
                o.transform.scale = Vec3{std::max(0.001f, std::min(sc.x, 1000.0f)), std::max(0.001f, std::min(sc.y, 1000.0f)),
                                         std::max(0.001f, std::min(sc.z, 1000.0f))};
            }
            return okOut("Transform updated");
        });

    add("move_by", "Move an object by an offset.", {pInt("object_id", "Object id", true), pVec3("offset", "[dx,dy,dz]", true)}, true,
        [](Editor& e, const Args& a) {
            obj(e.mutableScene(), a).transform.position += a.vec3("offset");
            return okOut("Moved");
        });

    add("rotate_by", "Rotate an object by Euler angles in degrees.", {pInt("object_id", "Object id", true), pVec3("degrees", "[rx,ry,rz]", true)}, true,
        [](Editor& e, const Args& a) {
            obj(e.mutableScene(), a).transform.rotationDeg += a.vec3("degrees");
            return okOut("Rotated");
        });

    add("scale_by", "Multiply an object's scale per axis.", {pInt("object_id", "Object id", true), pVec3("factor", "[fx,fy,fz]", true)}, true,
        [](Editor& e, const Args& a) {
            Object& o = obj(e.mutableScene(), a);
            const Vec3 f = a.vec3("factor");
            const Vec3 n{o.transform.scale.x * f.x, o.transform.scale.y * f.y, o.transform.scale.z * f.z};
            o.transform.scale = Vec3{std::max(0.001f, std::min(n.x, 1000.0f)), std::max(0.001f, std::min(n.y, 1000.0f)),
                                     std::max(0.001f, std::min(n.z, 1000.0f))};
            return okOut("Scaled");
        });

    add("apply_transform",
        "Bake the object's transform into its mesh (position/rotation/scale become identity). Needed before mesh operations that assume a unit scale. Fails if the object has children.",
        {pInt("object_id", "Object id", true)}, true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            Object& o = obj(s, a);
            detachChildrenCheck(s, o.id);
            PolyMesh m = transformed(polyOf(o), o.transform.matrix());
            if (o.transform.scale.x * o.transform.scale.y * o.transform.scale.z < 0.0f) m = flipFaces(m);
            setPoly(o, std::move(m));
            o.transform = Transform();
            return okOut("Transform applied");
        });

    add("place_on_ground", "Move a top-level object up or down so its lowest point touches y = 0.",
        {pInt("object_id", "Object id", true)}, true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            Object& o = obj(s, a);
            if (o.parent >= 0) throw ToolError("object has a parent");
            Vec3 mn, mx;
            worldBounds(s, o, mn, mx);
            o.transform.position.y -= mn.y;
            return okOut("Placed on the ground");
        });

    add("center_origin",
        "Move the mesh so its bounding-box center is at the object's origin (the object does not move visually). Useful for rotating around the middle. Fails if the object has children.",
        {pInt("object_id", "Object id", true)}, true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            Object& o = obj(s, a);
            detachChildrenCheck(s, o.id);
            Vec3 mn, mx;
            objectBounds(o, mn, mx);
            const Vec3 c = (mn + mx) * 0.5f;
            PolyMesh m = polyOf(o);
            for (Vec3& v : m.verts) v = v - c;
            o.transform.position += transformDir(o.transform.matrix(), c);
            setPoly(o, std::move(m));
            return okOut("Origin centered");
        });

    add("align_objects",
        "Align top-level objects along an axis in world space to the first object of the list (or to value). mode: min, center or max of their bounding boxes.",
        {pInts("object_ids", "Objects to align; the first is the reference", true), pEnum("axis", "x, y or z", {"x", "y", "z"}, true),
         pEnum("mode", "min, center or max", {"min", "center", "max"}), pNum("value", "Align to this coordinate instead of the first object")},
        true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            const std::vector<int> ids = a.ints("object_ids");
            if (ids.empty()) throw ToolError("no objects");
            const int axis = axisOf(a.str("axis"));
            const std::string mode = a.str("mode", "center");
            auto coordOf = [&](const Object& o) {
                Vec3 mn, mx;
                worldBounds(s, o, mn, mx);
                return mode == "min" ? axisVal(mn, axis) : (mode == "max" ? axisVal(mx, axis) : (axisVal(mn, axis) + axisVal(mx, axis)) * 0.5f);
            };
            const float target = a.has("value") ? a.f("value", 0.0f) : coordOf(getObject(s, ids[0]));
            for (int id : ids) {
                Object& o = getObject(s, id);
                if (o.parent >= 0) throw ToolError("object " + std::to_string(id) + " has a parent");
                axisRef(o.transform.position, axis) += target - coordOf(o);
            }
            return okOut("Aligned " + std::to_string(ids.size()) + " objects");
        });

    add("mirror_object",
        "Create a mirrored copy of an object across its local plane through its origin (left/right parts, wheels, mirrors). axis is the axis to flip. The copy has the same transform.",
        {pInt("object_id", "Object id", true), pEnum("axis", "x, y or z", {"x", "y", "z"}, true)}, true,
        [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            Object c = obj(s, a);
            const std::string srcName = c.name;
            const int axis = axisOf(a.str("axis"));
            c.id = s.nextId++;
            c.name = srcName + "_mirror";
            setPoly(c, reflectMesh(polyOf(c), axis));
            const int id = c.id;
            s.objects.push_back(std::move(c));
            return okOut("Created mirrored copy", idData(id));
        });

    add("join_objects",
        "Merge several objects into one mesh (world transforms are baked; the result has an identity transform). The originals are deleted.",
        {pInts("object_ids", "Objects to join (at least 2)", true), pStr("name", "Name of the result")}, true,
        [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            const std::vector<int> ids = a.ints("object_ids");
            if (ids.size() < 2) throw ToolError("need at least 2 objects");
            PolyMesh joined;
            Material mat;
            bool first = true;
            for (int id : ids) {
                const Object& o = getObject(s, id);
                PolyMesh part = transformed(polyOf(o), worldMatrix(s, o));
                if (first) {
                    joined = std::move(part);
                    mat = o.material;
                    first = false;
                } else {
                    joined = mergeMeshes(joined, part);
                }
            }
            for (int id : ids) eraseObject(s, id);
            Object& n = addObjectTo(s, "joined", std::move(joined), a.str("name", ""), Vec3{});
            n.material = mat;
            return created(n);
        });

    add("boolean",
        "Boolean between two closed objects: union, difference (a minus b) or intersect. Use it for wheel arches, windows, holes, panel cuts. Operands should be closed meshes with outward normals and under ~20000 triangles in total, so do it BEFORE subdividing. The result is a new object.",
        {pInt("a", "First object id", true), pInt("b", "Second object id (the cutter for difference)", true),
         pEnum("operation", "union, difference or intersect", {"union", "difference", "intersect"}, true),
         pBool("keep_inputs", "Keep the original objects, default false"), pStr("name", "Name of the result")},
        true, [](Editor& e, const Args& a) {
            Scene& s = e.mutableScene();
            const int ia = a.integer("a"), ib = a.integer("b");
            if (ia == ib) throw ToolError("a and b must be different objects");
            const Object& oa = getObject(s, ia);
            const Object& ob = getObject(s, ib);
            const std::string op = a.str("operation");
            const int code = op == "union" ? 0 : (op == "difference" ? 1 : (op == "intersect" ? 2 : -1));
            if (code < 0) throw ToolError("operation must be union, difference or intersect");
            const PolyMesh pa = transformed(polyOf(oa), worldMatrix(s, oa));
            const PolyMesh pb = transformed(polyOf(ob), worldMatrix(s, ob));
            PolyMesh result = booleanOp(pa, pb, code);
            const Material mat = oa.material;
            if (!a.boolean("keep_inputs", false)) {
                eraseObject(s, ia);
                eraseObject(s, ib);
            }
            Object& n = addObjectTo(s, "boolean", std::move(result), a.str("name", ""), Vec3{});
            n.material = mat;
            return created(n);
        });

    // ===== Materiais e sombreamento =====
    add("set_material",
        "Set PBR material fields (only the given ones change). clearcoat adds a glossy lacquer layer (car paint). opacity < 1 makes it transparent (glass).",
        {pInt("object_id", "Object id", true), pVec3("base_color", "Albedo [r,g,b] 0..1"), pNum("metallic", "0..1"),
         pNum("roughness", "0..1 (0 = mirror)"), pNum("clearcoat", "0..1"), pVec3("emissive", "Emitted color [r,g,b]"),
         pNum("emissive_strength", "Glow multiplier"), pNum("opacity", "0..1")},
        true, [](Editor& e, const Args& a) {
            Material& m = obj(e.mutableScene(), a).material;
            if (a.has("base_color")) m.base = toColor(a.vec3("base_color"));
            if (a.has("metallic")) m.metallic = clamp01(a.f("metallic", 0.0f));
            if (a.has("roughness")) m.roughness = clamp01(a.f("roughness", 0.5f));
            if (a.has("clearcoat")) m.clearcoat = clamp01(a.f("clearcoat", 0.0f));
            if (a.has("emissive")) m.emissive = toColor(a.vec3("emissive"));
            if (a.has("emissive_strength")) m.emissiveStrength = std::max(0.0f, std::min(a.f("emissive_strength", 0.0f), 50.0f));
            if (a.has("opacity")) m.opacity = clamp01(a.f("opacity", 1.0f));
            return okOut("Material updated");
        });

    add("apply_material_preset",
        "Apply a ready-made material: car_paint, chrome, glass, rubber, plastic, aluminum, steel, gold, copper, wood, concrete, leather, emissive, headlight. color overrides the tint of car_paint, glass, plastic, wood, leather and emissive.",
        {pInt("object_id", "Object id", true),
         pEnum("preset", "Preset name", {"car_paint", "chrome", "glass", "rubber", "plastic", "aluminum", "steel", "gold", "copper", "wood", "concrete", "leather", "emissive", "headlight"}, true),
         pVec3("color", "Optional tint [r,g,b]")},
        true, [](Editor& e, const Args& a) {
            Material& m = obj(e.mutableScene(), a).material;
            applyPreset(m, a.str("preset"), a.has("color"), toColor(a.vec3("color", Vec3{})));
            return okOut("Preset applied");
        });

    add("set_shading",
        "Choose smooth or flat shading. auto smooths edges below angle_deg (default 45) and keeps hard edges sharp, which suits most hard-surface models.",
        {pInt("object_id", "Object id", true), pEnum("mode", "smooth, flat or auto", {"smooth", "flat", "auto"}, true),
         pNum("angle_deg", "Angle for auto mode, default 45"), pBool("double_sided", "Render both faces (for open surfaces)")},
        true, [](Editor& e, const Args& a) {
            Object& o = obj(e.mutableScene(), a);
            const std::string mode = a.str("mode");
            if (mode == "smooth") o.smoothAngle = 180.0f;
            else if (mode == "flat") o.smoothAngle = 0.0f;
            else if (mode == "auto") o.smoothAngle = std::max(1.0f, std::min(a.f("angle_deg", 45.0f), 180.0f));
            else throw ToolError("mode must be smooth, flat or auto");
            o.doubleSided = a.boolean("double_sided", o.doubleSided);
            refreshRender(o);
            return okOut("Shading updated");
        });

    // ===== Edição de malha =====
    addMesh("subdivide",
        "Catmull-Clark subdivision: smooths and rounds the mesh, multiplying the faces by 4 per level. Hard edges set with set_creases/crease_by_angle stay sharp. Model a coarse cage first, then use 2-3 levels for a perfect rounded surface.",
        {pInt("levels", "1..4, default 1")}, [](const PolyMesh& m, const Args& a) {
            const int levels = std::max(1, std::min(a.integer("levels", 1), 4));
            if (estimateSubdividedFaces(m, levels) > 600000) throw ToolError("result would have too many faces; use fewer levels");
            return catmullClark(m, levels);
        });

    addMesh("extrude_faces",
        "Extrude faces (as one region) along their normals or a direction, creating side walls. Negative distance cuts inward. scale < 1 tapers the top. Face indices stay valid for the extruded top faces.",
        {pInts("faces", "Face indices"), pBool("all_faces", "Use every face"), pNum("distance", "Extrusion distance", true),
         pVec3("direction", "Fixed direction instead of normals"), pNum("scale", "Scale of the extruded region, default 1")},
        [](const PolyMesh& m, const Args& a) {
            return extrudeFaces(m, faceSelection(a, m), a.f("distance", 0.0f), a.has("direction"),
                                a.vec3("direction", Vec3{0.0f, 1.0f, 0.0f}), a.f("scale", 1.0f));
        });

    addMesh("inset_faces",
        "Inset each selected face (a smaller face inside, with a ring of quads). Typical use: inset then extrude with negative distance for windows, vents, panels.",
        {pInts("faces", "Face indices"), pBool("all_faces", "Use every face"), pNum("amount", "Fraction toward the face center, 0..0.95, default 0.2")},
        [](const PolyMesh& m, const Args& a) { return insetFaces(m, faceSelection(a, m), a.f("amount", 0.2f)); });

    addMesh("loop_cut",
        "Insert edge loops across a ring of quads, starting from the edge between vertices edge=[v0,v1]. Adds detail and support loops where you need them. Face/vertex indices change afterwards.",
        {pArr("edge", "Two vertex indices of a quad edge", "{\"type\":\"integer\"}", true), pInt("cuts", "Number of loops, default 1, max 32")},
        [](const PolyMesh& m, const Args& a) {
            const std::vector<int> e = a.ints("edge");
            if (e.size() != 2) throw ToolError("edge needs exactly 2 vertex indices");
            for (int v : e) {
                if (v < 0 || static_cast<size_t>(v) >= m.verts.size()) throw ToolError("vertex index out of range");
            }
            return loopCut(m, static_cast<uint32_t>(e[0]), static_cast<uint32_t>(e[1]), a.integer("cuts", 1));
        });

    addMesh("mirror_mesh",
        "Mirror the mesh across the plane through the object's origin and weld the seam. Model half of a symmetric object (car, face, plane) and mirror it. Vertices on the plane are merged.",
        {pEnum("axis", "Axis to flip: x, y or z", {"x", "y", "z"}, true), pNum("merge_distance", "Weld distance, default 0.0001")},
        [](const PolyMesh& m, const Args& a) { return mirrorMesh(m, axisOf(a.str("axis")), a.f("merge_distance", 1e-4f)); });

    addMesh("solidify",
        "Give an open surface thickness (panels, shells, glass, sheet metal). Positive thickness grows inward.",
        {pNum("thickness", "Wall thickness", true)},
        [](const PolyMesh& m, const Args& a) { return solidify(m, a.f("thickness", 0.05f)); });

    addMesh("smooth_mesh", "Relax vertex positions (Laplacian) to even out the surface. Border and creased vertices stay fixed.",
        {pInt("iterations", "1..50, default 1"), pNum("factor", "0..1, default 0.5")},
        [](const PolyMesh& m, const Args& a) { return smoothMesh(m, a.integer("iterations", 1), a.f("factor", 0.5f)); });

    addMesh("weld_vertices", "Merge vertices closer than a distance (closes cracks, removes duplicates).",
        {pNum("distance", "Weld distance, default 0.0001")},
        [](const PolyMesh& m, const Args& a) { return weldVertices(m, a.f("distance", 1e-4f)); });

    addMesh("flip_normals", "Reverse the winding of every face (turns the mesh inside out).", {},
            [](const PolyMesh& m, const Args&) { return flipFaces(m); });

    addMesh("recalc_normals", "Make closed meshes face outward: flips all faces when the volume is negative.", {},
            [](const PolyMesh& m, const Args&) { return signedVolume(m) < 0.0f ? flipFaces(m) : m; });

    addMesh("triangulate", "Convert all faces to triangles.", {}, [](const PolyMesh& m, const Args&) { return triangulate(m); });

    addMesh("set_creases",
        "Mark (or unmark) hard edges for subdivision, given as vertex-index pairs. Creased edges stay sharp when subdivided: use them for panel lines, shut lines and sharp trims.",
        {pArr("edges", "Edges as [v0,v1] pairs", kIntPairItems), pBool("enable", "true to crease (default), false to remove"),
         pBool("clear_all", "Remove every crease")},
        [](const PolyMesh& m, const Args& a) {
            std::vector<std::pair<uint32_t, uint32_t>> edges;
            if (!a.boolean("clear_all", false)) {
                for (const Json& pr : a.array("edges").a) {
                    if (!pr.isArray() || pr.a.size() != 2 || !pr.a[0].isNumber() || !pr.a[1].isNumber()) {
                        throw ToolError("edges must be [v0,v1] pairs");
                    }
                    const double u = pr.a[0].n, v = pr.a[1].n;
                    if (u < 0 || v < 0 || u >= static_cast<double>(m.verts.size()) || v >= static_cast<double>(m.verts.size())) {
                        throw ToolError("vertex index out of range");
                    }
                    edges.push_back({static_cast<uint32_t>(u), static_cast<uint32_t>(v)});
                }
            }
            return setCreases(m, edges, a.boolean("enable", true), a.boolean("clear_all", false));
        });

    addMesh("crease_by_angle",
        "Crease every edge whose two faces meet at more than angle_deg. Run this on a hard-surface cage before subdividing so corners stay sharp while curved areas round off.",
        {pNum("angle_deg", "Threshold angle, default 40")},
        [](const PolyMesh& m, const Args& a) { return creaseByAngle(m, a.f("angle_deg", 40.0f)); });

    addMesh("deform",
        "Deform all vertices along an axis. bend (amount = degrees over the full length), twist (degrees over the length), taper (amount = end scale factor), stretch (amount = factor), shear, spherify (amount 0..1), inflate (amount = push along normals), noise (amount = height, frequency, seed). Topology is unchanged.",
        {pEnum("type", "Deformer", {"bend", "twist", "taper", "stretch", "shear", "spherify", "inflate", "noise"}, true),
         pEnum("axis", "Axis x, y or z (default y)", {"x", "y", "z"}), pNum("amount", "Strength, depends on type", true),
         pNum("frequency", "Noise frequency, default 1"), pInt("seed", "Noise seed, default 1")},
        [](const PolyMesh& m, const Args& a) { return deformMesh(m, a); });

    addMesh("sculpt_move",
        "Soft-move the vertices near a point (proportional editing): like a sculpting grab brush. All in the object's local space.",
        {pVec3("center", "Brush center", true), pNum("radius", "Brush radius", true), pVec3("offset", "Displacement", true),
         pEnum("falloff", "smooth (default), linear or constant", {"smooth", "linear", "constant"})},
        [](const PolyMesh& in, const Args& a) {
            PolyMesh m = in;
            const Vec3 c = a.vec3("center"), off = a.vec3("offset");
            const float r = a.f("radius", 1.0f);
            if (!(r > 0.0f)) throw ToolError("radius must be positive");
            const std::string falloff = a.str("falloff", "smooth");
            for (Vec3& p : m.verts) {
                const float d = length(p - c);
                if (d >= r) continue;
                const float t = d / r;
                const float w = falloff == "constant" ? 1.0f : (falloff == "linear" ? 1.0f - t : (1.0f - t * t) * (1.0f - t * t));
                p += off * w;
            }
            return m;
        });

    addMesh("transform_vertices",
        "Move, rotate and scale a set of vertices (by index) around a pivot (default: their centroid). Use find_vertices to get indices.",
        {pInts("vertices", "Vertex indices", true), pVec3("translate", "Offset"), pVec3("rotate_degrees", "Euler XYZ degrees"),
         pVec3("scale", "Scale per axis"), pVec3("pivot", "Pivot, default centroid")},
        [](const PolyMesh& in, const Args& a) {
            PolyMesh m = in;
            const std::vector<int> idx = a.ints("vertices");
            if (idx.empty()) throw ToolError("no vertices selected");
            Vec3 pivot;
            for (int i : idx) {
                if (i < 0 || static_cast<size_t>(i) >= m.verts.size()) throw ToolError("vertex index out of range");
                pivot += m.verts[static_cast<size_t>(i)];
            }
            pivot = a.has("pivot") ? a.vec3("pivot") : pivot / static_cast<float>(idx.size());
            const Vec3 tr = a.vec3("translate", Vec3{});
            const Vec3 sc = a.vec3("scale", Vec3{1.0f, 1.0f, 1.0f});
            const Vec3 rot = a.vec3("rotate_degrees", Vec3{});
            const Mat4 R = multiply(rotateZ(radians(rot.z)), multiply(rotateY(radians(rot.y)), rotateX(radians(rot.x))));
            for (int i : idx) {
                Vec3& v = m.verts[static_cast<size_t>(i)];
                Vec3 p = v - pivot;
                p = Vec3{p.x * sc.x, p.y * sc.y, p.z * sc.z};
                v = pivot + transformDir(R, p) + tr;
            }
            return m;
        });

    // ===== Física =====
    add("set_physics",
        "Give an object a physics body. dynamic bodies fall and collide; static bodies (dynamic=false) are fixed colliders. The ground is the plane y = 0.",
        {pInt("object_id", "Object id", true), pBool("enabled", "Enable physics, default true"), pBool("dynamic", "Dynamic (default true) or static"),
         pNum("mass", "Mass in kg, default 1"), pNum("restitution", "Bounciness 0..1, default 0.2"), pNum("friction", "0..1, default 0.5")},
        true, [](Editor& e, const Args& a) {
            PhysicsBody& p = obj(e.mutableScene(), a).physics;
            p.enabled = a.boolean("enabled", true);
            p.dynamic = a.boolean("dynamic", p.dynamic);
            p.mass = std::max(0.001f, a.f("mass", p.mass));
            p.restitution = clamp01(a.f("restitution", p.restitution));
            p.friction = clamp01(a.f("friction", p.friction));
            return okOut("Physics updated");
        });

    add("simulate_physics",
        "Run a rigid-body simulation (gravity, ground, box collisions, no rotation) and bake the result into the objects' positions. Approximates each body by its bounding box. Undoable.",
        {pNum("seconds", "Simulated time, 0.05..10, default 2"), pNum("gravity", "Gravity in m/s^2, default 9.81")}, true,
        [](Editor& e, const Args& a) { return simulatePhysics(e.mutableScene(), a); });

    // ===== Histórico =====
    add("undo", "Undo the last scene change.", {}, false, [](Editor& e, const Args&) {
        if (!e.undo()) throw ToolError("nothing to undo");
        return okOut("Undone");
    });

    add("redo", "Redo the last undone change.", {}, false, [](Editor& e, const Args&) {
        if (!e.redo()) throw ToolError("nothing to redo");
        return okOut("Redone");
    });

    add("save_checkpoint",
        "Save the whole scene under a name, so you can experiment and come back with restore_checkpoint. Max 20 checkpoints.",
        {pStr("name", "Checkpoint name", true)}, false, [](Editor& e, const Args& a) {
            const std::string name = a.str("name");
            if (e.checkpoints.size() >= 20 && e.checkpoints.count(name) == 0) throw ToolError("too many checkpoints");
            e.checkpoints[name] = e.scene();
            return okOut("Checkpoint '" + name + "' saved");
        });

    add("restore_checkpoint", "Restore a saved checkpoint (undoable).", {pStr("name", "Checkpoint name", true)}, true,
        [](Editor& e, const Args& a) {
            const std::string name = a.str("name");
            const auto it = e.checkpoints.find(name);
            if (it == e.checkpoints.end()) {
                std::string names;
                for (const auto& kv : e.checkpoints) names += (names.empty() ? "" : ", ") + kv.first;
                throw ToolError("no checkpoint named '" + name + "'. Available: " + (names.empty() ? "none" : names));
            }
            Scene restored = it->second;
            e.mutableScene() = std::move(restored);
            return okOut("Checkpoint '" + name + "' restored");
        });

    return t;
}

const std::vector<Tool>& tools() {
    static const std::vector<Tool> registry = buildTools();
    return registry;
}

}  // namespace

std::string toolDefinitionsJson() {
    std::string out = "[";
    bool first = true;
    for (const Tool& tool : tools()) {
        if (!first) out += ",";
        first = false;
        std::string props, required;
        for (const Param& p : tool.params) {
            if (!props.empty()) props += ",";
            props += jsonQuote(p.name) + ":" + p.schema;
            if (p.required) {
                if (!required.empty()) required += ",";
                required += jsonQuote(p.name);
            }
        }
        JObj params;
        params.add("type", jsonQuote("object")).add("properties", "{" + props + "}").add("required", "[" + required + "]");
        JObj def;
        def.add("name", jsonQuote(tool.name)).add("description", jsonQuote(tool.description)).add("parameters", params.str());
        out += def.str();
    }
    return out + "]";
}

std::string callTool(Editor& editor, const std::string& name, const std::string& argsJson) {
    ToolOutput out;
    try {
        const Tool* tool = nullptr;
        for (const Tool& candidate : tools()) {
            if (candidate.name == name) {
                tool = &candidate;
                break;
            }
        }
        if (tool == nullptr) throw ToolError("unknown tool: " + name);

        Json args = argsJson.empty() ? Json() : Json::parse(argsJson);
        if (args.type == Json::Type::Null) args.type = Json::Type::Object;
        if (args.type != Json::Type::Object) throw ToolError("arguments must be a JSON object");
        const Args a{args};
        for (const Param& p : tool->params) {
            if (p.required && !a.has(p.name.c_str())) throw ToolError("missing required argument: " + p.name);
        }
        if (tool->mutating) {
            editor.transact([&](Scene&) {
                out = tool->fn(editor, a);
                return out.ok;
            });
        } else {
            out = tool->fn(editor, a);
        }
    } catch (const std::exception& e) {
        out = ToolOutput();
        out.ok = false;
        out.message = e.what();
    }
    JObj r;
    r.add("ok", jbool(out.ok)).add("message", jsonQuote(out.message));
    if (!out.data.empty()) r.add("data", out.data);
    return r.str();
}

}  // namespace core
