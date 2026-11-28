#include <GLES3/gl3.h>
#include <android/log.h>
#include <jni.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "core/editor.h"

using namespace core;

#define LOG_TAG "aimodeling"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

constexpr float kFovY = 1.0472f;  // 60 graus
constexpr int kGridHalf = 10;

const char* kObjectVertexShader = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMvp;
uniform mat4 uModel;
uniform mat3 uNormalMat;
out vec3 vNormal;
out vec3 vWorldPos;
void main() {
    vNormal = normalize(uNormalMat * aNormal);
    vWorldPos = (uModel * vec4(aPos, 1.0)).xyz;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kObjectFragmentShader = R"(#version 300 es
precision highp float;
in vec3 vNormal;
in vec3 vWorldPos;
uniform vec3 uColor;
uniform vec3 uCameraPosition;
uniform float uSelected;
uniform float uMetallic;
uniform float uRoughness;
uniform float uClearcoat;
uniform float uEmission;
uniform vec3 uSunDirection;
uniform vec3 uSunColor;
uniform float uSunIntensity;
uniform float uAmbientStrength;
out vec4 outColor;
void main() {
    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(uSunDirection);
    vec3 viewDir = normalize(uCameraPosition - vWorldPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    float diffuse = max(dot(n, lightDir), 0.0);
    float gloss = mix(8.0, 160.0, 1.0 - uRoughness);
    float specular = pow(max(dot(n, halfDir), 0.0), gloss) * mix(0.18, 0.92, uMetallic);
    float coat = pow(max(dot(n, halfDir), 0.0), 96.0) * uClearcoat * 0.65;
    float fresnel = pow(1.0 - max(dot(n, viewDir), 0.0), 3.0) * 0.10;
    float rim = pow(1.0 - max(dot(n, viewDir), 0.0), 2.0) * 0.07;
    vec3 f0 = mix(vec3(0.04), uColor, uMetallic);
    vec3 base = uColor * (uAmbientStrength + uSunIntensity * 0.76 * diffuse) * (1.0 - 0.28 * uMetallic);
    vec3 lit = base + uSunColor * f0 * uSunIntensity * (specular + fresnel) + uSunColor * vec3(coat) * uSunIntensity;
    lit += uColor * rim;
    lit += uColor * uEmission;
    lit = mix(lit, vec3(1.0, 0.58, 0.16), 0.42 * uSelected);
    outColor = vec4(lit, 1.0);
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
        char log[512];
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

// Câmera orbital: rodar (yaw/pitch), aproximar (distance) e deslocar (pan no plano da tela).
struct Camera {
    float yaw = 0.6f;
    float pitch = 0.4f;
    float distance = 6.0f;
    float panX = 0.0f;
    float panY = 0.0f;

    Mat4 view() const {
        return multiply(translate(panX, panY, -distance), multiply(rotateX(pitch), rotateY(yaw)));
    }
};

struct GpuMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ibo = 0;
    GLsizei count = 0;
};

GpuMesh upload(const Mesh& mesh) {
    GpuMesh g;
    g.count = static_cast<GLsizei>(mesh.indices.size());

    glGenVertexArrays(1, &g.vao);
    glBindVertexArray(g.vao);

    glGenBuffers(1, &g.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)),
                 mesh.vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &g.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint32_t)),
                 mesh.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, px)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, nx)));

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
    GLuint objectProgram = 0;
    GLint objMvp = -1;
    GLint objModel = -1;
    GLint objNormal = -1;
    GLint objCamera = -1;
    GLint objColor = -1;
    GLint objSelected = -1;
    GLint objMetallic = -1;
    GLint objRoughness = -1;
    GLint objClearcoat = -1;
    GLint objEmission = -1;
    GLint objSunDirection = -1;
    GLint objSunColor = -1;
    GLint objSunIntensity = -1;
    GLint objAmbientStrength = -1;

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
    bool darkViewport = true;
    Vec3 sunDirection{-0.45f, 0.82f, 0.36f};
    Vec3 sunColor{1.0f, 0.94f, 0.82f};
    float sunIntensity = 1.0f;
    float ambientStrength = 0.16f;
    Color skyColor{0.12f, 0.14f, 0.20f};
    Camera camera;
    Editor editor;
};

Renderer gRenderer;
std::mutex gMutex;  // protege o editor, a câmera e o estado do renderizador (UI e thread do GL)

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
    // Eixos no fim: X (2 vértices) e Z (2 vértices).
    v.insert(v.end(), {-e, 0.0f, 0.0f, e, 0.0f, 0.0f});
    v.insert(v.end(), {0.0f, 0.0f, -e, 0.0f, 0.0f, e});

    glGenVertexArrays(1, &r.gridVao);
    glBindVertexArray(r.gridVao);
    glGenBuffers(1, &r.gridVbo);
    glBindBuffer(GL_ARRAY_BUFFER, r.gridVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(float)), v.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);
}

Ray cameraRay(const Renderer& r, float px, float py) {
    const float ndcX = 2.0f * px / r.width - 1.0f;
    const float ndcY = 1.0f - 2.0f * py / r.height;
    const float t = std::tan(kFovY * 0.5f);
    const Vec3 dirView{ndcX * t * r.aspect, ndcY * t, -1.0f};

    // view = T(pan, -dist) * Rx(pitch) * Ry(yaw)  =>  inversa = Ry(-yaw) * Rx(-pitch) * T(-pan, dist)
    const Camera& c = r.camera;
    const Mat4 rot = multiply(rotateY(-c.yaw), rotateX(-c.pitch));
    Ray ray;
    ray.origin = transformPoint(rot, Vec3{-c.panX, -c.panY, c.distance});
    ray.dir = normalize(transformDir(rot, dirView));
    return ray;
}

jboolean toJ(bool b) { return b ? JNI_TRUE : JNI_FALSE; }

}  // namespace

extern "C" {

// ---------- Ciclo de vida do GL ----------

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onSurfaceCreated(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    Renderer& r = gRenderer;

    // Contexto novo: os objetos GL antigos já não existem, basta esquecer.
    r.gpu.clear();

    r.objectProgram = linkProgram(kObjectVertexShader, kObjectFragmentShader);
    r.objMvp = glGetUniformLocation(r.objectProgram, "uMvp");
    r.objModel = glGetUniformLocation(r.objectProgram, "uModel");
    r.objCamera = glGetUniformLocation(r.objectProgram, "uCameraPosition");
    r.objNormal = glGetUniformLocation(r.objectProgram, "uNormalMat");
    r.objColor = glGetUniformLocation(r.objectProgram, "uColor");
    r.objSelected = glGetUniformLocation(r.objectProgram, "uSelected");
    r.objMetallic = glGetUniformLocation(r.objectProgram, "uMetallic");
    r.objRoughness = glGetUniformLocation(r.objectProgram, "uRoughness");
    r.objClearcoat = glGetUniformLocation(r.objectProgram, "uClearcoat");
    r.objEmission = glGetUniformLocation(r.objectProgram, "uEmission");
    r.objSunDirection = glGetUniformLocation(r.objectProgram, "uSunDirection");
    r.objSunColor = glGetUniformLocation(r.objectProgram, "uSunColor");
    r.objSunIntensity = glGetUniformLocation(r.objectProgram, "uSunIntensity");
    r.objAmbientStrength = glGetUniformLocation(r.objectProgram, "uAmbientStrength");

    r.lineProgram = linkProgram(kLineVertexShader, kLineFragmentShader);
    r.lineMvp = glGetUniformLocation(r.lineProgram, "uMvp");
    r.lineColor = glGetUniformLocation(r.lineProgram, "uColor");
    buildGrid(r);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(r.skyColor.r, r.skyColor.g, r.skyColor.b, 1.0f);
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onSurfaceChanged(JNIEnv*, jobject, jint width,
                                                           jint height) {
    std::lock_guard<std::mutex> lock(gMutex);
    glViewport(0, 0, width, height);
    gRenderer.width = width > 0 ? static_cast<float>(width) : 1.0f;
    gRenderer.height = height > 0 ? static_cast<float>(height) : 1.0f;
    gRenderer.aspect = gRenderer.width / gRenderer.height;
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_setViewportTheme(JNIEnv*, jobject, jboolean dark) {
    std::lock_guard<std::mutex> lock(gMutex);
    gRenderer.darkViewport = dark == JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_setEnvironment(
    JNIEnv*, jobject, jfloat sunX, jfloat sunY, jfloat sunZ,
    jfloat sunR, jfloat sunG, jfloat sunB, jfloat sunIntensity,
    jfloat ambientStrength, jfloat skyR, jfloat skyG, jfloat skyB) {
    std::lock_guard<std::mutex> lock(gMutex);
    Renderer& r = gRenderer;
    if (!std::isfinite(sunX) || !std::isfinite(sunY) || !std::isfinite(sunZ) ||
        !std::isfinite(sunR) || !std::isfinite(sunG) || !std::isfinite(sunB) ||
        !std::isfinite(sunIntensity) || !std::isfinite(ambientStrength) ||
        !std::isfinite(skyR) || !std::isfinite(skyG) || !std::isfinite(skyB)) return;
    r.sunDirection = normalize(Vec3{sunX, sunY, sunZ});
    r.sunColor = Vec3{std::clamp(sunR, 0.0f, 1.0f), std::clamp(sunG, 0.0f, 1.0f), std::clamp(sunB, 0.0f, 1.0f)};
    r.sunIntensity = std::clamp(sunIntensity, 0.0f, 8.0f);
    r.ambientStrength = std::clamp(ambientStrength, 0.0f, 1.0f);
    r.skyColor = Color{std::clamp(skyR, 0.0f, 1.0f), std::clamp(skyG, 0.0f, 1.0f), std::clamp(skyB, 0.0f, 1.0f)};
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onDrawFrame(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    Renderer& r = gRenderer;
    const Scene& scene = r.editor.scene();

    const float skyMix = r.darkViewport ? 0.72f : 0.34f;
    glClearColor(r.skyColor.r * skyMix, r.skyColor.g * skyMix, r.skyColor.b * skyMix, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const Mat4 proj = perspective(kFovY, r.aspect, 0.1f, 200.0f);
    const Mat4 viewProj = multiply(proj, r.camera.view());

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

    // Objetos.
    glUseProgram(r.objectProgram);
    for (const Object& o : scene.objects) {
        auto it = r.gpu.find(o.id);
        if (it == r.gpu.end()) it = r.gpu.emplace(o.id, upload(o.mesh)).first;

        const Mat4 mvp = multiply(viewProj, o.transform.matrix());
        float nm[9];
        normalMatrix(o.transform, nm);

        glUniformMatrix4fv(r.objMvp, 1, GL_FALSE, mvp.m);
        glUniformMatrix4fv(r.objModel, 1, GL_FALSE, o.transform.matrix().m);
        const Mat4 cameraRotation = multiply(rotateY(-r.camera.yaw), rotateX(-r.camera.pitch));
        const Vec3 cameraPosition = transformPoint(cameraRotation, Vec3{-r.camera.panX, -r.camera.panY, r.camera.distance});
        glUniform3f(r.objCamera, cameraPosition.x, cameraPosition.y, cameraPosition.z);
        glUniformMatrix3fv(r.objNormal, 1, GL_FALSE, nm);
        glUniform3f(r.objColor, o.color.r, o.color.g, o.color.b);
        glUniform1f(r.objSelected, o.id == scene.selectedId ? 1.0f : 0.0f);
        glUniform1f(r.objMetallic, o.material.metallic);
        glUniform1f(r.objRoughness, o.material.roughness);
        glUniform1f(r.objClearcoat, o.material.clearcoat);
        glUniform1f(r.objEmission, o.material.emission);
        glUniform3f(r.objSunDirection, r.sunDirection.x, r.sunDirection.y, r.sunDirection.z);
        glUniform3f(r.objSunColor, r.sunColor.x, r.sunColor.y, r.sunColor.z);
        glUniform1f(r.objSunIntensity, r.sunIntensity);
        glUniform1f(r.objAmbientStrength, r.ambientStrength);

        glBindVertexArray(it->second.vao);
        glDrawElements(GL_TRIANGLES, it->second.count, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);

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
    Camera& c = gRenderer.camera;
    c.yaw += dx * 0.01f;
    c.pitch = std::clamp(c.pitch + dy * 0.01f, -1.5f, 1.5f);
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onPan(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    std::lock_guard<std::mutex> lock(gMutex);
    Camera& c = gRenderer.camera;
    const float scale = c.distance * 2.0f * std::tan(kFovY * 0.5f) / gRenderer.height;
    c.panX += dx * scale;
    c.panY -= dy * scale;
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onZoom(JNIEnv*, jobject, jfloat factor) {
    std::lock_guard<std::mutex> lock(gMutex);
    Camera& c = gRenderer.camera;
    if (factor > 0.0f) c.distance = std::clamp(c.distance / factor, 1.0f, 60.0f);
}

// ---------- Edição da cena ----------

JNIEXPORT jint JNICALL
Java_com_layouter_aimodeling_NativeBridge_addPrimitive(JNIEnv*, jobject, jint type, jfloat size,
                                                       jfloat x, jfloat y, jfloat z) {
    if (type < 0 || type >= kPrimitiveCount) return -1;
    std::lock_guard<std::mutex> lock(gMutex);
    return gRenderer.editor.addPrimitive(static_cast<Primitive>(type), size, Vec3{x, y, z});
}

JNIEXPORT jint JNICALL
Java_com_layouter_aimodeling_NativeBridge_addPrimitiveDetailed(JNIEnv*, jobject, jint type,
                                                               jfloat size, jfloat x, jfloat y,
                                                               jfloat z, jint detail) {
    if (type < 0 || type >= kPrimitiveCount || detail < 1 || detail > 64) return -1;
    std::lock_guard<std::mutex> lock(gMutex);
    return gRenderer.editor.addPrimitive(static_cast<Primitive>(type), size, Vec3{x, y, z}, detail);
}

JNIEXPORT jint JNICALL
Java_com_layouter_aimodeling_NativeBridge_createLoftMesh(JNIEnv* env, jobject, jfloatArray data,
                                                        jint segments, jfloat x, jfloat y, jfloat z) {
    if (data == nullptr) return -1;
    const jsize length=env->GetArrayLength(data);
    if (length < 12 || length > 96 || length % 4 != 0) return -1;
    std::vector<float> profiles(static_cast<size_t>(length));
    env->GetFloatArrayRegion(data,0,length,profiles.data());
    if (env->ExceptionCheck()) { env->ExceptionClear(); return -1; }
    std::lock_guard<std::mutex> lock(gMutex);
    return gRenderer.editor.addLoftMesh(profiles,segments,Vec3{x,y,z});
}

JNIEXPORT jint JNICALL
Java_com_layouter_aimodeling_NativeBridge_duplicateObject(JNIEnv*, jobject, jint id,
                                                          jfloat dx, jfloat dy, jfloat dz) {
    std::lock_guard<std::mutex> lock(gMutex);
    return gRenderer.editor.duplicateObject(id, Vec3{dx, dy, dz});
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_deleteObject(JNIEnv*, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.deleteObject(id));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_setTransform(JNIEnv*, jobject, jint id, jfloat px,
                                                       jfloat py, jfloat pz, jfloat rx, jfloat ry,
                                                       jfloat rz, jfloat sx, jfloat sy, jfloat sz) {
    Transform t;
    t.position = Vec3{px, py, pz};
    t.rotationDeg = Vec3{rx, ry, rz};
    t.scale = Vec3{sx, sy, sz};
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.setTransform(id, t));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_moveBy(JNIEnv*, jobject, jint id, jfloat dx, jfloat dy,
                                                 jfloat dz) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.moveBy(id, Vec3{dx, dy, dz}));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_rotateBy(JNIEnv*, jobject, jint id, jfloat rx, jfloat ry,
                                                   jfloat rz) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.rotateBy(id, Vec3{rx, ry, rz}));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_scaleBy(JNIEnv*, jobject, jint id, jfloat fx, jfloat fy,
                                                  jfloat fz) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.scaleBy(id, Vec3{fx, fy, fz}));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_setColor(JNIEnv*, jobject, jint id, jfloat r, jfloat g,
                                                   jfloat b) {
    Color c;
    c.r = r;
    c.g = g;
    c.b = b;
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.setColor(id, c));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_setMaterial(JNIEnv*, jobject, jint id, jfloat metallic,
                                                       jfloat roughness, jfloat clearcoat,
                                                       jfloat emission) {
    Material material;
    material.metallic = metallic;
    material.roughness = roughness;
    material.clearcoat = clearcoat;
    material.emission = emission;
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.setMaterial(id, material));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_sculptMesh(JNIEnv*, jobject, jint id, jfloat cx,jfloat cy,jfloat cz,jfloat radius,jfloat dx,jfloat dy,jfloat dz){std::lock_guard<std::mutex> lock(gMutex);return toJ(gRenderer.editor.sculptMesh(id,Vec3{cx,cy,cz},radius,Vec3{dx,dy,dz}));}


JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_meshOperation(JNIEnv* env, jobject, jint id, jstring operation,
                                                        jintArray faceIds, jint parameter, jfloat value, jint otherObjectId) {
    if (operation == nullptr) return JNI_FALSE;
    const char* raw = env->GetStringUTFChars(operation, nullptr);
    if (raw == nullptr) return JNI_FALSE;
    std::string op(raw);
    env->ReleaseStringUTFChars(operation, raw);
    std::vector<int> faces;
    if (faceIds != nullptr) {
        const jsize count = env->GetArrayLength(faceIds);
        if (count > 1200) return JNI_FALSE;
        faces.resize(static_cast<size_t>(count));
        env->GetIntArrayRegion(faceIds, 0, count, reinterpret_cast<jint*>(faces.data()));
        if (env->ExceptionCheck()) { env->ExceptionClear(); return JNI_FALSE; }
    }
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.meshOperation(id, op, faces, parameter, value, otherObjectId));
}

JNIEXPORT jstring JNICALL
Java_com_layouter_aimodeling_NativeBridge_getMeshTopology(JNIEnv* env, jobject, jint id, jint offset, jint limit) {
    std::string output;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        output = gRenderer.editor.describeMesh(id, offset, limit);
    }
    return env->NewStringUTF(output.c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_snapToGround(JNIEnv*, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.snapToGround(id));
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_snapToGrid(JNIEnv*, jobject, jint id, jfloat step) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.snapToGrid(id, step));
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_clearScene(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    gRenderer.editor.clearScene();
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_clearHistory(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    gRenderer.editor.clearHistory();
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_undo(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.undo());
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_redo(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.redo());
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_canUndo(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.canUndo());
}

JNIEXPORT jboolean JNICALL
Java_com_layouter_aimodeling_NativeBridge_canRedo(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return toJ(gRenderer.editor.canRedo());
}

// ---------- Seleção e descrição da cena ----------

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_select(JNIEnv*, jobject, jint id) {
    std::lock_guard<std::mutex> lock(gMutex);
    gRenderer.editor.select(id);
}

JNIEXPORT jint JNICALL
Java_com_layouter_aimodeling_NativeBridge_getSelected(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    return gRenderer.editor.selected();
}

JNIEXPORT jint JNICALL
Java_com_layouter_aimodeling_NativeBridge_pickAt(JNIEnv*, jobject, jfloat x, jfloat y) {
    std::lock_guard<std::mutex> lock(gMutex);
    return gRenderer.editor.pick(cameraRay(gRenderer, x, y));
}


JNIEXPORT jstring JNICALL
Java_com_layouter_aimodeling_NativeBridge_exportObj(JNIEnv* env, jobject) {
    std::string output;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        const Scene& scene = gRenderer.editor.scene();
        output.reserve(scene.objects.size() * 2048);
        output += "# 3D-AI-Modeling OBJ export\n";
        output += "mtllib 3d-model.mtl\n";
        size_t vertexOffset = 1;
        for (const Object& object : scene.objects) {
            output += "o " + object.name + "\n";
            output += "usemtl material_" + std::to_string(object.id) + "\n";
            const Mat4 model = object.transform.matrix();
            for (const Vertex& vertex : object.mesh.vertices) {
                const Vec3 p = transformPoint(model, Vec3{vertex.px, vertex.py, vertex.pz});
                char line[160];
                std::snprintf(line, sizeof(line), "v %.6f %.6f %.6f\n", p.x, p.y, p.z);
                output += line;
            }
            float nm[9];
            normalMatrix(object.transform, nm);
            for (const Vertex& vertex : object.mesh.vertices) {
                const Vec3 n = normalize(Vec3{
                    nm[0] * vertex.nx + nm[3] * vertex.ny + nm[6] * vertex.nz,
                    nm[1] * vertex.nx + nm[4] * vertex.ny + nm[7] * vertex.nz,
                    nm[2] * vertex.nx + nm[5] * vertex.ny + nm[8] * vertex.nz
                });
                char line[160];
                std::snprintf(line, sizeof(line), "vn %.6f %.6f %.6f\n", n.x, n.y, n.z);
                output += line;
            }
            const size_t base = vertexOffset;
            for (size_t i = 0; i + 2 < object.mesh.indices.size(); i += 3) {
                char line[128];
                const size_t a = base + object.mesh.indices[i];
                const size_t b = base + object.mesh.indices[i + 1];
                const size_t d = base + object.mesh.indices[i + 2];
                std::snprintf(line, sizeof(line), "f %zu//%zu %zu//%zu %zu//%zu\n",
                              a, a, b, b, d, d);
                output += line;
            }
            vertexOffset += object.mesh.vertices.size();
        }
    }
    return env->NewStringUTF(output.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_layouter_aimodeling_NativeBridge_exportMtl(JNIEnv* env, jobject) {
    std::string output = "# Material library for 3d-model.obj\n";
    {
        std::lock_guard<std::mutex> lock(gMutex);
        const Scene& scene = gRenderer.editor.scene();
        for (const Object& object : scene.objects) {
            char line[320];
            std::snprintf(
                line, sizeof(line),
                "newmtl material_%d\nKa 0.080000 0.080000 0.080000\nKd %.6f %.6f %.6f\nKs %.6f %.6f %.6f\nNs %.6f\nKe %.6f %.6f %.6f\nillum 2\n\n",
                object.id, object.color.r, object.color.g, object.color.b,
                0.12f + object.material.metallic * 0.88f,
                0.12f + object.material.metallic * 0.88f,
                0.12f + object.material.metallic * 0.88f,
                8.0f + (1.0f - object.material.roughness) * 992.0f,
                object.color.r * object.material.emission,
                object.color.g * object.material.emission,
                object.color.b * object.material.emission
            );
            output += line;
        }
    }
    return env->NewStringUTF(output.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_layouter_aimodeling_NativeBridge_describeScene(JNIEnv* env, jobject) {
    std::string json;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        json = gRenderer.editor.describeScene();
        if (!json.empty() && json.back() == '}') {
            json.pop_back();
            char environment[320];
            std::snprintf(environment, sizeof(environment),
                          R"JSON(,"environment":{"sun_direction":[%.3f,%.3f,%.3f],"sun_color":[%.3f,%.3f,%.3f],"sun_intensity":%.3f,"ambient_strength":%.3f,"sky_color":[%.3f,%.3f,%.3f]}})JSON",
                          gRenderer.sunDirection.x, gRenderer.sunDirection.y, gRenderer.sunDirection.z,
                          gRenderer.sunColor.x, gRenderer.sunColor.y, gRenderer.sunColor.z,
                          gRenderer.sunIntensity, gRenderer.ambientStrength,
                          gRenderer.skyColor.r, gRenderer.skyColor.g, gRenderer.skyColor.b);
            json += environment;
        }
    }
    return env->NewStringUTF(json.c_str());
}

}  // extern "C"
