#include <GLES3/gl3.h>
#include <android/log.h>
#include <jni.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/editor.h"
#include "core/tools.h"

using namespace core;

#define LOG_TAG "aimodeling"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

constexpr float kFovY = 1.0472f;  // 60 graus
constexpr int kGridHalf = 10;
constexpr int kMaxLights = 8;

const char* kObjectVertexShader = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMvp;
uniform mat4 uModel;
uniform mat3 uNormalMat;
out vec3 vWorldPos;
out vec3 vNormal;
void main() {
    vWorldPos = (uModel * vec4(aPos, 1.0)).xyz;
    vNormal = uNormalMat * aNormal;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

// PBR (GGX) com verniz, luzes direcionais/pontuais/foco e um céu em degradé como ambiente.
const char* kObjectFragmentShader = R"(#version 300 es
precision highp float;
in vec3 vWorldPos;
in vec3 vNormal;
out vec4 outColor;

uniform vec3 uBase;
uniform float uMetallic;
uniform float uRoughness;
uniform float uClearcoat;
uniform vec3 uEmissive;
uniform float uOpacity;
uniform float uSelected;
uniform vec3 uCamPos;
uniform float uExposure;
uniform float uAmbient;
uniform vec3 uSkyTop;
uniform vec3 uSkyHorizon;
uniform vec3 uGround;
uniform int uLightCount;
uniform vec4 uLightPosType[8];
uniform vec4 uLightColorRange[8];
uniform vec4 uLightSpot[8];

const float PI = 3.14159265;

vec3 envColor(vec3 d) {
    float t = clamp(d.y, -1.0, 1.0);
    if (t >= 0.0) return mix(uSkyHorizon, uSkyTop, pow(t, 0.6));
    return mix(uSkyHorizon, uGround, pow(-t, 0.4));
}

float distributionGGX(float NoH, float a) {
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + 1e-6);
}

float geometrySmith(float NoV, float NoL, float rough) {
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;
    float gv = NoV / (NoV * (1.0 - k) + k);
    float gl = NoL / (NoL * (1.0 - k) + k);
    return gv * gl;
}

vec3 fresnelSchlick(float cosT, vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - cosT, 5.0);
}

void main() {
    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(uCamPos - vWorldPos);
    float NoV = max(dot(N, V), 1e-3);
    float rough = clamp(uRoughness, 0.04, 1.0);
    float a = rough * rough;
    vec3 f0 = mix(vec3(0.04), uBase, uMetallic);
    vec3 diffuseColor = uBase * (1.0 - uMetallic);
    float ccRough = 0.06;
    float ccA = ccRough * ccRough;

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 8; i++) {
        if (i >= uLightCount) break;
        vec4 lp = uLightPosType[i];
        vec3 L;
        float att = 1.0;
        if (lp.w < 0.5) {
            L = normalize(lp.xyz);
        } else {
            vec3 toL = lp.xyz - vWorldPos;
            float dist = length(toL);
            L = toL / max(dist, 1e-4);
            float range = max(uLightColorRange[i].a, 0.1);
            float win = clamp(1.0 - pow(dist / range, 4.0), 0.0, 1.0);
            att = win * win / (dist * dist + 1.0);
            if (lp.w > 1.5) {
                float c = dot(-L, normalize(uLightSpot[i].xyz));
                att *= smoothstep(uLightSpot[i].w, uLightSpot[i].w + 0.08, c);
            }
        }
        float NoL = max(dot(N, L), 0.0);
        if (NoL <= 0.0) continue;
        vec3 H = normalize(V + L);
        float NoH = max(dot(N, H), 0.0);
        float VoH = max(dot(V, H), 0.0);
        vec3 F = fresnelSchlick(VoH, f0);
        float D = distributionGGX(NoH, a);
        float G = geometrySmith(NoV, NoL, rough);
        vec3 spec = D * G * F / (4.0 * NoV * NoL + 1e-4);
        vec3 kd = (1.0 - F) * diffuseColor / PI;
        vec3 layer = (kd + spec) * NoL;
        if (uClearcoat > 0.0) {
            float Dc = distributionGGX(NoH, ccA);
            float Gc = geometrySmith(NoV, NoL, ccRough);
            float Fc = 0.04 + 0.96 * pow(1.0 - VoH, 5.0);
            float specC = Dc * Gc * Fc / (4.0 * NoV * NoL + 1e-4);
            layer = layer * (1.0 - Fc * uClearcoat) + vec3(specC * NoL * uClearcoat);
        }
        Lo += layer * uLightColorRange[i].rgb * att;
    }

    vec3 R = reflect(-V, N);
    float fr = pow(1.0 - NoV, 5.0);
    vec3 Fenv = f0 + (max(vec3(1.0 - rough), f0) - f0) * fr;
    vec3 envDiffuse = envColor(N) * uAmbient;
    vec3 envSpec = mix(envColor(R), envColor(N), rough * rough) * uAmbient;
    vec3 ambient = diffuseColor * (1.0 - Fenv) * envDiffuse + Fenv * envSpec;
    if (uClearcoat > 0.0) {
        float Fc = 0.04 + 0.96 * fr;
        ambient = ambient * (1.0 - Fc * uClearcoat) + envColor(R) * uAmbient * Fc * uClearcoat;
    }

    vec3 color = Lo + ambient + uEmissive;
    color = mix(color, vec3(1.0, 0.6, 0.1), 0.3 * uSelected);
    color *= uExposure;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    outColor = vec4(color, uOpacity);
}
)";

const char* kLineVertexShader = R"(#version 300 es
layout(location = 0) in vec3 aPos;
uniform mat4 uMvp;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kLineFragmentShader = R"(#version 300 es
precision mediump float;
uniform vec3 uColor;
out vec4 outColor;
void main() {
    outColor = vec4(uColor, 1.0);
}
)";

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        LOGE("Erro no shader: %s", log);
    }
    return shader;
}

GLuint linkProgram(const char* vsSource, const char* fsSource) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

struct ObjectProgram {
    GLuint id = 0;
    GLint mvp = -1, model = -1, normalMat = -1, base = -1, metallic = -1, roughness = -1, clearcoat = -1;
    GLint emissive = -1, opacity = -1, selected = -1, camPos = -1, exposure = -1, ambient = -1;
    GLint skyTop = -1, skyHorizon = -1, ground = -1, lightCount = -1;
    GLint lightPosType = -1, lightColorRange = -1, lightSpot = -1;
};

void fetchUniforms(ObjectProgram& p) {
    auto u = [&p](const char* name) { return glGetUniformLocation(p.id, name); };
    p.mvp = u("uMvp");
    p.model = u("uModel");
    p.normalMat = u("uNormalMat");
    p.base = u("uBase");
    p.metallic = u("uMetallic");
    p.roughness = u("uRoughness");
    p.clearcoat = u("uClearcoat");
    p.emissive = u("uEmissive");
    p.opacity = u("uOpacity");
    p.selected = u("uSelected");
    p.camPos = u("uCamPos");
    p.exposure = u("uExposure");
    p.ambient = u("uAmbient");
    p.skyTop = u("uSkyTop");
    p.skyHorizon = u("uSkyHorizon");
    p.ground = u("uGround");
    p.lightCount = u("uLightCount");
    p.lightPosType = u("uLightPosType");
    p.lightColorRange = u("uLightColorRange");
    p.lightSpot = u("uLightSpot");
}

struct GpuMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ibo = 0;
    GLsizei count = 0;
    int version = -1;
};

GpuMesh upload(const Mesh& mesh, int version) {
    GpuMesh g;
    g.count = static_cast<GLsizei>(mesh.indices.size());
    g.version = version;

    glGenVertexArrays(1, &g.vao);
    glBindVertexArray(g.vao);

    glGenBuffers(1, &g.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)),
                 mesh.vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &g.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint32_t)),
                 mesh.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, px)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, nx)));

    glBindVertexArray(0);
    return g;
}

void destroy(GpuMesh& g) {
    glDeleteBuffers(1, &g.vbo);
    glDeleteBuffers(1, &g.ibo);
    glDeleteVertexArrays(1, &g.vao);
    g = GpuMesh{};
}

struct Renderer {
    ObjectProgram object;

    GLuint lineProgram = 0;
    GLint lineMvp = -1;
    GLint lineColor = -1;
    GLuint gridVao = 0;
    GLuint gridVbo = 0;
    GLsizei gridVertexCount = 0;

    std::unordered_map<int, GpuMesh> gpu;

    float aspect = 1.0f;
    float width = 1.0f;
    float height = 1.0f;
    Editor editor;
};

Renderer gRenderer;
std::mutex gMutex;  // protege o editor e o renderizador (thread da UI, do GL e do agente)

void buildGrid(Renderer& r) {
    std::vector<float> v;
    const float e = static_cast<float>(kGridHalf);
    for (int i = -kGridHalf; i <= kGridHalf; ++i) {
        if (i == 0) continue;
        const float f = static_cast<float>(i);
        v.insert(v.end(), {-e, 0.0f, f, e, 0.0f, f});
        v.insert(v.end(), {f, 0.0f, -e, f, 0.0f, e});
    }
    r.gridVertexCount = static_cast<GLsizei>(v.size() / 3);
    v.insert(v.end(), {-e, 0.0f, 0.0f, e, 0.0f, 0.0f});  // eixo X
    v.insert(v.end(), {0.0f, 0.0f, -e, 0.0f, 0.0f, e});  // eixo Z

    glGenVertexArrays(1, &r.gridVao);
    glBindVertexArray(r.gridVao);
    glGenBuffers(1, &r.gridVbo);
    glBindBuffer(GL_ARRAY_BUFFER, r.gridVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(float)), v.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);
}

// view = T(0,0,-dist) * Rx(pitch) * Ry(yaw) * T(-target)
Mat4 viewMatrix(const CameraState& c) {
    return multiply(translate(0.0f, 0.0f, -c.distance),
                    multiply(rotateX(c.pitch), multiply(rotateY(c.yaw), translate(-c.target.x, -c.target.y, -c.target.z))));
}

Mat4 cameraRotationInverse(const CameraState& c) { return multiply(rotateY(-c.yaw), rotateX(-c.pitch)); }

Vec3 cameraPosition(const CameraState& c) {
    return c.target + transformDir(cameraRotationInverse(c), Vec3{0.0f, 0.0f, c.distance});
}

Ray cameraRay(const Renderer& r, float px, float py) {
    const float ndcX = 2.0f * px / r.width - 1.0f;
    const float ndcY = 1.0f - 2.0f * py / r.height;
    const float t = std::tan(kFovY * 0.5f);
    const Vec3 dirView{ndcX * t * r.aspect, ndcY * t, -1.0f};
    const CameraState& c = r.editor.camera;
    Ray ray;
    ray.origin = cameraPosition(c);
    ray.dir = normalize(transformDir(cameraRotationInverse(c), dirView));
    return ray;
}

jboolean toJ(bool b) { return b ? JNI_TRUE : JNI_FALSE; }

std::string toStd(JNIEnv* env, jstring s) {
    if (s == nullptr) return std::string();
    const char* chars = env->GetStringUTFChars(s, nullptr);
    std::string out = chars != nullptr ? chars : "";
    if (chars != nullptr) env->ReleaseStringUTFChars(s, chars);
    return out;
}

Editor& ed() { return gRenderer.editor; }

const char* kPrimitiveNames[] = {"box", "sphere", "cylinder", "cone", "torus", "plane"};

}  // namespace

extern "C" {

// ---------- Ciclo de vida do GL ----------

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onSurfaceCreated(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    Renderer& r = gRenderer;

    // Contexto novo: os objetos GL antigos já não existem, basta esquecer.
    r.gpu.clear();

    r.object.id = linkProgram(kObjectVertexShader, kObjectFragmentShader);
    fetchUniforms(r.object);

    r.lineProgram = linkProgram(kLineVertexShader, kLineFragmentShader);
    r.lineMvp = glGetUniformLocation(r.lineProgram, "uMvp");
    r.lineColor = glGetUniformLocation(r.lineProgram, "uColor");
    buildGrid(r);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onSurfaceChanged(JNIEnv*, jobject, jint width, jint height) {
    std::lock_guard<std::mutex> lock(gMutex);
    glViewport(0, 0, width, height);
    gRenderer.width = width > 0 ? static_cast<float>(width) : 1.0f;
    gRenderer.height = height > 0 ? static_cast<float>(height) : 1.0f;
    gRenderer.aspect = gRenderer.width / gRenderer.height;
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onDrawFrame(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    Renderer& r = gRenderer;
    const Scene& scene = r.editor.scene();
    const CameraState& cam = r.editor.camera;
    const ObjectProgram& op = r.object;

    glClearColor(scene.env.skyHorizon.r * 0.25f, scene.env.skyHorizon.g * 0.25f, scene.env.skyHorizon.b * 0.25f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const Mat4 proj = perspective(kFovY, r.aspect, 0.05f, 500.0f);
    const Mat4 viewProj = multiply(proj, viewMatrix(cam));
    const Vec3 camPos = cameraPosition(cam);

    // Grelha e eixos.
    glUseProgram(r.lineProgram);
    glUniformMatrix4fv(r.lineMvp, 1, GL_FALSE, viewProj.m);
    glBindVertexArray(r.gridVao);
    glUniform3f(r.lineColor, 0.30f, 0.30f, 0.34f);
    glDrawArrays(GL_LINES, 0, r.gridVertexCount);
    glUniform3f(r.lineColor, 0.80f, 0.25f, 0.25f);
    glDrawArrays(GL_LINES, r.gridVertexCount, 2);
    glUniform3f(r.lineColor, 0.25f, 0.40f, 0.85f);
    glDrawArrays(GL_LINES, r.gridVertexCount + 2, 2);
    glBindVertexArray(0);

    // Luzes.
    float posType[kMaxLights * 4] = {0};
    float colorRange[kMaxLights * 4] = {0};
    float spot[kMaxLights * 4] = {0};
    int lightCount = 0;
    for (const Light& l : scene.lights) {
        if (!l.enabled || lightCount >= kMaxLights) continue;
        const int k = lightCount * 4;
        const Vec3 dir = normalize(l.direction);
        if (l.type == LightType::Directional) {
            posType[k] = -dir.x;
            posType[k + 1] = -dir.y;
            posType[k + 2] = -dir.z;
            posType[k + 3] = 0.0f;
        } else {
            posType[k] = l.position.x;
            posType[k + 1] = l.position.y;
            posType[k + 2] = l.position.z;
            posType[k + 3] = l.type == LightType::Point ? 1.0f : 2.0f;
        }
        colorRange[k] = l.color.r * l.intensity;
        colorRange[k + 1] = l.color.g * l.intensity;
        colorRange[k + 2] = l.color.b * l.intensity;
        colorRange[k + 3] = l.range;
        spot[k] = dir.x;
        spot[k + 1] = dir.y;
        spot[k + 2] = dir.z;
        spot[k + 3] = std::cos(radians(l.spotAngleDeg * 0.5f));
        ++lightCount;
    }

    glUseProgram(op.id);
    glUniform3f(op.camPos, camPos.x, camPos.y, camPos.z);
    glUniform1f(op.exposure, scene.env.exposure);
    glUniform1f(op.ambient, scene.env.ambient);
    glUniform3f(op.skyTop, scene.env.skyTop.r, scene.env.skyTop.g, scene.env.skyTop.b);
    glUniform3f(op.skyHorizon, scene.env.skyHorizon.r, scene.env.skyHorizon.g, scene.env.skyHorizon.b);
    glUniform3f(op.ground, scene.env.ground.r, scene.env.ground.g, scene.env.ground.b);
    glUniform1i(op.lightCount, lightCount);
    glUniform4fv(op.lightPosType, kMaxLights, posType);
    glUniform4fv(op.lightColorRange, kMaxLights, colorRange);
    glUniform4fv(op.lightSpot, kMaxLights, spot);

    struct Item {
        const Object* object;
        Mat4 world;
        float distance;
    };
    std::vector<Item> opaque, transparent;
    for (const Object& o : scene.objects) {
        if (!o.visible || !o.render || o.render->indices.empty()) continue;
        Item item{&o, worldMatrix(scene, o), 0.0f};
        const Vec3 pos{item.world.m[12], item.world.m[13], item.world.m[14]};
        item.distance = length(pos - camPos);
        (o.material.opacity < 0.999f ? transparent : opaque).push_back(item);
    }
    std::sort(transparent.begin(), transparent.end(), [](const Item& a, const Item& b) { return a.distance > b.distance; });

    auto draw = [&](const Item& item) {
        const Object& o = *item.object;
        auto found = r.gpu.find(o.id);
        if (found != r.gpu.end() && found->second.version != o.meshVersion) {
            destroy(found->second);
            r.gpu.erase(found);
            found = r.gpu.end();
        }
        if (found == r.gpu.end()) found = r.gpu.emplace(o.id, upload(*o.render, o.meshVersion)).first;

        const Mat4 mvp = multiply(viewProj, item.world);
        float nm[9];
        normalMatrixFromWorld(item.world, nm);
        glUniformMatrix4fv(op.mvp, 1, GL_FALSE, mvp.m);
        glUniformMatrix4fv(op.model, 1, GL_FALSE, item.world.m);
        glUniformMatrix3fv(op.normalMat, 1, GL_FALSE, nm);
        glUniform3f(op.base, o.material.base.r, o.material.base.g, o.material.base.b);
        glUniform1f(op.metallic, o.material.metallic);
        glUniform1f(op.roughness, o.material.roughness);
        glUniform1f(op.clearcoat, o.material.clearcoat);
        glUniform3f(op.emissive, o.material.emissive.r * o.material.emissiveStrength,
                    o.material.emissive.g * o.material.emissiveStrength,
                    o.material.emissive.b * o.material.emissiveStrength);
        glUniform1f(op.opacity, o.material.opacity);
        glUniform1f(op.selected, o.id == scene.selectedId ? 1.0f : 0.0f);
        if (o.doubleSided) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);

        glBindVertexArray(found->second.vao);
        glDrawElements(GL_TRIANGLES, found->second.count, GL_UNSIGNED_INT, nullptr);
    };

    for (const Item& item : opaque) draw(item);
    if (!transparent.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        for (const Item& item : transparent) draw(item);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);

    // Liberta a memória da GPU de objetos que já não existem na cena.
    for (auto it = r.gpu.begin(); it != r.gpu.end();) {
        if (scene.find(it->first) == nullptr) {
            destroy(it->second);
            it = r.gpu.erase(it);
        } else {
            ++it;
        }
    }
}

// ---------- Câmera ----------

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onOrbit(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    std::lock_guard<std::mutex> lock(gMutex);
    CameraState& c = ed().camera;
    c.yaw += dx * 0.01f;
    c.pitch = std::max(-1.55f, std::min(c.pitch + dy * 0.01f, 1.55f));
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onPan(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    std::lock_guard<std::mutex> lock(gMutex);
    CameraState& c = ed().camera;
    const float scale = c.distance * 2.0f * std::tan(kFovY * 0.5f) / gRenderer.height;
    const Mat4 rot = cameraRotationInverse(c);
    const Vec3 right = transformDir(rot, Vec3{1.0f, 0.0f, 0.0f});
    const Vec3 up = transformDir(rot, Vec3{0.0f, 1.0f, 0.0f});
    c.target = c.target - right * (dx * scale) + up * (dy * scale);
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onZoom(JNIEnv*, jobject, jfloat factor) {
    std::lock_guard<std::mutex> lock(gMutex);
    CameraState& c = ed().camera;
    if (factor > 0.0f) c.distance = std::max(0.2f, std::min(c.distance / factor, 400.0f));
}

// ---------- Ferramentas da IA (todas as ferramentas passam por aqui) ----------

JNIEXPORT jstring JNICALL
Java_com_layouter_aimodeling_NativeBridge_toolDefinitions(JNIEnv* env, jobject) {
    static const std::string json = toolDefinitionsJson();
    return env->NewStringUTF(json.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_layouter_aimodeling_NativeBridge_callTool(JNIEnv* env, jobject, jstring name, jstring argsJson) {
    const std::string n = toStd(env, name);
    const std::string a = toStd(env, argsJson);
    std::string result;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        result = callTool(ed(), n, a);
    }
    return env->NewStringUTF(result.c_str());
}

// ---------- Interface (botões e toques) ----------

JNIEXPORT jint JNICALL
Java_com_layouter_aimodeling_NativeBridge_addPrimitive(JNIEnv*, jobject, jint type, jfloat size, jfloat x,
                                                       jfloat y, jfloat z) {
    if (type < 0 || type >= 6 || !(size > 0.0f)) return -1;
    std::lock_guard<std::mutex> lock(gMutex);
    return ed().createPrimitive(kPrimitiveNames[type], Vec3{size, size, size}, 0, Vec3{x, y, z});
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_deleteObject(JNIEnv*, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(ed().transact([&](Scene& s) {
        if (s.find(id) == nullptr) return false;
        eraseObject(s, id);
        return true;
    }));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_setTransform(JNIEnv*, jobject, jint id, jfloat px, jfloat py, jfloat pz,
                                                       jfloat rx, jfloat ry, jfloat rz, jfloat sx, jfloat sy,
                                                       jfloat sz) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(ed().transact([&](Scene& s) {
        Object* o = s.find(id);
        if (o == nullptr) return false;
        o->transform.position = Vec3{px, py, pz};
        o->transform.rotationDeg = Vec3{rx, ry, rz};
        o->transform.scale = Vec3{std::max(0.001f, sx), std::max(0.001f, sy), std::max(0.001f, sz)};
        return true;
    }));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_moveBy(JNIEnv*, jobject, jint id, jfloat dx, jfloat dy, jfloat dz) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(ed().transact([&](Scene& s) {
        Object* o = s.find(id);
        if (o == nullptr) return false;
        o->transform.position += Vec3{dx, dy, dz};
        return true;
    }));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_rotateBy(JNIEnv*, jobject, jint id, jfloat rx, jfloat ry, jfloat rz) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(ed().transact([&](Scene& s) {
        Object* o = s.find(id);
        if (o == nullptr) return false;
        o->transform.rotationDeg += Vec3{rx, ry, rz};
        return true;
    }));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_scaleBy(JNIEnv*, jobject, jint id, jfloat fx, jfloat fy, jfloat fz) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(ed().transact([&](Scene& s) {
        Object* o = s.find(id);
        if (o == nullptr) return false;
        const Vec3 sc = o->transform.scale;
        o->transform.scale = Vec3{std::max(0.001f, std::min(sc.x * fx, 1000.0f)), std::max(0.001f, std::min(sc.y * fy, 1000.0f)),
                                  std::max(0.001f, std::min(sc.z * fz, 1000.0f))};
        return true;
    }));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_setColor(JNIEnv*, jobject, jint id, jfloat r, jfloat g, jfloat b) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(ed().transact([&](Scene& s) {
        Object* o = s.find(id);
        if (o == nullptr) return false;
        o->material.base.r = std::max(0.0f, std::min(r, 1.0f));
        o->material.base.g = std::max(0.0f, std::min(g, 1.0f));
        o->material.base.b = std::max(0.0f, std::min(b, 1.0f));
        return true;
    }));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_undo(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(ed().undo());
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_redo(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(ed().redo());
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_canUndo(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(ed().canUndo());
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_canRedo(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(ed().canRedo());
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_select(JNIEnv*, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    ed().select(id);
}

JNIEXPORT jint JNICALL
Java_com_layouter_aimodeling_NativeBridge_getSelected(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return ed().selected();
}

JNIEXPORT jint JNICALL
Java_com_layouter_aimodeling_NativeBridge_pickAt(JNIEnv*, jobject, jfloat x, jfloat y) {
    std::lock_guard<std::mutex> lock(gMutex);
    return ed().pick(cameraRay(gRenderer, x, y));
}

JNIEXPORT jstring JNICALL
Java_com_layouter_aimodeling_NativeBridge_describeScene(JNIEnv* env, jobject) {
    std::string json;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        json = ed().describeScene(0, 40);
    }
    return env->NewStringUTF(json.c_str());
}

}  // extern "C"
