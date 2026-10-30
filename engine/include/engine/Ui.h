#pragma once

#include <raylib.h>

#include <string>
#include <vector>

// Small immediate-mode UI helpers: fonts, text, panels, buttons and keyboard/mouse menus.
// Layout is computed by the caller; Update code checks input, Draw code draws.
namespace engine::ui {

// Loads the first font file that exists from each list; falls back to raylib's built-in font.
void LoadFonts(const std::vector<std::string>& regular, const std::vector<std::string>& bold);
void UnloadFonts();

Vector2 Measure(const std::string& text, float size, bool bold = false);
void Text(const std::string& text, Vector2 pos, float size, Color color, bool bold = false);
void TextCentered(const std::string& text, Vector2 center, float size, Color color,
                  bool bold = false);
std::vector<std::string> Wrap(const std::string& text, float width, float size, bool bold = false);
// Word-wrapped text. maxChars >= 0 shows only that many characters (typewriter effect).
// Returns the height used.
float TextWrapped(const std::string& text, Rectangle box, float size, Color color,
                  int maxChars = -1, bool bold = false);

void Panel(Rectangle rect, Color fill, Color border, float borderThickness = 2.0f);
void DrawButton(Rectangle rect, const std::string& label, bool focused, bool enabled = true,
                float textSize = 22.0f);
void Bar(Rectangle rect, float fraction, Color fill, Color back);

bool Hovered(Rectangle rect);
bool Clicked(Rectangle rect);
bool ConfirmPressed();  // Enter, Space or E

// Keyboard (Up/Down, number keys, Enter/Space/E) and mouse navigation over a list of buttons.
class Menu {
public:
    // Returns the index activated this frame, or -1.
    int Update(const std::vector<Rectangle>& items, const std::vector<bool>& enabled);

    int focused = 0;
};

}  // namespace engine::ui
