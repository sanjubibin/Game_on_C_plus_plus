#pragma once

#include <raylib.h>

#include <string>

#include "GameSession.h"
#include "logic/Content.h"
#include "logic/Rules.h"

// Shared look of the game: palette and procedural drawing of characters and props.
namespace visuals {

constexpr float kTile = 32.0f;

constexpr Color kInk{46, 34, 40, 255};
constexpr Color kMuted{128, 112, 102, 255};
constexpr Color kPaper{255, 248, 236, 255};
constexpr Color kPanel{255, 250, 242, 240};
constexpr Color kPanelBorder{150, 110, 80, 255};
constexpr Color kAccent{232, 116, 59, 255};
constexpr Color kGood{62, 150, 82, 255};
constexpr Color kBad{200, 70, 60, 255};
constexpr Color kGold{240, 190, 60, 255};

inline Color ToColor(game::Rgb c, unsigned char alpha = 255) {
    return Color{c.r, c.g, c.b, alpha};
}

struct Look {
    Color body;
    Color hair;
    Color skin;
};

Look LookOf(const game::NpcDef* npc);
Look PlayerLook();

// Feet position in world pixels. bob is a small vertical offset for walking/idle animation.
void DrawCharacter(Vector2 feet, const Look& look, float bob, float scale = 1.0f);
void DrawPortrait(Vector2 center, float radius, const Look& look);
void DrawPushcart(Vector2 center, float scale, bool fancySign);
// A market stall filling rect: striped awning on top, counter with goods below.
void DrawStall(Rectangle rect, Color awning, const std::string& label);
void DrawSign(Vector2 feet, const std::string& text);  // a wooden board on a post
void DrawCoin(Vector2 center, float radius);
void DrawHeart(Vector2 center, float size, Color color);
void DrawToasts(const GameSession& session);

// Six-sided radar chart of the Life Wheel. Values are 0-100.
void DrawLifeWheel(Vector2 center, float radius, const game::LifeWheel& wheel, bool labels);

}  // namespace visuals
