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

    // Mesh operations (soft/hard normals, divide, smooth, mirror, weld, extrude, ...)
    // Returns empty string on failure, or success message.
    std::string meshOperation(int id, const std::string& op, float amount, int iterations);

    // Combine object b into a (a keeps id, b is deleted).
    bool combineObjects(int idA, int idB);

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
};

}  // namespace core
