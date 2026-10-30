#include "engine/Ui.h"

#include <algorithm>

#include "engine/Audio.h"

namespace engine::ui {

namespace {

Font g_regular{};
Font g_bold{};
bool g_hasRegular = false;
bool g_hasBold = false;

bool TryLoad(const std::vector<std::string>& candidates, int baseSize, Font& out) {
    for (const std::string& path : candidates) {
        if (!FileExists(path.c_str())) {
            continue;
        }
        Font font = LoadFontEx(path.c_str(), baseSize, nullptr, 0);
        // On failure raylib hands back its default font; treat that as "not loaded".
        if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id) {
            GenTextureMipmaps(&font.texture);
            SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
            out = font;
            return true;
        }
    }
    return false;
}

bool HasFont(bool bold) {
    return bold ? (g_hasBold || g_hasRegular) : g_hasRegular;
}

Font FontFor(bool bold) {
    if (bold && g_hasBold) {
        return g_bold;
    }
    if (g_hasRegular) {
        return g_regular;
    }
    return GetFontDefault();
}

float Spacing(float size, bool bold) {
    // raylib's pixel font needs letter spacing; real fonts carry their own.
    return HasFont(bold) ? 0.0f : size / 10.0f;
}

float Roundness(Rectangle rect, float radius) {
    const float shortest = std::max(1.0f, std::min(rect.width, rect.height));
    return std::min(1.0f, 2.0f * radius / shortest);
}

}  // namespace

void LoadFonts(const std::vector<std::string>& regular, const std::vector<std::string>& bold) {
    g_hasRegular = TryLoad(regular, 64, g_regular);
    g_hasBold = TryLoad(bold, 96, g_bold);
}

void UnloadFonts() {
    if (g_hasRegular) {
        UnloadFont(g_regular);
        g_hasRegular = false;
    }
    if (g_hasBold) {
        UnloadFont(g_bold);
        g_hasBold = false;
    }
}

Vector2 Measure(const std::string& text, float size, bool bold) {
    return MeasureTextEx(FontFor(bold), text.c_str(), size, Spacing(size, bold));
}

void Text(const std::string& text, Vector2 pos, float size, Color color, bool bold) {
    DrawTextEx(FontFor(bold), text.c_str(), pos, size, Spacing(size, bold), color);
}

void TextCentered(const std::string& text, Vector2 center, float size, Color color, bool bold) {
    const Vector2 m = Measure(text, size, bold);
    Text(text, {center.x - m.x / 2.0f, center.y - m.y / 2.0f}, size, color, bold);
}

std::vector<std::string> Wrap(const std::string& text, float width, float size, bool bold) {
    std::vector<std::string> lines;
    std::string current;
    std::string word;
    auto pushWord = [&]() {
        if (word.empty()) {
            return;
        }
        const std::string candidate = current.empty() ? word : current + " " + word;
        if (!current.empty() && Measure(candidate, size, bold).x > width) {
            lines.push_back(current);
            current = word;
        } else {
            current = candidate;
        }
        word.clear();
    };
    for (const char ch : text) {
        if (ch == ' ') {
            pushWord();
        } else if (ch == '\n') {
            pushWord();
            lines.push_back(current);
            current.clear();
        } else {
            word += ch;
        }
    }
    pushWord();
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

float TextWrapped(const std::string& text, Rectangle box, float size, Color color, int maxChars,
                  bool bold) {
    const float lineHeight = size * 1.3f;
    const std::vector<std::string> lines = Wrap(text, box.width, size, bold);
    int remaining = maxChars;
    float y = box.y;
    for (const std::string& line : lines) {
        if (maxChars >= 0 && remaining <= 0) {
            break;
        }
        const int length = static_cast<int>(line.size());
        const std::string shown =
            maxChars >= 0 && remaining < length
                ? line.substr(0, static_cast<std::size_t>(remaining))
                : line;
        Text(shown, {box.x, y}, size, color, bold);
        remaining -= length + 1;
        y += lineHeight;
    }
    return static_cast<float>(lines.size()) * lineHeight;
}

void Panel(Rectangle rect, Color fill, Color border, float borderThickness) {
    const float roundness = Roundness(rect, 12.0f);
    DrawRectangleRounded(rect, roundness, 10, fill);
    if (borderThickness > 0.0f) {
        DrawRectangleRoundedLinesEx(rect, roundness, 10, borderThickness, border);
    }
}

void DrawButton(Rectangle rect, const std::string& label, bool focused, bool enabled,
                float textSize) {
    const Color fill = !enabled  ? Color{226, 220, 212, 255}
                       : focused ? Color{255, 214, 153, 255}
                                 : Color{255, 246, 232, 255};
    const Color border = focused && enabled ? Color{232, 116, 59, 255} : Color{176, 146, 120, 255};
    Panel(rect, fill, border, focused && enabled ? 3.0f : 2.0f);
    const Color ink = enabled ? Color{52, 38, 30, 255} : Color{150, 140, 130, 255};
    TextCentered(label, {rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f}, textSize, ink);
}

void Bar(Rectangle rect, float fraction, Color fill, Color back) {
    const float roundness = 1.0f;
    DrawRectangleRounded(rect, roundness, 8, back);
    const float f = std::clamp(fraction, 0.0f, 1.0f);
    if (f > 0.0f) {
        DrawRectangleRounded({rect.x, rect.y, std::max(rect.height, rect.width * f), rect.height},
                             roundness, 8, fill);
    }
}

bool Hovered(Rectangle rect) {
    return CheckCollisionPointRec(GetMousePosition(), rect);
}

bool Clicked(Rectangle rect) {
    return Hovered(rect) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

bool ConfirmPressed() {
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE) ||
           IsKeyPressed(KEY_E);
}

int Menu::Update(const std::vector<Rectangle>& items, const std::vector<bool>& enabled) {
    const int count = static_cast<int>(items.size());
    if (count == 0) {
        return -1;
    }
    auto isEnabled = [&](int i) {
        return i >= static_cast<int>(enabled.size()) || enabled[static_cast<std::size_t>(i)];
    };
    focused = std::clamp(focused, 0, count - 1);

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        focused = (focused + count - 1) % count;
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        focused = (focused + 1) % count;
    }

    const Vector2 mouseDelta = GetMouseDelta();
    const bool mouseMoved = mouseDelta.x != 0.0f || mouseDelta.y != 0.0f;
    for (int i = 0; i < count; ++i) {
        if (!Hovered(items[static_cast<std::size_t>(i)])) {
            continue;
        }
        if (mouseMoved) {
            focused = i;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && isEnabled(i)) {
            focused = i;
            audio::Play("click");
            return i;
        }
    }
    for (int i = 0; i < std::min(count, 9); ++i) {
        if (IsKeyPressed(KEY_ONE + i) && isEnabled(i)) {
            focused = i;
            audio::Play("click");
            return i;
        }
    }
    if (ConfirmPressed() && isEnabled(focused)) {
        audio::Play("click");
        return focused;
    }
    return -1;
}

}  // namespace engine::ui
