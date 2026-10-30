#pragma once

namespace engine {

class Application;

// One screen of the game (title, menu, level, pause overlay, ...). Owned by the SceneManager.
class Scene {
public:
    explicit Scene(Application& app) : app_(app) {}
    virtual ~Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    virtual void OnEnter() {}
    virtual void OnExit() {}

    // Once per rendered frame, with the real frame time. Use for input and UI.
    virtual void Update(float /*dt*/) {}
    // At the fixed simulation rate (AppConfig::fixedTimestep). Use for gameplay and physics.
    virtual void FixedUpdate(float /*dt*/) {}
    // alpha in [0, 1) is how far the frame is between two fixed steps, for interpolation.
    virtual void Draw(float alpha) const = 0;

protected:
    Application& App() const { return app_; }

private:
    Application& app_;
};

}  // namespace engine
