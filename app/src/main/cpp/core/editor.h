#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/scene.h"

namespace core {

class Command {
public:
    virtual ~Command() = default;
    virtual void apply(Scene& scene) = 0;
    virtual void revert(Scene& scene) = 0;
};

enum class SelectMode { Object, Vertex, Edge, Face };

class Editor {
public:
    const Scene& scene() const { return scene_; }

    int addPrimitive(Primitive type, float size, Vec3 position);
    int cloneObject(int id);
    bool deleteObject(int id);

    bool setTransform(int id, const Transform& transform);
    bool moveBy(int id, Vec3 offset);
    bool rotateBy(int id, Vec3 degrees);
    bool scaleBy(int id, Vec3 factor);
    bool setColor(int id, Color color);

    // Mesh ops: soft_normals, hard_normals, divide, smooth, weld, mirror, extrude, bevel
    bool meshOp(int id, const std::string& op, float amount, int segments,
                const std::vector<int>& faceIndices);

    int combineObjects(int idA, int idB);
    int separateFaces(int id, const std::vector<int>& faceIndices);

    void setSelectMode(SelectMode mode) { selectMode_ = mode; }
    SelectMode selectMode() const { return selectMode_; }
    void setSelectedFaces(const std::vector<int>& faces) { selectedFaces_ = faces; }

    bool undo();
    bool redo();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }

    void select(int id);
    int selected() const { return scene_.selectedId; }
    int pick(const Ray& ray);

    std::string describeScene() const;

private:
    void execute(std::unique_ptr<Command> command);
    void fixSelection();

    Scene scene_;
    std::vector<std::unique_ptr<Command>> undo_;
    std::vector<std::unique_ptr<Command>> redo_;
    SelectMode selectMode_ = SelectMode::Object;
    std::vector<int> selectedFaces_;
};

}  // namespace core
