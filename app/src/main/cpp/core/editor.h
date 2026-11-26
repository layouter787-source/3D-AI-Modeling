#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/scene.h"

namespace core {

// Reconstrói a malha de desenho de um objeto a partir da malha editável.
void refreshRender(Object& object);
void setPoly(Object& object, PolyMesh poly);

// Cria um objeto (fica selecionado). Devolve a referência ao novo objeto: só é válida até à próxima inserção.
Object& addObjectTo(Scene& scene, const std::string& kind, PolyMesh poly, const std::string& name,
                    Vec3 position);
void eraseObject(Scene& scene, int id);

// Primitivas com topologia boa para subdivisão. type: box, sphere, cylinder, cone, torus, plane.
// dims escala a primitiva unitária (diâmetro/aresta 1). segments <= 0 usa o padrão do tipo.
PolyMesh makePrimitivePoly(const std::string& type, Vec3 dims, int segments);

std::string describeObject(const Scene& scene, const Object& object, bool detailed);

// Cena + histórico (desfazer/refazer por cópias da cena; as malhas são partilhadas, por isso é barato).
class Editor {
public:
    Editor();

    const Scene& scene() const { return scene_; }
    Scene& mutableScene() { return scene_; }  // usar só dentro de transact ou das ferramentas

    CameraState camera;
    std::map<std::string, Scene> checkpoints;

    // Executa fn sobre a cena. Se fn devolver false ou lançar uma exceção, a cena volta ao estado
    // anterior (a exceção continua a propagar-se). Se tiver sucesso, entra no histórico.
    bool transact(const std::function<bool(Scene&)>& fn);

    bool undo();
    bool redo();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }

    void select(int id);
    int selected() const { return scene_.selectedId; }
    int pick(const Ray& ray);

    int createPrimitive(const std::string& type, Vec3 dims, int segments, Vec3 position);

    std::string describeScene(int offset, int limit) const;

private:
    void fixSelection();

    Scene scene_;
    std::vector<Scene> undo_;
    std::vector<Scene> redo_;
};

}  // namespace core
