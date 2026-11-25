#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/scene.h"

namespace core {

// Operação reversível sobre a cena.
class Command {
public:
    virtual ~Command() = default;
    virtual void apply(Scene& scene) = 0;
    virtual void revert(Scene& scene) = 0;
};

// Fachada de edição: toda alteração da cena passa por aqui e entra no histórico (undo/redo).
// Usada tanto pela interface quanto pelas chamadas de ferramentas da IA.
class Editor {
public:
    const Scene& scene() const { return scene_; }

    // Devolve o id do novo objeto, ou -1 se os valores forem inválidos ou o limite foi atingido.
    int addPrimitive(Primitive type, float size, Vec3 position);
    int duplicateObject(int id, Vec3 offset);
    bool deleteObject(int id);

    bool setTransform(int id, const Transform& transform);
    bool moveBy(int id, Vec3 offset);
    bool rotateBy(int id, Vec3 degrees);
    bool scaleBy(int id, Vec3 factor);
    bool setColor(int id, Color color);
    bool snapToGround(int id);
    bool snapToGrid(int id, float step);

    void clearScene();
    void clearHistory();
    bool undo();
    bool redo();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }

    void select(int id);
    int selected() const { return scene_.selectedId; }
    // Seleciona o objeto atingido pelo raio (ou limpa a seleção) e devolve o id.
    int pick(const Ray& ray);

    // Resumo da cena em JSON, para dar contexto à IA.
    std::string describeScene() const;

private:
    void execute(std::unique_ptr<Command> command);
    void fixSelection();

    Scene scene_;
    std::vector<std::unique_ptr<Command>> undo_;
    std::vector<std::unique_ptr<Command>> redo_;
};

}  // namespace core
