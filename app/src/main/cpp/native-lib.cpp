#include <GLES3/gl3.h>
#include <android/log.h>
#include <jni.h>

#include <cmath>

#include "core/mesh.h"

#define LOG_TAG "aimodeling"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

// Matrizes 4x4 em ordem de colunas (como o OpenGL espera).
struct Mat4 {
    float m[16];
};

Mat4 identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[c * 4 + k];
            }
            r.m[c * 4 + row] = sum;
        }
    }
    return r;
}

Mat4 perspective(float fovyRadians, float aspect, float nearZ, float farZ) {
    const float f = 1.0f / std::tan(fovyRadians * 0.5f);
    Mat4 r{};
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (farZ + nearZ) / (nearZ - farZ);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    return r;
}

Mat4 translateZ(float z) {
    Mat4 r = identity();
    r.m[14] = z;
    return r;
}

Mat4 rotateX(float a) {
    Mat4 r = identity();
    r.m[5] = std::cos(a);
    r.m[6] = std::sin(a);
    r.m[9] = -std::sin(a);
    r.m[10] = std::cos(a);
    return r;
}

Mat4 rotateY(float a) {
    Mat4 r = identity();
    r.m[0] = std::cos(a);
    r.m[2] = -std::sin(a);
    r.m[8] = std::sin(a);
    r.m[10] = std::cos(a);
    return r;
}

const char* kVertexShader = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMvp;
uniform mat4 uModel;
out vec3 vNormal;
void main() {
    vNormal = mat3(uModel) * aNormal;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kFragmentShader = R"(#version 300 es
precision mediump float;
in vec3 vNormal;
out vec4 outColor;
void main() {
    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.8, 0.6));
    float diff = max(dot(n, lightDir), 0.0);
    vec3 base = vec3(0.35, 0.60, 0.95);
    outColor = vec4(base * (0.25 + 0.75 * diff), 1.0);
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

struct Renderer {
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ibo = 0;
    GLint mvpLoc = -1;
    GLint modelLoc = -1;
    GLsizei indexCount = 0;
    float aspect = 1.0f;
    float angle = 0.0f;
};

Renderer gRenderer;

}  // namespace

extern "C" {

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onSurfaceCreated(JNIEnv*, jobject) {
    Renderer& r = gRenderer;

    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    r.program = glCreateProgram();
    glAttachShader(r.program, vs);
    glAttachShader(r.program, fs);
    glLinkProgram(r.program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    r.mvpLoc = glGetUniformLocation(r.program, "uMvp");
    r.modelLoc = glGetUniformLocation(r.program, "uModel");

    const core::Mesh mesh = core::makeCube(1.0f);
    r.indexCount = static_cast<GLsizei>(mesh.indices.size());

    glGenVertexArrays(1, &r.vao);
    glBindVertexArray(r.vao);

    glGenBuffers(1, &r.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(core::Vertex)),
                 mesh.vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &r.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint16_t)),
                 mesh.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(core::Vertex),
                          reinterpret_cast<void*>(offsetof(core::Vertex, px)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(core::Vertex),
                          reinterpret_cast<void*>(offsetof(core::Vertex, nx)));

    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onSurfaceChanged(JNIEnv*, jobject, jint width,
                                                           jint height) {
    glViewport(0, 0, width, height);
    gRenderer.aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
}

JNIEXPORT void JNICALL
Java_com_layouter_aimodeling_NativeBridge_onDrawFrame(JNIEnv*, jobject) {
    Renderer& r = gRenderer;
    r.angle += 0.015f;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const Mat4 model = multiply(rotateY(r.angle), rotateX(r.angle * 0.6f));
    const Mat4 view = translateZ(-3.0f);
    const Mat4 proj = perspective(1.0472f, r.aspect, 0.1f, 100.0f);  // 60 graus
    const Mat4 mvp = multiply(proj, multiply(view, model));

    glUseProgram(r.program);
    glUniformMatrix4fv(r.mvpLoc, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(r.modelLoc, 1, GL_FALSE, model.m);

    glBindVertexArray(r.vao);
    glDrawElements(GL_TRIANGLES, r.indexCount, GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);
}

}  // extern "C"
