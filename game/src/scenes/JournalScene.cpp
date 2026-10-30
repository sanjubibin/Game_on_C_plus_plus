#include "scenes/JournalScene.h"

#include <array>
#include <string>

#include "Visuals.h"
#include "engine/Application.h"
#include "engine/Ui.h"
#include "logic/Rules.h"

namespace ui = engine::ui;
using namespace visuals;

namespace {

constexpr std::array<const char*, 5> kTabs = {"Quests", "Bag", "Skills", "People", "Life"};

struct SkillInfo {
    const char* id;
    const char* name;
    const char* use;
};

constexpr std::array<SkillInfo, 5> kSkills = {{
    {"skill.money_sense", "Money sense", "Pricing, budgeting and spotting bad deals"},
    {"skill.communication", "Communication", "Explaining, persuading and inviting people in"},
    {"skill.empathy", "Empathy", "Understanding how other people feel"},
    {"skill.creativity", "Creativity", "New ideas and fresh solutions"},
    {"skill.hard_work", "Hard work", "Keeping going when things are tough"},
}};

float F(int value) {
    return static_cast<float>(value);
}

}  // namespace

JournalScene::JournalScene(engine::Application& app, GameSession& session)
    : Scene(app), s_(session) {}

Rectangle JournalScene::PanelRect() const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    return {w / 2.0f - 480.0f, h / 2.0f - 310.0f, 960.0f, 620.0f};
}

std::vector<Rectangle> JournalScene::TabRects() const {
    const Rectangle panel = PanelRect();
    std::vector<Rectangle> rects;
    for (std::size_t i = 0; i < kTabs.size(); ++i) {
        rects.push_back({panel.x + 24.0f + F(static_cast<int>(i)) * 150.0f, panel.y + 20.0f, 140.0f, 44.0f});
    }
    return rects;
}

void JournalScene::DrawLife(Rectangle area) const {
    const game::LifeWheel wheel = game::ComputeLifeWheel(s_.state, s_.content);
    ui::Text("Your Life Wheel", {area.x, area.y}, 24.0f, kInk, true);
    ui::TextWrapped("There's no single score. A good life has many parts - try to keep the wheel round.",
                    {area.x, area.y + 34.0f, 400.0f, 60.0f}, 17.0f, kMuted);
    DrawLifeWheel({area.x + 210.0f, area.y + 270.0f}, 132.0f, wheel, true);

    const float x = area.x + 480.0f;
    ui::Text("Moments that mattered", {x, area.y}, 24.0f, kInk, true);
    float y = area.y + 40.0f;
    if (s_.state.memories.empty()) {
        ui::Text("Your story is just beginning.", {x, y}, 19.0f, kMuted);
    }
    // Newest first, as many as fit.
    for (auto it = s_.state.memories.rbegin(); it != s_.state.memories.rend() && y < area.y + area.height - 40.0f; ++it) {
        ui::Text("Day " + std::to_string(it->day), {x, y}, 15.0f, kAccent, true);
        y += 20.0f;
        y += ui::TextWrapped(it->text, {x, y, area.width - 480.0f, 50.0f}, 18.0f, kInk) + 6.0f;
    }
}

void JournalScene::Update(float /*dt*/) {
    if (IsKeyPressed(KEY_J) || IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE)) {
        App().Scenes().Pop();
        return;
    }
    const int count = static_cast<int>(kTabs.size());
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        tab_ = (tab_ + 1) % count;
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        tab_ = (tab_ + count - 1) % count;
    }
    const std::vector<Rectangle> tabs = TabRects();
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        if (ui::Clicked(tabs[i])) {
            tab_ = static_cast<int>(i);
        }
    }
}

void JournalScene::DrawQuests(Rectangle area) const {
    float y = area.y;
    bool any = false;
    for (const bool done : {false, true}) {
        for (const game::QuestDef& quest : s_.content.quests) {
            const game::QuestStatus status = game::GetQuestStatus(quest, s_.state);
            if (status == game::QuestStatus::Hidden || (status == game::QuestStatus::Done) != done) {
                continue;
            }
            any = true;
            ui::Text(quest.title + (done ? "  (complete)" : ""), {area.x, y}, 24.0f,
                     done ? kGood : kInk, true);
            y += 30.0f;
            if (!done) {
                y += ui::TextWrapped(quest.summary, {area.x, y, area.width, 60.0f}, 18.0f, kMuted);
                const game::QuestStep* current = game::CurrentStep(quest, s_.state);
                for (const game::QuestStep& step : quest.steps) {
                    const bool stepDone = s_.state.HasFlag(step.doneFlag);
                    const bool isCurrent = &step == current;
                    if (!stepDone && !isCurrent) {
                        continue;  // future steps stay a surprise
                    }
                    const Vector2 box{area.x + 10.0f, y + 4.0f};
                    DrawRectangleLinesEx({box.x, box.y, 16.0f, 16.0f}, 2.0f, stepDone ? kGood : kAccent);
                    if (stepDone) {
                        DrawLineEx({box.x + 3.0f, box.y + 8.0f}, {box.x + 7.0f, box.y + 13.0f}, 2.5f, kGood);
                        DrawLineEx({box.x + 7.0f, box.y + 13.0f}, {box.x + 14.0f, box.y + 3.0f}, 2.5f, kGood);
                    }
                    y += ui::TextWrapped(step.text, {area.x + 36.0f, y, area.width - 36.0f, 60.0f}, 20.0f,
                                         stepDone ? kMuted : kInk);
                }
            }
            y += 14.0f;
        }
    }
    if (!any) {
        ui::Text("No quests yet. Talk to people around town!", {area.x, y}, 22.0f, kMuted);
    }
}

void JournalScene::DrawBag(Rectangle area) const {
    const game::GameState& st = s_.state;
    DrawCoin({area.x + 12.0f, area.y + 14.0f}, 11.0f);
    ui::Text(std::to_string(st.Get("money")) + " coins", {area.x + 32.0f, area.y}, 26.0f, kInk, true);
    ui::Text("Reputation " + std::to_string(st.Get("reputation")) + " / 100",
             {area.x + 260.0f, area.y + 4.0f}, 22.0f, kMuted);

    float y = area.y + 52.0f;
    ui::Text("Stock for the pushcart", {area.x, y}, 22.0f, kAccent, true);
    y += 34.0f;
    if (st.stock.empty()) {
        ui::Text("Nothing yet. Buy stock at Lin's Wholesale.", {area.x, y}, 20.0f, kMuted);
        return;
    }
    for (const game::StockBatch& batch : st.stock) {
        const game::ItemDef* item = s_.content.FindItem(batch.item);
        const std::string name = item ? item->name : batch.item;
        const int daysLeft = (item ? item->shelfDays : 30) - (st.day - batch.dayBought);
        std::string freshness = daysLeft <= 1 ? "goes off tonight"
                                              : "fresh for " + std::to_string(daysLeft) + " more days";
        if (batch.dodgy) {
            freshness = "from Smiley Steve...";
        }
        DrawCircleV({area.x + 10.0f, y + 12.0f}, 8.0f, item ? ToColor(item->color) : GRAY);
        ui::Text(std::to_string(batch.qty) + " x " + name, {area.x + 28.0f, y}, 22.0f, kInk, true);
        ui::Text("cost " + std::to_string(batch.unitCost) + " each, bought Day " +
                     std::to_string(batch.dayBought) + ", " + freshness,
                 {area.x + 230.0f, y + 2.0f}, 19.0f, daysLeft <= 1 ? kBad : kMuted);
        y += 34.0f;
    }
}

void JournalScene::DrawSkills(Rectangle area) const {
    float y = area.y;
    ui::Text("Skills grow when you use them.", {area.x, y}, 20.0f, kMuted);
    y += 40.0f;
    for (const SkillInfo& skill : kSkills) {
        const int level = s_.state.Get(skill.id);
        ui::Text(skill.name, {area.x, y}, 24.0f, kInk, true);
        ui::Text(std::to_string(level) + " / 10", {area.x + 220.0f, y + 2.0f}, 20.0f, kAccent, true);
        ui::Bar({area.x + 320.0f, y + 6.0f, 360.0f, 14.0f}, F(level) / 10.0f, kAccent,
                Color{230, 220, 205, 255});
        ui::Text(skill.use, {area.x, y + 30.0f}, 18.0f, kMuted);
        y += 70.0f;
    }
}

void JournalScene::DrawPeople(Rectangle area) const {
    float y = area.y;
    int shown = 0;
    for (const game::NpcDef& npc : s_.content.npcs) {
        if (!npc.relationship || !game::HasMet(npc, s_.state, s_.content)) {
            continue;
        }
        ++shown;
        DrawPortrait({area.x + 30.0f, y + 32.0f}, 28.0f, LookOf(&npc));
        ui::Text(npc.name, {area.x + 74.0f, y + 4.0f}, 24.0f, kInk, true);
        if (s_.state.InParty(npc.id)) {
            ui::Text("on your team", {area.x + 74.0f, y + 34.0f}, 16.0f, kGood, true);
        }
        const std::array<std::pair<const char*, const char*>, 3> dims = {
            {{"trust", "Trust"}, {"fun", "Fun"}, {"respect", "Respect"}}};
        float x = area.x + 240.0f;
        for (const auto& [key, label] : dims) {
            const int value = s_.state.Get(npc.id + "." + key);
            ui::Text(label, {x, y + 4.0f}, 18.0f, kMuted);
            ui::Bar({x, y + 30.0f, 180.0f, 12.0f}, F(value) / 100.0f, kGood, Color{230, 220, 205, 255});
            x += 210.0f;
        }
        y += 74.0f;
    }
    if (shown == 0) {
        ui::Text("You haven't met anyone yet.", {area.x, y}, 22.0f, kMuted);
    }
}

void JournalScene::Draw(float /*alpha*/) const {
    DrawRectangleRec({0.0f, 0.0f, F(GetScreenWidth()), F(GetScreenHeight())}, Color{20, 12, 20, 140});
    const Rectangle panel = PanelRect();
    ui::Panel(panel, kPaper, kPanelBorder, 3.0f);

    const std::vector<Rectangle> tabs = TabRects();
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        ui::DrawButton(tabs[i], kTabs[i], static_cast<int>(i) == tab_, true, 22.0f);
    }
    ui::Text("J / Esc to close   Left / Right to switch pages", {panel.x + 36.0f, panel.y + panel.height - 36.0f},
             18.0f, kMuted);

    const Rectangle area{panel.x + 36.0f, panel.y + 90.0f, panel.width - 72.0f, panel.height - 120.0f};
    switch (tab_) {
        case 0: DrawQuests(area); break;
        case 1: DrawBag(area); break;
        case 2: DrawSkills(area); break;
        case 3: DrawPeople(area); break;
        default: DrawLife(area); break;
    }
}
