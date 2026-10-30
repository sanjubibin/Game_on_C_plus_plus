#pragma once

#include <memory>
#include <vector>

#include "engine/Scene.h"

namespace engine {

// Stack of scenes. Only the top scene updates; all scenes draw bottom-to-top so overlays
// such as a pause menu can sit on top of gameplay. Changes are queued and applied between
// frames, so a scene can safely push, pop or replace itself from inside Update.
class SceneManager {
public:
    void Push(std::unique_ptr<Scene> scene);
    void Pop();
    void Replace(std::unique_ptr<Scene> scene);
    void Clear();

    void ApplyPending();
    bool Empty() const { return stack_.empty(); }

    void Update(float dt);
    void FixedUpdate(float dt);
    void Draw(float alpha) const;

private:
    enum class Op { Push, Pop, Replace, Clear };
    struct Change {
        Op op;
        std::unique_ptr<Scene> scene;
    };

    void PopTop();

    std::vector<std::unique_ptr<Scene>> stack_;
    std::vector<Change> pending_;
};

}  // namespace engine
