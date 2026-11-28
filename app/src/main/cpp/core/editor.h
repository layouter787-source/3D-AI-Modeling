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

    // Devolve o id do novo objeto, ou -1 se os valores forem inválidos ou os recursos da malha falharem.
    int addPrimitive(Primitive type, float size, Vec3 position, int detail = 8);
    int addImportedMesh(Mesh mesh, Color color, float metallic, float roughness, const std::string& name);
    // Profiles are [x, halfWidth, centerY, halfHeight] along the X axis.
    int addLoftMesh(const std::vector<float>& profiles, int segments, Vec3 position);
    int duplicateObject(int id, Vec3 offset);
    bool deleteObject(int id);

    bool setTransform(int id, const Transform& transform);
    bool moveBy(int id, Vec3 offset);
    bool rotateBy(int id, Vec3 degrees);
    bool scaleBy(int id, Vec3 factor);
    bool sculptMesh(int id, Vec3 center, float radius, Vec3 offset);
    // Blender-inspired polygon operations applied to the existing editable triangle topology.
    bool meshOperation(int id, const std::string& operation, const std::vector<int>& faces,
                       int parameter, float value, int otherObjectId = -1);
    int detachFaces(int id, const std::vector<int>& faces);
    bool combineObjects(int id, int otherObjectId);
    std::vector<int> faceShell(int id, int startFace) const;
    bool beginVertexPaintStroke(int id);
    bool paintVertexAt(int id, const Ray& ray, float radius, Vec3 color, float opacity);
    void endVertexPaintStroke();
    bool paintVertexRegion(int id, Vec3 center, float radius, Color color, float opacity);
    std::string describeMesh(int id, int offset, int limit) const;
    bool setColor(int id, Color color);
    bool setMaterial(int id, Material material);
    std::vector<float> getVertexColors(int id) const;
    bool setVertexColors(int id, const std::vector<float>& rgb);
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
    int paintStrokeObjectId_ = -1;
    bool paintStrokeChanged_ = false;
    Mesh paintStrokeBeforeMesh_;
    PolyMesh paintStrokeBeforeTopology_;
    std::vector<Vec3> paintStrokeBeforeColors_;
    float paintStrokeBeforeSmoothAngle_ = 35.0f;
    std::vector<std::unique_ptr<Command>> undo_;
    std::vector<std::unique_ptr<Command>> redo_;
};

}  // namespace core
