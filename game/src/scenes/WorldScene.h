#pragma once

#include <raylib.h>

#include <string>

#include "GameSession.h"
#include "engine/Scene.h"

// The explorable world: the current area (town, market, ...), walking, talking, doors, spots,
// exits to other areas, the clock and the HUD.
class WorldScene : public engine::Scene {
public:
    WorldScene(engine::Application& app, GameSession& session);

    void Update(float dt) override;
    void FixedUpdate(float dt) override;
    void Draw(float alpha) const override;

private:
    enum class TargetKind { None, Npc, Door, Spot };
    struct Target {
        TargetKind kind = TargetKind::None;
        const game::NpcDef* npc = nullptr;
        const game::Building* building = nullptr;
        const game::Spot* spot = nullptr;
        Vector2 point{};
        std::string label;
    };

    const game::MapDef& Map() const;
    Target FindTarget() const;
    void Interact(const Target& target);
    bool Blocked(Rectangle feet) const;
    bool NpcPresent(const game::NpcDef& npc) const;
    bool HasNewStory(const game::NpcDef& npc) const;
    Vector2 FollowerPos(std::size_t index) const;
    Camera2D MakeCamera(Vector2 focus) const;
    void CheckExits();
    void Travel(const game::Exit& exit);

    void DrawTiles(const Camera2D& camera) const;
    void DrawBuildings() const;
    void DrawSpots() const;
    void DrawFountain() const;
    void DrawBunting() const;
    void DrawCharacters(Vector2 player) const;
    void DrawOverheads(const Camera2D& camera, Vector2 player) const;
    void DrawPrompt(const Camera2D& camera) const;
    void DrawHud() const;

    GameSession& s_;
    Vector2 prevPos_{};
    float clockAccum_ = 0.0f;
    float walkTime_ = 0.0f;
    double fadeStart_ = -100.0;  // when we last travelled; drives a short black fade-in
    bool moving_ = false;
    bool eveningQueued_ = false;
    Target target_;
};
