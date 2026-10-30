#include "Visuals.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "engine/Ui.h"

namespace visuals {

namespace ui = engine::ui;

Look LookOf(const game::NpcDef* npc) {
    if (npc == nullptr) {
        return PlayerLook();
    }
    return {ToColor(npc->body), ToColor(npc->hair), ToColor(npc->skin)};
}

Look PlayerLook(int generation) {
    if (generation >= 2) {
        return {Color{64, 192, 87, 255}, Color{43, 43, 43, 255}, Color{198, 142, 95, 255}};
    }
    return {Color{245, 166, 35, 255}, Color{70, 45, 30, 255}, Color{226, 178, 132, 255}};
}

void DrawCharacter(Vector2 feet, const Look& look, const Pose& pose, float scale) {
    const float s = scale;
    DrawEllipse(static_cast<int>(feet.x), static_cast<int>(feet.y), 9.0f * s, 3.5f * s, Color{0, 0, 0, 60});

    const float top = feet.y - 6.0f * s + pose.bob;
    const float swing = pose.walking ? std::sin(pose.step) : 0.0f;
    const Color legs{60, 60, 80, 255};
    const Color sleeve = ColorLerp(look.body, BLACK, 0.15f);

    // Legs step forwards and back; the lifted leg is a little shorter.
    const float leftLift = std::max(0.0f, swing) * 2.0f * s;
    const float rightLift = std::max(0.0f, -swing) * 2.0f * s;
    DrawRectangleRec({feet.x - 5.0f * s + swing * 1.5f * s, top - 2.0f * s, 4.0f * s, 8.0f * s - leftLift}, legs);
    DrawRectangleRec({feet.x + 1.0f * s - swing * 1.5f * s, top - 2.0f * s, 4.0f * s, 8.0f * s - rightLift}, legs);

    // Arms swing opposite to the legs.
    const Rectangle leftArm{feet.x - 10.5f * s, top - 14.0f * s - swing * 1.5f * s, 3.5f * s, 10.0f * s};
    const Rectangle rightArm{feet.x + 7.0f * s, top - 14.0f * s + swing * 1.5f * s, 3.5f * s, 10.0f * s};
    DrawRectangleRounded(leftArm, 1.0f, 4, sleeve);
    DrawRectangleRounded(rightArm, 1.0f, 4, sleeve);
    DrawCircleV({leftArm.x + 1.75f * s, leftArm.y + leftArm.height}, 1.8f * s, look.skin);
    DrawCircleV({rightArm.x + 1.75f * s, rightArm.y + rightArm.height}, 1.8f * s, look.skin);

    // Body
    const Rectangle body{feet.x - 8.0f * s, top - 16.0f * s, 16.0f * s, 16.0f * s};
    DrawRectangleRounded(body, 0.5f, 6, look.body);

    // Head: eyes look the way the character faces, and blink every few seconds.
    const Vector2 head{feet.x, top - 22.0f * s};
    DrawCircleV(head, 8.0f * s, look.skin);
    DrawCircleSector(head, 8.5f * s, 180.0f, 360.0f, 12, look.hair);
    const float lookX = pose.facing * 2.2f * s;
    const bool blinking = std::fmod(static_cast<float>(GetTime()) + pose.seed * 1.37f, 4.2f) < 0.13f;
    for (const float side : {-3.0f, 3.0f}) {
        const Vector2 eye{head.x + side * s + lookX, head.y + 1.0f * s};
        if (blinking) {
            DrawRectangleRec({eye.x - 1.4f * s, eye.y - 0.3f * s, 2.8f * s, 0.8f * s}, kInk);
        } else {
            DrawCircleV(eye, 1.2f * s, kInk);
        }
    }
}

void DrawCharacter(Vector2 feet, const Look& look, float bob, float scale) {
    Pose pose;
    pose.walking = bob != 0.0f;
    pose.step = static_cast<float>(GetTime()) * 14.0f;
    pose.bob = bob;
    pose.seed = feet.x * 0.37f;
    DrawCharacter(feet, look, pose, scale);
}

void DrawPortrait(Vector2 center, float radius, const Look& look) {
    DrawCircleV(center, radius, Color{255, 236, 210, 255});
    DrawCircleLinesV(center, radius, kPanelBorder);
    const float r = radius;
    // Shoulders
    DrawCircleSector({center.x, center.y + r * 0.95f}, r * 0.75f, 180.0f, 360.0f, 24, look.body);
    // Head and hair
    const Vector2 head{center.x, center.y - r * 0.12f};
    DrawCircleV(head, r * 0.42f, look.skin);
    DrawCircleSector(head, r * 0.45f, 180.0f, 360.0f, 24, look.hair);
    DrawCircleV({head.x - r * 0.15f, head.y + r * 0.05f}, r * 0.05f, kInk);
    DrawCircleV({head.x + r * 0.15f, head.y + r * 0.05f}, r * 0.05f, kInk);
    DrawCircleSector({head.x, head.y + r * 0.16f}, r * 0.12f, 0.0f, 180.0f, 12, kInk);
}

void DrawPushcart(Vector2 center, float scale, bool fancySign) {
    const float s = scale;
    // Wheels
    DrawCircleV({center.x - 14.0f * s, center.y + 12.0f * s}, 6.0f * s, Color{60, 50, 45, 255});
    DrawCircleV({center.x + 14.0f * s, center.y + 12.0f * s}, 6.0f * s, Color{60, 50, 45, 255});
    // Cart body
    DrawRectangleRounded({center.x - 22.0f * s, center.y - 8.0f * s, 44.0f * s, 20.0f * s}, 0.2f, 6,
                         Color{170, 110, 60, 255});
    DrawRectangleRec({center.x - 22.0f * s, center.y - 2.0f * s, 44.0f * s, 3.0f * s},
                     Color{140, 85, 45, 255});
    // Poles and striped awning
    DrawRectangleRec({center.x - 20.0f * s, center.y - 30.0f * s, 2.5f * s, 22.0f * s},
                     Color{120, 80, 50, 255});
    DrawRectangleRec({center.x + 17.5f * s, center.y - 30.0f * s, 2.5f * s, 22.0f * s},
                     Color{120, 80, 50, 255});
    for (int i = 0; i < 6; ++i) {
        const float x = center.x - 24.0f * s + static_cast<float>(i) * 8.0f * s;
        const Color stripe = i % 2 == 0 ? Color{220, 70, 60, 255} : Color{250, 245, 235, 255};
        DrawRectangleRec({x, center.y - 36.0f * s, 8.0f * s, 8.0f * s}, stripe);
        DrawTriangle({x, center.y - 28.0f * s}, {x + 4.0f * s, center.y - 24.0f * s},
                     {x + 8.0f * s, center.y - 28.0f * s}, stripe);
    }
    if (fancySign) {
        const Rectangle sign{center.x - 20.0f * s, center.y - 52.0f * s, 40.0f * s, 13.0f * s};
        DrawRectangleRounded(sign, 0.3f, 6, Color{255, 240, 120, 255});
        DrawRectangleRoundedLinesEx(sign, 0.3f, 6, 1.0f * s, Color{90, 160, 230, 255});
        ui::TextCentered("YUM!", {sign.x + sign.width / 2.0f, sign.y + sign.height / 2.0f},
                         9.0f * s, Color{200, 60, 120, 255}, true);
        DrawCircleV({sign.x + 4.0f * s, sign.y + 4.0f * s}, 1.5f * s, Color{90, 160, 230, 255});
        DrawCircleV({sign.x + sign.width - 4.0f * s, sign.y + 4.0f * s}, 1.5f * s,
                    Color{90, 200, 120, 255});
    }
}

void DrawStall(Rectangle rect, Color awning, const std::string& label) {
    const float awningH = rect.height * 0.45f;
    // Poles
    DrawRectangleRec({rect.x + 3.0f, rect.y + awningH - 4.0f, 4.0f, rect.height - awningH}, Color{120, 80, 50, 255});
    DrawRectangleRec({rect.x + rect.width - 7.0f, rect.y + awningH - 4.0f, 4.0f, rect.height - awningH},
                     Color{120, 80, 50, 255});
    // Counter with goods
    const Rectangle counter{rect.x, rect.y + rect.height * 0.62f, rect.width, rect.height * 0.38f};
    DrawRectangleRec({counter.x + 4.0f, counter.y + 6.0f, counter.width, counter.height}, Color{0, 0, 0, 45});
    DrawRectangleRec(counter, Color{176, 116, 66, 255});
    DrawRectangleRec({counter.x, counter.y, counter.width, 5.0f}, Color{206, 146, 90, 255});
    for (float gx = counter.x + 10.0f; gx < counter.x + counter.width - 8.0f; gx += 14.0f) {
        DrawCircleV({gx, counter.y - 3.0f}, 5.0f, ColorLerp(awning, WHITE, 0.25f));
    }
    // Striped awning
    const int stripes = std::max(3, static_cast<int>(rect.width / 16.0f));
    const float stripeW = (rect.width + 8.0f) / static_cast<float>(stripes);
    for (int i = 0; i < stripes; ++i) {
        const float x = rect.x - 4.0f + static_cast<float>(i) * stripeW;
        const Color c = i % 2 == 0 ? awning : Color{250, 245, 235, 255};
        DrawRectangleRec({x, rect.y, stripeW, awningH}, c);
        DrawTriangle({x, rect.y + awningH}, {x + stripeW / 2.0f, rect.y + awningH + 6.0f},
                     {x + stripeW, rect.y + awningH}, c);
    }
    if (!label.empty()) {
        const float size = 11.0f;
        const Vector2 m = ui::Measure(label, size, true);
        const Rectangle plate{rect.x + rect.width / 2.0f - m.x / 2.0f - 6.0f, rect.y + awningH / 2.0f - 9.0f,
                              m.x + 12.0f, 18.0f};
        DrawRectangleRounded(plate, 0.3f, 6, Color{255, 248, 230, 255});
        ui::Text(label, {plate.x + 6.0f, plate.y + 3.0f}, size, kInk, true);
    }
}

void DrawSign(Vector2 feet, const std::string& text) {
    DrawRectangleRec({feet.x - 2.0f, feet.y - 30.0f, 4.0f, 30.0f}, Color{120, 80, 50, 255});
    const float size = 11.0f;
    const Vector2 m = ui::Measure(text, size, true);
    const Rectangle board{feet.x - m.x / 2.0f - 8.0f, feet.y - 46.0f, m.x + 16.0f, 22.0f};
    DrawRectangleRounded(board, 0.2f, 6, Color{214, 168, 110, 255});
    DrawRectangleRoundedLinesEx(board, 0.2f, 6, 1.5f, Color{120, 80, 50, 255});
    ui::Text(text, {board.x + 8.0f, board.y + 5.0f}, size, kInk, true);
}

void DrawHeart(Vector2 center, float size, Color color) {
    DrawCircleV({center.x - size * 0.25f, center.y - size * 0.1f}, size * 0.3f, color);
    DrawCircleV({center.x + size * 0.25f, center.y - size * 0.1f}, size * 0.3f, color);
    DrawTriangle({center.x - size * 0.53f, center.y}, {center.x, center.y + size * 0.55f},
                 {center.x + size * 0.53f, center.y}, color);
}

void DrawRain(Rectangle area, float time) {
    DrawRectangleRec(area, Color{40, 60, 90, 55});
    const Color drop{205, 220, 240, 150};
    const int drops = static_cast<int>(area.width * area.height / 7000.0f);
    const float fall = area.height + 40.0f;
    for (int i = 0; i < drops; ++i) {
        // Each drop has its own column, start height and speed, and wraps around the area.
        const float n = static_cast<float>(i);
        const float column = std::fmod(n * 0.618034f, 1.0f);
        const float start = std::fmod(n * 0.754878f, 1.0f);
        const float speed = 620.0f + 260.0f * std::fmod(n * 0.414214f, 1.0f);
        const float t = std::fmod(start * fall + time * speed, fall);
        float x = area.x + column * area.width - t * 0.2f;
        if (x < area.x) {
            x += area.width;
        }
        const float y = area.y - 20.0f + t;
        DrawLineEx({x, y}, {x - 3.0f, y + 14.0f}, 1.5f, drop);
    }
}

void DrawRainCloud(Vector2 center, float time) {
    const Color drop{70, 130, 210, 255};
    for (int i = 0; i < 3; ++i) {
        const float x = center.x - 7.0f + static_cast<float>(i) * 7.0f;
        const float y = center.y + 6.0f + std::fmod(time * 14.0f + static_cast<float>(i) * 4.0f, 9.0f);
        DrawLineEx({x, y}, {x - 1.5f, y + 4.0f}, 2.0f, drop);
    }
    const Color cloud{150, 160, 175, 255};
    DrawCircleV({center.x - 5.0f, center.y + 1.0f}, 7.0f, cloud);
    DrawCircleV({center.x + 4.0f, center.y - 2.0f}, 9.0f, cloud);
    DrawRectangleRec({center.x - 5.0f, center.y + 1.0f, 12.0f, 7.0f}, cloud);
}

void DrawLifeWheel(Vector2 center, float radius, const game::LifeWheel& wheel, bool labels) {
    const std::array<std::pair<const char*, int>, 6> axes = {{{"Money", wheel.money},
                                                              {"Health", wheel.health},
                                                              {"Happiness", wheel.happiness},
                                                              {"Relationships", wheel.relationships},
                                                              {"Knowledge", wheel.knowledge},
                                                              {"Community", wheel.community}}};
    auto corner = [&](std::size_t i, float r) {
        const float angle = -PI / 2.0f + static_cast<float>(i) * PI / 3.0f;
        return Vector2{center.x + std::cos(angle) * r, center.y + std::sin(angle) * r};
    };
    // Rings
    for (int ring = 1; ring <= 4; ++ring) {
        const float r = radius * static_cast<float>(ring) / 4.0f;
        for (std::size_t i = 0; i < axes.size(); ++i) {
            DrawLineEx(corner(i, r), corner((i + 1) % axes.size(), r), 1.0f, Color{200, 185, 165, 255});
        }
    }
    for (std::size_t i = 0; i < axes.size(); ++i) {
        DrawLineEx(center, corner(i, radius), 1.0f, Color{200, 185, 165, 255});
    }
    // Filled shape (drawn in both windings so it shows whatever the culling).
    const Color fill = Fade(kAccent, 0.35f);
    for (std::size_t i = 0; i < axes.size(); ++i) {
        const std::size_t j = (i + 1) % axes.size();
        const Vector2 a = corner(i, radius * static_cast<float>(axes[i].second) / 100.0f);
        const Vector2 b = corner(j, radius * static_cast<float>(axes[j].second) / 100.0f);
        DrawTriangle(center, a, b, fill);
        DrawTriangle(center, b, a, fill);
    }
    for (std::size_t i = 0; i < axes.size(); ++i) {
        const std::size_t j = (i + 1) % axes.size();
        const Vector2 a = corner(i, radius * static_cast<float>(axes[i].second) / 100.0f);
        const Vector2 b = corner(j, radius * static_cast<float>(axes[j].second) / 100.0f);
        DrawLineEx(a, b, 3.0f, kAccent);
        DrawCircleV(a, 4.0f, kAccent);
    }
    if (!labels) {
        return;
    }
    for (std::size_t i = 0; i < axes.size(); ++i) {
        const Vector2 p = corner(i, radius + 30.0f);
        ui::TextCentered(axes[i].first, {p.x, p.y - 9.0f}, 18.0f, kInk, true);
        ui::TextCentered(std::to_string(axes[i].second), {p.x, p.y + 11.0f}, 16.0f, kMuted);
    }
}

void DrawCoin(Vector2 center, float radius) {
    DrawCircleV(center, radius, kGold);
    DrawCircleLinesV(center, radius, Color{180, 130, 30, 255});
    DrawCircleV({center.x - radius * 0.3f, center.y - radius * 0.3f}, radius * 0.25f,
                Color{255, 235, 150, 255});
}

void DrawToasts(const GameSession& session) {
    const float centerX = static_cast<float>(GetScreenWidth()) / 2.0f;
    float y = 130.0f;
    for (const Toast& toast : session.toasts) {
        const float alpha = std::min(1.0f, toast.time / 0.4f);
        const Vector2 m = ui::Measure(toast.text, 20.0f);
        const Rectangle r{centerX - m.x / 2.0f - 16.0f, y, m.x + 32.0f, 36.0f};
        ui::Panel(r, Fade(Color{50, 40, 46, 255}, 0.85f * alpha), Fade(kGold, alpha), 2.0f);
        ui::Text(toast.text, {r.x + 16.0f, r.y + 8.0f}, 20.0f, Fade(WHITE, alpha));
        y += 42.0f;
    }
}

}  // namespace visuals
