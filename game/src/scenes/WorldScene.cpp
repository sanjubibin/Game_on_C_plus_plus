#include "scenes/WorldScene.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "Flow.h"
#include "Visuals.h"
#include "engine/Application.h"
#include "engine/Ui.h"
#include "logic/Rules.h"
#include "scenes/JournalScene.h"
#include "scenes/PauseScene.h"

namespace ui = engine::ui;
using namespace visuals;

namespace {

constexpr float kSecondsPerGameMinute = 0.35f;
constexpr float kWalkSpeed = 150.0f;
constexpr float kRunSpeed = 230.0f;
constexpr float kTiredSpeedFactor = 0.8f;

float F(int value) {
    return static_cast<float>(value);
}

float Distance(Vector2 a, Vector2 b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

Rectangle FeetBox(Vector2 feet) {
    return {feet.x - 7.0f, feet.y - 5.0f, 14.0f, 7.0f};
}

Vector2 TileFeet(float tx, float ty) {
    return {(tx + 0.5f) * kTile, (ty + 0.5f) * kTile + 8.0f};
}

Rectangle BuildingRect(const game::Building& b) {
    return {F(b.x) * kTile, F(b.y) * kTile, F(b.w) * kTile, F(b.h) * kTile};
}

// A market stall spot covers three tiles across and two down, centred on the spot.
Rectangle StallRect(const game::Spot& spot) {
    return {F(spot.x - 1) * kTile, F(spot.y) * kTile, 3.0f * kTile, 2.0f * kTile};
}

std::uint32_t Hash(int x, int y) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 73856093u ^ static_cast<std::uint32_t>(y) * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    return h ^ (h >> 15);
}

Color Shade(Color c, int delta) {
    auto channel = [delta](unsigned char v) {
        return static_cast<unsigned char>(std::clamp(static_cast<int>(v) + delta, 0, 255));
    };
    return Color{channel(c.r), channel(c.g), channel(c.b), c.a};
}

// "LIN'S WHOLESALE" -> "Lin's Wholesale"
std::string TitleCase(const std::string& text) {
    std::string out = text;
    bool startOfWord = true;
    for (char& ch : out) {
        const auto u = static_cast<unsigned char>(ch);
        ch = static_cast<char>(startOfWord ? std::toupper(u) : std::tolower(u));
        startOfWord = ch == ' ';
    }
    return out;
}

}  // namespace

WorldScene::WorldScene(engine::Application& app, GameSession& session)
    : Scene(app), s_(session), prevPos_(session.playerPos) {}

const game::MapDef& WorldScene::Map() const {
    const game::MapDef* map = s_.content.FindMap(s_.currentMap);
    return map ? *map : s_.content.StartMap();
}

// --- Simulation -------------------------------------------------------------------------------

bool WorldScene::Blocked(Rectangle feet) const {
    const game::MapDef& map = Map();
    const int x0 = static_cast<int>(std::floor(feet.x / kTile));
    const int x1 = static_cast<int>(std::floor((feet.x + feet.width - 0.01f) / kTile));
    const int y0 = static_cast<int>(std::floor(feet.y / kTile));
    const int y1 = static_cast<int>(std::floor((feet.y + feet.height - 0.01f) / kTile));
    for (int ty = y0; ty <= y1; ++ty) {
        for (int tx = x0; tx <= x1; ++tx) {
            if (game::MapDef::IsSolid(map.TileAt(tx, ty))) {
                return true;
            }
        }
    }
    for (const game::Building& b : map.buildings) {
        if (CheckCollisionRecs(feet, BuildingRect(b))) {
            return true;
        }
    }
    for (const game::Spot& spot : map.spots) {
        if (spot.draw == "stall" && game::CheckCondition(spot.when, s_.state) &&
            CheckCollisionRecs(feet, StallRect(spot))) {
            return true;
        }
    }
    return false;
}

void WorldScene::FixedUpdate(float dt) {
    game::GameState& st = s_.state;
    prevPos_ = s_.playerPos;

    Vector2 dir{0.0f, 0.0f};
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) dir.y -= 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) dir.y += 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) dir.x -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir.x += 1.0f;
    const float length = std::hypot(dir.x, dir.y);
    moving_ = length > 0.0f;
    if (moving_) {
        const bool run = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const float tired = game::IsTired(st) ? kTiredSpeedFactor : 1.0f;
        const float step = (run ? kRunSpeed : kWalkSpeed) * tired * dt;
        Vector2 pos = s_.playerPos;
        const Vector2 tryX{pos.x + dir.x / length * step, pos.y};
        if (!Blocked(FeetBox(tryX))) {
            pos.x = tryX.x;
        }
        const Vector2 tryY{pos.x, pos.y + dir.y / length * step};
        if (!Blocked(FeetBox(tryY))) {
            pos.y = tryY.y;
        }
        s_.playerPos = pos;
        walkTime_ += dt;
        if (s_.trail.empty() || Distance(s_.trail.front(), pos) >= 3.0f) {
            s_.trail.insert(s_.trail.begin(), pos);
            if (s_.trail.size() > 400) {
                s_.trail.pop_back();
            }
        }
    }
    CheckExits();

    // The clock only runs while you are out and about.
    clockAccum_ += dt;
    while (clockAccum_ >= kSecondsPerGameMinute) {
        clockAccum_ -= kSecondsPerGameMinute;
        ++st.minutes;
    }
    if (st.minutes >= 18 * 60 && !st.HasFlag("today.dinner_call")) {
        st.SetFlag("today.dinner_call");
        s_.AddToast("Mom: Dinner's at home whenever you're ready!");
    }
    if (st.minutes < 20 * 60) {
        eveningQueued_ = false;
    } else if (!eveningQueued_) {
        eveningQueued_ = true;
        s_.AddToast("It's dark - you head home, exhausted.");
        flow::StartEvening(App(), s_, true);
    }
}

void WorldScene::CheckExits() {
    const Vector2 p = s_.playerPos;
    const int tx = static_cast<int>(std::floor(p.x / kTile));
    const int ty = static_cast<int>(std::floor(p.y / kTile));
    for (const game::Exit& exit : Map().exits) {
        if (tx >= exit.x && tx < exit.x + exit.w && ty >= exit.y && ty < exit.y + exit.h) {
            flow::TravelTo(App(), s_, exit.to, exit.toX, exit.toY);
            prevPos_ = s_.playerPos;
            return;
        }
    }
}

void WorldScene::Update(float dt) {
    s_.UpdateToasts(dt);
    target_ = FindTarget();

    if (ui::ConfirmPressed() && target_.kind != TargetKind::None) {
        Interact(target_);
        return;
    }
    if (IsKeyPressed(KEY_M)) {
        s_.settings.music = !s_.settings.music;
        ApplySettings(s_.settings);
        SaveSettings(s_.settingsPath, s_.settings);
        s_.AddToast(s_.settings.music ? "Music on" : "Music off");
    }
    if (IsKeyPressed(KEY_J) || IsKeyPressed(KEY_TAB)) {
        App().Scenes().Push(std::make_unique<JournalScene>(App(), s_));
    } else if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
        App().Scenes().Push(std::make_unique<PauseScene>(App(), s_));
    }
}

bool WorldScene::NpcPresent(const game::NpcDef& npc) const {
    return npc.map == s_.currentMap && !s_.state.InParty(npc.id) &&
           game::CheckCondition(npc.presentWhen, s_.state);
}

bool WorldScene::HasNewStory(const game::NpcDef& npc) const {
    const std::string event = game::PickTalkEvent(npc, s_.state);
    return !event.empty() && !event.starts_with("idle.") && !s_.state.seenEvents.contains(event);
}

Vector2 WorldScene::FollowerPos(std::size_t index) const {
    const std::size_t spacing = 12;
    const std::size_t wanted = (index + 1) * spacing;
    if (s_.trail.size() > wanted) {
        return s_.trail[wanted];
    }
    if (!s_.trail.empty() && s_.trail.size() > index * spacing + 4) {
        return s_.trail.back();
    }
    return {s_.playerPos.x - 20.0f * F(static_cast<int>(index) + 1), s_.playerPos.y + 2.0f};
}

WorldScene::Target WorldScene::FindTarget() const {
    Target best;
    float bestDistance = 1e9f;
    const Vector2 p = s_.playerPos;
    // bias makes a candidate lose ties: followers are always close by, so they should only be
    // picked when nothing else is in reach.
    auto consider = [&](Target candidate, float reach, float bias = 0.0f) {
        const float d = Distance(p, candidate.point);
        if (d <= reach && d + bias < bestDistance) {
            bestDistance = d + bias;
            best = std::move(candidate);
        }
    };

    for (const game::NpcDef& npc : s_.content.npcs) {
        if (NpcPresent(npc)) {
            consider({TargetKind::Npc, &npc, nullptr, nullptr, TileFeet(npc.x, npc.y), "Talk to " + npc.name},
                     46.0f);
        }
    }
    for (std::size_t i = 0; i < s_.state.party.size(); ++i) {
        if (const game::NpcDef* npc = s_.content.FindNpc(s_.state.party[i])) {
            consider({TargetKind::Npc, npc, nullptr, nullptr, FollowerPos(i), "Talk to " + npc->name}, 30.0f,
                     100.0f);
        }
    }
    const game::MapDef& map = Map();
    for (const game::Building& b : map.buildings) {
        if (b.doorX < 0) {
            continue;
        }
        const Vector2 door{(F(b.doorX) + 0.5f) * kTile, F(b.y + b.h) * kTile + 8.0f};
        std::string label = "Enter " + TitleCase(b.label);
        if (b.action == "home") {
            label = s_.state.Hour() >= 17 ? "Go home for dinner" : "Go inside";
        }
        consider({TargetKind::Door, nullptr, &b, nullptr, door, label}, 30.0f);
    }
    for (const game::Spot& spot : map.spots) {
        if (!game::CheckCondition(spot.when, s_.state)) {
            continue;
        }
        const Vector2 point = spot.draw == "stall"
                                  ? Vector2{(F(spot.x) + 0.5f) * kTile, F(spot.y) * kTile + 4.0f}
                                  : Vector2{(F(spot.x) + 0.5f) * kTile, (F(spot.y) + 0.5f) * kTile + 6.0f};
        consider({TargetKind::Spot, nullptr, nullptr, &spot, point, spot.label}, 42.0f);
    }
    return best;
}

void WorldScene::Interact(const Target& target) {
    switch (target.kind) {
        case TargetKind::Npc:
            flow::TalkTo(App(), s_, *target.npc);
            break;
        case TargetKind::Door:
            flow::RunAction(App(), s_, target.building->action);
            break;
        case TargetKind::Spot:
            flow::RunAction(App(), s_, target.spot->action);
            break;
        case TargetKind::None:
            break;
    }
}

// --- Drawing ----------------------------------------------------------------------------------

Camera2D WorldScene::MakeCamera(Vector2 focus) const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    Camera2D camera{};
    camera.zoom = std::max(1.0f, h / 480.0f);
    camera.offset = {w / 2.0f, h / 2.0f};
    const float halfW = w / 2.0f / camera.zoom;
    const float halfH = h / 2.0f / camera.zoom;
    const float mapW = F(Map().width) * kTile;
    const float mapH = F(Map().height) * kTile;
    camera.target.x = mapW <= 2.0f * halfW ? mapW / 2.0f : std::clamp(focus.x, halfW, mapW - halfW);
    camera.target.y = mapH <= 2.0f * halfH ? mapH / 2.0f : std::clamp(focus.y - 12.0f, halfH, mapH - halfH);
    return camera;
}

void WorldScene::DrawTiles(const Camera2D& camera) const {
    const game::MapDef& map = Map();
    const float halfW = F(GetScreenWidth()) / 2.0f / camera.zoom;
    const float halfH = F(GetScreenHeight()) / 2.0f / camera.zoom;
    const int x0 = std::max(0, static_cast<int>((camera.target.x - halfW) / kTile) - 1);
    const int y0 = std::max(0, static_cast<int>((camera.target.y - halfH) / kTile) - 1);
    const int x1 = std::min(map.width - 1, static_cast<int>((camera.target.x + halfW) / kTile) + 1);
    const int y1 = std::min(map.height - 1, static_cast<int>((camera.target.y + halfH) / kTile) + 1);
    const float time = static_cast<float>(GetTime());

    const Color grass{112, 182, 96, 255};
    const Color path{226, 206, 162, 255};
    const Color road{92, 96, 108, 255};
    const Color water{84, 156, 214, 255};
    const Color plaza{214, 204, 186, 255};

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const char t = map.TileAt(x, y);
            const std::uint32_t h = Hash(x, y);
            const Rectangle r{F(x) * kTile, F(y) * kTile, kTile, kTile};
            const Vector2 c{r.x + kTile / 2.0f, r.y + kTile / 2.0f};

            switch (t) {
                case ':':
                    DrawRectangleRec(r, Shade(path, static_cast<int>(h % 7) - 3));
                    if (h % 4 == 0) {
                        DrawCircleV({r.x + F(static_cast<int>(h % 24)) + 4.0f, r.y + F(static_cast<int>((h >> 8) % 24)) + 4.0f},
                                    1.5f, Shade(path, -30));
                    }
                    break;
                case '=':
                    DrawRectangleRec(r, road);
                    if (map.TileAt(x, y - 1) == '=' && map.TileAt(x, y - 2) != '=') {
                        DrawRectangleRec({r.x + 6.0f, r.y - 2.0f, 20.0f, 4.0f}, Color{235, 230, 210, 255});
                    }
                    break;
                case '~': {
                    DrawRectangleRec(r, water);
                    const float wave = std::sin(time * 1.6f + F(static_cast<int>(h % 100)) * 0.1f) * 3.0f;
                    DrawRectangleRec({r.x + 6.0f + wave, r.y + 10.0f, 10.0f, 2.0f}, Color{170, 215, 245, 255});
                    DrawRectangleRec({r.x + 16.0f - wave, r.y + 22.0f, 9.0f, 2.0f}, Color{170, 215, 245, 255});
                    break;
                }
                case 'c': {
                    // Strawberry field: soil rows with green plants and red berries.
                    DrawRectangleRec(r, Color{150, 110, 72, 255});
                    for (int row = 0; row < 2; ++row) {
                        const float py = r.y + 9.0f + F(row) * 16.0f;
                        DrawRectangleRec({r.x, py - 2.0f, kTile, 5.0f}, Color{126, 90, 58, 255});
                        DrawCircleV({r.x + 8.0f, py - 3.0f}, 5.0f, Color{70, 150, 70, 255});
                        DrawCircleV({r.x + 24.0f, py - 3.0f}, 5.0f, Color{70, 150, 70, 255});
                        if ((h >> row) % 2 == 0) {
                            DrawCircleV({r.x + 10.0f, py}, 2.2f, Color{224, 49, 49, 255});
                            DrawCircleV({r.x + 22.0f, py - 1.0f}, 2.2f, Color{224, 49, 49, 255});
                        }
                    }
                    break;
                }
                case 'k':
                    // Brick wall.
                    DrawRectangleRec(r, Color{150, 82, 66, 255});
                    for (int row = 0; row < 4; ++row) {
                        const float by = r.y + F(row) * 8.0f;
                        const float off = row % 2 == 0 ? 0.0f : 8.0f;
                        DrawLineEx({r.x, by}, {r.x + kTile, by}, 1.0f, Color{120, 64, 52, 255});
                        DrawLineEx({r.x + off + 8.0f, by}, {r.x + off + 8.0f, by + 8.0f}, 1.0f, Color{120, 64, 52, 255});
                        DrawLineEx({r.x + off + 24.0f, by}, {r.x + off + 24.0f, by + 8.0f}, 1.0f,
                                   Color{120, 64, 52, 255});
                    }
                    break;
                case 'p':
                case 'o':
                    DrawRectangleRec(r, Shade(plaza, static_cast<int>(h % 9) - 4));
                    DrawRectangleLinesEx(r, 1.0f, Shade(plaza, -22));
                    if (h % 6 == 0) {
                        DrawRectangleRec({r.x + 4.0f, r.y + 4.0f, 10.0f, 10.0f}, Shade(plaza, -10));
                    }
                    break;
                default:
                    DrawRectangleRec(r, h % 7 == 0 ? Shade(grass, -8) : grass);
                    if (h % 5 == 0) {
                        const float gx = r.x + F(static_cast<int>(h % 22)) + 5.0f;
                        const float gy = r.y + F(static_cast<int>((h >> 6) % 22)) + 6.0f;
                        DrawLineEx({gx, gy}, {gx - 2.0f, gy - 5.0f}, 1.5f, Shade(grass, -28));
                        DrawLineEx({gx + 3.0f, gy}, {gx + 4.0f, gy - 5.0f}, 1.5f, Shade(grass, -28));
                    }
                    break;
            }

            if (t == 'f') {
                const Color petals[3] = {Color{245, 120, 160, 255}, Color{255, 220, 90, 255},
                                         Color{250, 250, 250, 255}};
                for (int i = 0; i < 3; ++i) {
                    const std::uint32_t k = Hash(x * 3 + i, y * 5 - i);
                    const Vector2 fp{r.x + 5.0f + F(static_cast<int>(k % 22)), r.y + 5.0f + F(static_cast<int>((k >> 5) % 22))};
                    DrawCircleV(fp, 3.0f, petals[(h + static_cast<std::uint32_t>(i)) % 3]);
                    DrawCircleV(fp, 1.2f, Color{240, 170, 40, 255});
                }
            } else if (t == '#') {
                DrawRectangleRounded({r.x + 1.0f, r.y + 1.0f, kTile - 2.0f, kTile - 2.0f}, 0.45f, 6,
                                     Color{62, 128, 66, 255});
                DrawCircleV({c.x - 6.0f, c.y - 5.0f}, 3.0f, Color{88, 158, 84, 255});
                DrawCircleV({c.x + 5.0f, c.y + 4.0f}, 3.0f, Color{88, 158, 84, 255});
            } else if (t == 'b') {
                DrawRectangleRec({r.x + 4.0f, r.y + 10.0f, 24.0f, 5.0f}, Color{150, 100, 60, 255});
                DrawRectangleRec({r.x + 4.0f, r.y + 17.0f, 24.0f, 5.0f}, Color{150, 100, 60, 255});
                DrawRectangleRec({r.x + 6.0f, r.y + 22.0f, 3.0f, 6.0f}, Color{90, 70, 50, 255});
                DrawRectangleRec({r.x + 23.0f, r.y + 22.0f, 3.0f, 6.0f}, Color{90, 70, 50, 255});
            } else if (t == 'T') {
                DrawEllipse(static_cast<int>(c.x), static_cast<int>(r.y + 28.0f), 12.0f, 4.0f, Color{0, 0, 0, 50});
                DrawRectangleRec({c.x - 3.0f, c.y + 2.0f, 6.0f, 12.0f}, Color{120, 80, 50, 255});
                DrawCircleV({c.x, c.y - 3.0f}, 15.0f, Color{56, 132, 70, 255});
                DrawCircleV({c.x - 5.0f, c.y - 8.0f}, 8.0f, Color{84, 164, 88, 255});
            }
        }
    }
}

void WorldScene::DrawFountain() const {
    // One round fountain covering all 'o' tiles.
    const game::MapDef& map = Map();
    int minX = map.width;
    int minY = map.height;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < map.height; ++y) {
        for (int x = 0; x < map.width; ++x) {
            if (map.TileAt(x, y) == 'o') {
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
    }
    if (maxX < 0) {
        return;
    }
    const Vector2 c{F(minX + maxX + 1) * kTile / 2.0f, F(minY + maxY + 1) * kTile / 2.0f};
    const float r = F(maxX - minX + 1) * kTile / 2.0f + 6.0f;
    const float time = static_cast<float>(GetTime());
    DrawCircleV({c.x + 3.0f, c.y + 5.0f}, r, Color{0, 0, 0, 40});
    DrawCircleV(c, r, Color{170, 160, 150, 255});
    DrawCircleV(c, r - 6.0f, Color{90, 165, 220, 255});
    for (int i = 0; i < 3; ++i) {
        const float ring = std::fmod(time * 12.0f + F(i) * 10.0f, r - 10.0f);
        DrawCircleLinesV(c, ring + 4.0f, Fade(Color{220, 240, 255, 255}, 1.0f - ring / r));
    }
    DrawCircleV(c, 7.0f, Color{170, 160, 150, 255});
    DrawCircleV({c.x, c.y - 10.0f - std::sin(time * 6.0f) * 2.0f}, 4.0f, Color{200, 230, 255, 220});
}

void WorldScene::DrawBunting() const {
    const game::MapDef& map = Map();
    if (map.decor != "bunting") {
        return;
    }
    const bool festival = s_.state.HasFlag("today.festival");
    const Color colors[5] = {Color{230, 73, 128, 255}, Color{250, 176, 5, 255}, Color{55, 178, 77, 255},
                             Color{34, 139, 230, 255}, Color{132, 94, 247, 255}};
    const float x0 = 3.0f * kTile;
    const float x1 = F(map.width - 3) * kTile;
    for (const float row : {6.6f, 12.4f}) {
        const float y = row * kTile;
        const float sag = 14.0f;
        const int flags = festival ? 36 : 22;
        auto at = [&](float t) {
            return Vector2{x0 + (x1 - x0) * t, y + std::sin(t * PI) * sag};
        };
        for (int i = 0; i < 40; ++i) {
            DrawLineEx(at(F(i) / 40.0f), at(F(i + 1) / 40.0f), 1.5f, Color{90, 70, 60, 255});
        }
        for (int i = 0; i < flags; ++i) {
            const Vector2 a = at((F(i) + 0.15f) / F(flags));
            const Vector2 b = at((F(i) + 0.85f) / F(flags));
            const Vector2 tip{(a.x + b.x) / 2.0f, (a.y + b.y) / 2.0f + 12.0f};
            const Color col = colors[i % 5];
            DrawTriangle(a, tip, b, col);
            DrawTriangle(a, b, tip, col);
            if (festival && i % 3 == 0) {
                const float glow = 0.6f + 0.4f * std::sin(static_cast<float>(GetTime()) * 3.0f + F(i));
                DrawCircleV({tip.x, tip.y + 6.0f}, 4.0f, Fade(Color{255, 230, 120, 255}, glow));
            }
        }
    }
}

void WorldScene::DrawBuildings() const {
    for (const game::Building& b : Map().buildings) {
        const Rectangle r = BuildingRect(b);
        const Color wall = ToColor(b.wall);
        const Color roof = ToColor(b.roof);
        if (b.style == "stall") {
            DrawStall(r, roof, b.label);
            continue;
        }
        const float roofH = std::min(r.height * 0.42f, 56.0f);

        DrawRectangleRec({r.x + 5.0f, r.y + 8.0f, r.width, r.height}, Color{0, 0, 0, 45});
        DrawRectangleRec({r.x, r.y + roofH * 0.5f, r.width, r.height - roofH * 0.5f}, wall);
        DrawRectangleRec({r.x, r.y + r.height - 4.0f, r.width, 4.0f}, Shade(wall, -40));

        // Roof with a darker eave.
        DrawRectangleRec({r.x - 4.0f, r.y, r.width + 8.0f, roofH}, roof);
        for (float sx = r.x; sx < r.x + r.width; sx += 16.0f) {
            DrawRectangleRec({sx, r.y + 6.0f, 2.0f, roofH - 12.0f}, Shade(roof, -18));
        }
        DrawRectangleRec({r.x - 4.0f, r.y + roofH - 6.0f, r.width + 8.0f, 6.0f}, Shade(roof, -45));

        // Windows on every other column, skipping the door.
        for (int col = b.x; col < b.x + b.w; ++col) {
            if (col == b.doorX || (col - b.x) % 2 == 0) {
                continue;
            }
            const Rectangle win{F(col) * kTile + 5.0f, r.y + roofH + 10.0f, 22.0f, 20.0f};
            DrawRectangleRec({win.x - 2.0f, win.y - 2.0f, win.width + 4.0f, win.height + 4.0f}, Shade(wall, -70));
            DrawRectangleRec(win, Color{170, 215, 240, 255});
            DrawRectangleRec({win.x + win.width / 2.0f - 1.0f, win.y, 2.0f, win.height}, Shade(wall, -70));
            DrawRectangleRec({win.x + 3.0f, win.y + 3.0f, 5.0f, 5.0f}, Color{235, 248, 255, 255});
        }

        if (b.doorX >= 0) {
            const Rectangle door{F(b.doorX) * kTile + 5.0f, r.y + r.height - 36.0f, 22.0f, 36.0f};
            DrawRectangleRec({door.x - 2.0f, door.y - 2.0f, door.width + 4.0f, door.height + 2.0f}, Shade(wall, -70));
            DrawRectangleRec(door, Color{130, 82, 50, 255});
            DrawCircleV({door.x + door.width - 5.0f, door.y + door.height / 2.0f}, 2.0f, kGold);
        }

        // Name sign on the roof
        const float signSize = 13.0f;
        const Vector2 m = ui::Measure(b.label, signSize, true);
        const Rectangle sign{r.x + r.width / 2.0f - m.x / 2.0f - 8.0f, r.y + roofH / 2.0f - 12.0f,
                             m.x + 16.0f, 22.0f};
        DrawRectangleRounded(sign, 0.3f, 6, Color{255, 248, 230, 255});
        DrawRectangleRoundedLinesEx(sign, 0.3f, 6, 1.5f, Shade(roof, -50));
        ui::Text(b.label, {sign.x + 8.0f, sign.y + 4.0f}, signSize, Shade(roof, -60), true);
    }
}

void WorldScene::DrawSpots() const {
    for (const game::Spot& spot : Map().spots) {
        if (!game::CheckCondition(spot.when, s_.state)) {
            continue;
        }
        const Vector2 center{(F(spot.x) + 0.5f) * kTile, (F(spot.y) + 0.5f) * kTile};
        if (spot.draw == "cart") {
            DrawPushcart({center.x, center.y - 2.0f}, 1.0f, s_.state.InParty("leo"));
        } else if (spot.draw == "stall") {
            DrawStall(StallRect(spot), kAccent, "THE CORNER SHOP");
        } else if (spot.draw == "sign") {
            DrawSign({center.x, center.y + 12.0f}, "FOR RENT");
        } else if (spot.draw == "signpost") {
            DrawSign({center.x, center.y + 12.0f}, "MARKET ->");
        } else if (spot.draw == "bus") {
            DrawSign({center.x, center.y + 12.0f}, "BUS STOP");
            DrawCircleV({center.x, center.y - 40.0f}, 7.0f, Color{34, 139, 230, 255});
        } else if (spot.draw == "signpost_farm") {
            DrawSign({center.x + 10.0f, center.y + 12.0f}, "<- FARM");
        }
    }
}

void WorldScene::DrawCharacters(Vector2 player) const {
    struct Entry {
        Vector2 feet;
        Look look;
        float bob;
    };
    std::vector<Entry> entries;
    const float time = static_cast<float>(GetTime());

    for (const game::NpcDef& npc : s_.content.npcs) {
        if (NpcPresent(npc)) {
            const float bob = std::sin(time * 2.0f + npc.x) * 0.8f;
            entries.push_back({TileFeet(npc.x, npc.y), LookOf(&npc), bob});
        }
    }
    for (std::size_t i = 0; i < s_.state.party.size(); ++i) {
        const game::NpcDef* npc = s_.content.FindNpc(s_.state.party[i]);
        const float bob = moving_ ? std::sin(walkTime_ * 14.0f + F(static_cast<int>(i))) * 1.5f : 0.0f;
        entries.push_back({FollowerPos(i), LookOf(npc), bob});
    }
    const float playerBob = moving_ ? std::sin(walkTime_ * 14.0f) * 1.5f : 0.0f;
    entries.push_back({player, PlayerLook(), playerBob});

    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.feet.y < b.feet.y; });
    for (const Entry& e : entries) {
        DrawCharacter(e.feet, e.look, e.bob);
    }
}

void WorldScene::DrawOverheads(const Camera2D& camera, Vector2 player) const {
    const float time = static_cast<float>(GetTime());
    for (const game::NpcDef& npc : s_.content.npcs) {
        if (!NpcPresent(npc)) {
            continue;
        }
        const Vector2 feet = TileFeet(npc.x, npc.y);
        const Vector2 head = GetWorldToScreen2D({feet.x, feet.y - 44.0f}, camera);
        if (HasNewStory(npc)) {
            const float bounce = std::sin(time * 5.0f) * 4.0f;
            const Vector2 c{head.x, head.y - 34.0f + bounce};
            DrawCircleV(c, 14.0f, kGold);
            DrawCircleLinesV(c, 14.0f, Color{170, 110, 20, 255});
            ui::TextCentered("!", c, 24.0f, kInk, true);
        }
        if (Distance(player, feet) < 220.0f) {
            const Vector2 m = ui::Measure(npc.name, 16.0f, true);
            const Rectangle pill{head.x - m.x / 2.0f - 8.0f, head.y - 12.0f, m.x + 16.0f, 22.0f};
            DrawRectangleRounded(pill, 1.0f, 8, Color{40, 30, 36, 170});
            ui::Text(npc.name, {pill.x + 8.0f, pill.y + 2.0f}, 16.0f, WHITE, true);
        }
    }
}

void WorldScene::DrawPrompt(const Camera2D& camera) const {
    if (target_.kind == TargetKind::None) {
        return;
    }
    const Vector2 at = GetWorldToScreen2D({target_.point.x, target_.point.y - 62.0f}, camera);
    const Vector2 m = ui::Measure(target_.label, 20.0f, true);
    const Rectangle pill{at.x - (m.x + 52.0f) / 2.0f, at.y - 18.0f, m.x + 52.0f, 36.0f};
    ui::Panel(pill, Color{255, 250, 240, 245}, kAccent, 2.0f);
    const Rectangle key{pill.x + 6.0f, pill.y + 5.0f, 26.0f, 26.0f};
    DrawRectangleRounded(key, 0.3f, 6, kAccent);
    ui::TextCentered("E", {key.x + 13.0f, key.y + 13.0f}, 18.0f, WHITE, true);
    ui::Text(target_.label, {pill.x + 40.0f, pill.y + 7.0f}, 20.0f, kInk, true);
}

void WorldScene::DrawHud() const {
    const game::GameState& st = s_.state;
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());

    // Day, time, weather, money and health.
    const Rectangle info{16.0f, 16.0f, 290.0f, 134.0f};
    ui::Panel(info, kPanel, kPanelBorder);
    ui::Text("Day " + std::to_string(st.day) + "   " + st.ClockText(), {32.0f, 24.0f}, 28.0f, kInk, true);
    const bool hot = st.HasFlag("today.hot");
    const bool festival = st.HasFlag("today.festival");
    if (hot) {
        DrawCircleV({42.0f, 72.0f}, 9.0f, Color{255, 170, 40, 255});
        for (int i = 0; i < 8; ++i) {
            const float a = F(i) * PI / 4.0f;
            DrawLineEx({42.0f + std::cos(a) * 11.0f, 72.0f + std::sin(a) * 11.0f},
                       {42.0f + std::cos(a) * 15.0f, 72.0f + std::sin(a) * 15.0f}, 2.0f, Color{255, 170, 40, 255});
        }
    } else {
        DrawCircleV({38.0f, 74.0f}, 7.0f, Color{200, 205, 215, 255});
        DrawCircleV({47.0f, 71.0f}, 9.0f, Color{200, 205, 215, 255});
    }
    std::string weather = hot ? "Hot day - lemonade weather!" : "Mild day";
    if (festival) {
        weather = "FESTIVAL DAY!";
    }
    ui::Text(weather, {62.0f, 61.0f}, 20.0f, hot || festival ? Color{200, 100, 20, 255} : kMuted, festival);
    DrawCoin({42.0f, 101.0f}, 9.0f);
    ui::Text(std::to_string(st.Get("money")) + " coins", {62.0f, 89.0f}, 22.0f, kInk, true);
    ui::Text("Reputation " + std::to_string(st.Get("reputation")), {170.0f, 91.0f}, 18.0f, kMuted);
    const int health = st.Get("health");
    DrawHeart({42.0f, 129.0f}, 18.0f, game::IsTired(st) ? kBad : Color{230, 90, 110, 255});
    ui::Text(game::IsTired(st) ? "Tired" : "Health", {62.0f, 118.0f}, 18.0f, game::IsTired(st) ? kBad : kMuted);
    ui::Bar({130.0f, 124.0f, 160.0f, 10.0f}, F(health) / 100.0f, game::IsTired(st) ? kBad : kGood,
            Color{230, 220, 205, 255});

    // Quest tracker: the current chapter's quest first.
    if (const game::QuestDef* tracked = game::TrackedQuest(st, s_.content)) {
        const game::QuestDef& quest = *tracked;
        const game::QuestStep* step = game::CurrentStep(quest, st);
        const float panelW = 420.0f;
        const float textW = panelW - 32.0f;
        const float stepH = F(static_cast<int>(ui::Wrap(step->text, textW, 20.0f).size())) * 26.0f;
        const Rectangle panel{w - panelW - 16.0f, 16.0f, panelW, 74.0f + stepH};
        ui::Panel(panel, kPanel, kPanelBorder);
        ui::Text("QUEST", {panel.x + 16.0f, panel.y + 10.0f}, 15.0f, kAccent, true);
        ui::Text(quest.title, {panel.x + 16.0f, panel.y + 28.0f}, 22.0f, kInk, true);
        ui::TextWrapped(step->text, {panel.x + 16.0f, panel.y + 60.0f, textW, stepH}, 20.0f, kInk);
    }

    // Controls hint.
    const std::string hint = Map().name + "      WASD walk   Shift run   E talk / use   J journal   M music   Esc menu";
    const Vector2 m = ui::Measure(hint, 18.0f);
    const Rectangle bar{w / 2.0f - m.x / 2.0f - 16.0f, h - 40.0f, m.x + 32.0f, 30.0f};
    DrawRectangleRounded(bar, 1.0f, 8, Color{40, 30, 36, 150});
    ui::Text(hint, {bar.x + 16.0f, bar.y + 5.0f}, 18.0f, Color{255, 245, 230, 255});
}

void WorldScene::Draw(float alpha) const {
    const Vector2 player{prevPos_.x + (s_.playerPos.x - prevPos_.x) * alpha,
                         prevPos_.y + (s_.playerPos.y - prevPos_.y) * alpha};
    const Camera2D camera = MakeCamera(player);

    ClearBackground(Color{62, 128, 66, 255});
    BeginMode2D(camera);
    DrawTiles(camera);
    DrawFountain();
    DrawBuildings();
    DrawSpots();
    DrawCharacters(player);
    DrawBunting();
    EndMode2D();

    // Evening light: from 17:00 the world slowly turns orange, then dusky purple.
    const float evening = std::clamp((F(s_.state.minutes) - 17.0f * 60.0f) / 180.0f, 0.0f, 1.0f);
    if (evening > 0.0f) {
        const Color dusk = ColorLerp(Color{255, 140, 60, 255}, Color{50, 30, 110, 255}, evening);
        DrawRectangleRec({0.0f, 0.0f, F(GetScreenWidth()), F(GetScreenHeight())}, Fade(dusk, 0.12f + 0.3f * evening));
    }

    DrawOverheads(camera, player);
    DrawPrompt(camera);
    DrawHud();
    DrawToasts(s_);
    // Time-based so it also fades in while a dialogue (like a first-visit scene) is on top.
    const float fade = 1.0f - static_cast<float>(GetTime() - s_.fadeStart) * 2.5f;
    if (fade > 0.0f) {
        DrawRectangleRec({0.0f, 0.0f, F(GetScreenWidth()), F(GetScreenHeight())}, Fade(BLACK, fade));
    }
}
