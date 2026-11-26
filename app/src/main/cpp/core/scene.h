#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/math.h"
#include "core/mesh.h"
#include "core/polymesh.h"

namespace core {

struct Color {
    float r = 0.8f;
    float g = 0.8f;
    float b = 0.8f;
};

// Material PBR (metal/rugosidade) com verniz (clearcoat) para tinta de carro.
struct Material {
    Color base;
    float metallic = 0.0f;
    float roughness = 0.5f;
    float clearcoat = 0.0f;
    Color emissive{0.0f, 0.0f, 0.0f};
    float emissiveStrength = 0.0f;
    float opacity = 1.0f;
};

struct Transform {
    Vec3 position;
    Vec3 rotationDeg;  // Euler XYZ em graus
    Vec3 scale{1.0f, 1.0f, 1.0f};

    // T * Rz * Ry * Rx * S
    Mat4 matrix() const;
};

struct PhysicsBody {
    bool enabled = false;
    bool dynamic = true;
    float mass = 1.0f;
    float restitution = 0.2f;
    float friction = 0.5f;
};

struct Object {
    int id = 0;
    std::string name;
    std::string kind;
    std::shared_ptr<const PolyMesh> poly;  // malha editável (partilhada entre cópias)
    std::shared_ptr<const Mesh> render;    // malha de desenho derivada de poly
    int meshVersion = 0;                   // muda sempre que a malha de desenho muda
    float smoothAngle = 45.0f;             // 0 = plano, 180 = tudo suave
    bool doubleSided = false;
    bool visible = true;
    int parent = -1;
    Transform transform;
    Material material;
    PhysicsBody physics;
};

enum class LightType { Directional = 0, Point = 1, Spot = 2 };

struct Light {
    int id = 0;
    std::string name;
    LightType type = LightType::Directional;
    Vec3 position;
    Vec3 direction{-0.4f, -0.8f, -0.6f};  // para onde a luz aponta (direcional e foco)
    Color color{1.0f, 1.0f, 1.0f};
    float intensity = 3.0f;
    float range = 20.0f;
    float spotAngleDeg = 40.0f;
    bool enabled = true;
};

struct Environment {
    Color skyTop{0.30f, 0.50f, 0.85f};
    Color skyHorizon{0.80f, 0.85f, 0.92f};
    Color ground{0.22f, 0.22f, 0.24f};
    float ambient = 0.6f;
    float exposure = 1.0f;
};

// Câmera orbital. Não faz parte do histórico de desfazer.
struct CameraState {
    float yaw = 0.6f;
    float pitch = 0.35f;
    float distance = 8.0f;
    Vec3 target{0.0f, 0.5f, 0.0f};
};

struct Ray {
    Vec3 origin;
    Vec3 dir;
};

struct Scene {
    std::vector<Object> objects;
    std::vector<Light> lights;
    Environment env;
    int nextId = 1;  // partilhado por objetos e luzes
    int selectedId = -1;

    Object* find(int id);
    const Object* find(int id) const;
    int indexOf(int id) const;
    Light* findLight(int id);
};

// Matriz do objeto no mundo (com a cadeia de pais).
Mat4 worldMatrix(const Scene& scene, const Object& object);

// Matriz 3x3 (ordem de colunas) para transformar normais: inversa transposta da parte 3x3.
void normalMatrixFromWorld(const Mat4& world, float out[9]);

// Id do objeto mais próximo atingido pelo raio (em espaço do mundo), ou -1.
int pickObject(const Scene& scene, const Ray& ray);

void objectBounds(const Object& object, Vec3& mn, Vec3& mx);                       // espaço local
void worldBounds(const Scene& scene, const Object& object, Vec3& mn, Vec3& mx);  // espaço do mundo

}  // namespace core
