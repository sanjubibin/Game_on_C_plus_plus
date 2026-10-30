#pragma once

#include <raylib.h>

#include <vector>

#include "GameSession.h"
#include "engine/Scene.h"

// Overlay with five pages: Quests, Bag, Skills, People and Life (wheel + memories).
class JournalScene : public engine::Scene {
public:
    JournalScene(engine::Application& app, GameSession& session);

    void Update(float dt) override;
    void Draw(float alpha) const override;

    void SetTab(int tab) { tab_ = tab; }

private:
    Rectangle PanelRect() const;
    std::vector<Rectangle> TabRects() const;

    void DrawQuests(Rectangle area) const;
    void DrawBag(Rectangle area) const;
    void DrawSkills(Rectangle area) const;
    void DrawPeople(Rectangle area) const;
    void DrawLife(Rectangle area) const;

    GameSession& s_;
    int tab_ = 0;
};
