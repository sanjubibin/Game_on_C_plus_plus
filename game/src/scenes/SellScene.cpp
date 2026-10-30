#include "scenes/SellScene.h"

#include <algorithm>
#include <cmath>

#include "Visuals.h"
#include "engine/Application.h"
#include "engine/Audio.h"
#include "engine/Ui.h"
#include "logic/Rules.h"

namespace ui = engine::ui;
using namespace visuals;

namespace {

constexpr float kWalkIn = 1.1f;
constexpr float kBubble = 0.9f;
constexpr float kWalkOut = 1.0f;

float F(int value) {
    return static_cast<float>(value);
}

Look CustomerLook(std::size_t index) {
    static const Color bodies[] = {{90, 140, 220, 255}, {230, 120, 90, 255}, {120, 190, 120, 255},
                                   {200, 120, 200, 255}, {240, 200, 80, 255}, {100, 180, 190, 255}};
    static const Color hairs[] = {{40, 30, 25, 255}, {120, 80, 40, 255}, {220, 190, 120, 255},
                                  {30, 30, 30, 255}, {160, 60, 40, 255}};
    static const Color skins[] = {{240, 205, 170, 255}, {200, 150, 110, 255}, {150, 100, 70, 255},
                                  {225, 180, 140, 255}};
    return {bodies[index % 6], hairs[(index * 7 + 3) % 5], skins[(index * 5 + 1) % 4]};
}

}  // namespace

SellScene::SellScene(engine::Application& app, GameSession& session, const game::VenueDef& venue)
    : Scene(app),
      s_(session),
      venue_(venue),
      items_(game::ItemsInStock(session.state, session.content)),
      staff_(venue.staffable ? game::StaffCandidates(session.state, session.content) : std::vector<std::string>{}) {
    row_ = static_cast<int>(items_.size());  // start on the main button
}

void SellScene::DevAutoRun(float elapsedSeconds) {
    Run({});
    clock_ = elapsedSeconds;
}

SellScene::Layout SellScene::MakeLayout() const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    Layout l;
    l.panel = {40.0f, 30.0f, 640.0f, h - 60.0f};
    float y = l.panel.y + 128.0f;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const Rectangle row{l.panel.x + 20.0f, y, l.panel.width - 40.0f, 46.0f};
        l.rows.push_back(row);
        l.minus.push_back({row.x + 380.0f, row.y + 5.0f, 36.0f, 36.0f});
        l.plus.push_back({row.x + 496.0f, row.y + 5.0f, 36.0f, 36.0f});
        y += 50.0f;
    }
    const float by = l.panel.y + l.panel.height - 122.0f;
    l.open = {l.panel.x + 20.0f, by, 380.0f, 52.0f};
    l.leave = {l.panel.x + 420.0f, by, 180.0f, 52.0f};
    for (std::size_t i = 0; i < staff_.size(); ++i) {
        l.staff.push_back({l.panel.x + 20.0f + F(static_cast<int>(i)) * 196.0f, by + 60.0f, 188.0f, 46.0f});
    }
    l.done = {w / 2.0f - 110.0f, h - 100.0f, 220.0f, 56.0f};
    return l;
}

void SellScene::Run(const std::string& staffId) {
    questsBefore_ = flow::TakeQuestSnapshot(s_);
    report_ = game::RunSellSession(s_.state, s_.content, venue_, staffId);
    clock_ = 0.0f;
    phase_ = staffId.empty() ? Phase::Running : Phase::Results;
}

float SellScene::CustomerInterval() const {
    const float count = std::max(1.0f, F(static_cast<int>(report_.sales.customers.size())));
    return std::min(0.9f, 16.0f / count);
}

std::string SellScene::Lesson() const {
    const game::SellResult& sales = report_.sales;
    if (sales.customers.empty()) {
        return "Nobody came by. Reputation brings customers - be fair and friendly!";
    }
    if (report_.Profit() < 0 && sales.revenue - report_.costOfSold >= 0) {
        return report_.wage > 0
                   ? "The wage and fees cost more than the stand earned. Is this the best job for your staff?"
                   : "You earned more than your stock cost, but the fee ate your profit. Fixed costs need enough sales!";
    }
    if (report_.Profit() < 0) {
        return "You sold for less than it cost you. Check that your price is above your cost!";
    }
    if (sales.tooExpensive > sales.TotalSold()) {
        return "Lots of people said it was too expensive. Try lowering your prices a little.";
    }
    if (sales.soldOut >= 3) {
        return "You ran out! Busy places need more stock.";
    }
    for (const auto& [item, qty] : report_.leftover) {
        const game::ItemDef* def = s_.content.FindItem(item);
        if (qty > 0 && def != nullptr && game::ShelfDays(*def, s_.state) <= 1) {
            return "Some of your stock won't last until tomorrow. Buy only what you think you can sell.";
        }
    }
    return "Nice work! Fair prices keep customers coming back.";
}

void SellScene::Update(float dt) {
    const Layout l = MakeLayout();
    const int items = static_cast<int>(items_.size());
    const int staffCount = static_cast<int>(staff_.size());

    switch (phase_) {
        case Phase::Setup: {
            if (IsKeyPressed(KEY_ESCAPE)) {
                App().Scenes().Pop();
                return;
            }
            // Rows: items, Open, Leave, then one per staff member.
            const int rows = items + 2 + staffCount;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) row_ = (row_ + rows - 1) % rows;
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) row_ = (row_ + 1) % rows;
            auto change = [&](int index, int delta) {
                const std::string& id = items_[static_cast<std::size_t>(index)];
                s_.state.prices[id] = std::clamp(game::PriceOf(s_.state, s_.content, id) + delta, 1, 30);
            };
            if (row_ < items) {
                if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT) || IsKeyPressed(KEY_D)) change(row_, 1);
                if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT) || IsKeyPressed(KEY_A)) change(row_, -1);
            }
            for (int i = 0; i < items; ++i) {
                const auto idx = static_cast<std::size_t>(i);
                if (ui::Clicked(l.minus[idx])) change(i, -1);
                if (ui::Clicked(l.plus[idx])) change(i, 1);
            }
            const bool confirm = ui::ConfirmPressed();
            const bool canOpen = game::CannotOpenReason(s_.state, s_.content, venue_).empty();
            if ((ui::Clicked(l.open) || (confirm && row_ == items)) && canOpen) {
                Run({});
                return;
            }
            if (ui::Clicked(l.leave) || (confirm && row_ == items + 1)) {
                App().Scenes().Pop();
                return;
            }
            for (int i = 0; i < staffCount; ++i) {
                const std::string& id = staff_[static_cast<std::size_t>(i)];
                const bool picked = ui::Clicked(l.staff[static_cast<std::size_t>(i)]) || (confirm && row_ == items + 2 + i);
                if (picked && game::StaffUnavailableReason(s_.state, s_.content, venue_, id).empty()) {
                    Run(id);
                    return;
                }
            }
            if (confirm && row_ < items) {
                row_ = items;
            }
            break;
        }
        case Phase::Running: {
            const bool fast = IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_E) ||
                              IsMouseButtonDown(MOUSE_BUTTON_LEFT);
            clock_ += dt * (fast ? 4.0f : 1.0f);
            const float end = F(static_cast<int>(report_.sales.customers.size())) * CustomerInterval() + kWalkIn +
                              kBubble + kWalkOut;
            if (clock_ >= end || IsKeyPressed(KEY_ESCAPE)) {
                phase_ = Phase::Results;
            }
            break;
        }
        case Phase::Results:
            if (!resultsSounded_) {
                resultsSounded_ = true;
                engine::audio::Play(report_.Profit() > 0 ? "coin" : "fail");
            }
            if (ui::Clicked(l.done) || ui::ConfirmPressed() || IsKeyPressed(KEY_ESCAPE)) {
                if (report_.skillUp) {
                    s_.AddToast("Money sense +1");
                }
                flow::ToastQuestChanges(s_, questsBefore_);
                App().Scenes().Pop();
            }
            break;
    }
}

Vector2 SellScene::CounterPos() const {
    return {F(GetScreenWidth()) * 0.72f, F(GetScreenHeight()) * 0.62f};
}

void SellScene::DrawShopInterior() const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    const float floorY = h * 0.55f;
    // Back wall and floor.
    DrawRectangleRec({0.0f, 0.0f, w, floorY}, Color{238, 214, 176, 255});
    for (float x = 0.0f; x < w; x += 48.0f) {
        DrawRectangleRec({x, 0.0f, 2.0f, floorY}, Color{226, 200, 160, 255});
    }
    DrawRectangleRec({0.0f, floorY, w, h - floorY}, Color{160, 116, 78, 255});
    for (float y = floorY; y < h; y += 34.0f) {
        DrawLineEx({0.0f, y}, {w, y}, 2.0f, Color{140, 98, 64, 255});
    }

    // Shelves full of colourful goods (tidier with the upgrade).
    const bool tidy = s_.state.HasFlag("upgrade_shelves");
    const Color goods[] = {Color{230, 73, 128, 255}, Color{250, 176, 5, 255}, Color{55, 178, 77, 255},
                           Color{34, 139, 230, 255}, Color{245, 120, 60, 255}};
    for (int shelf = 0; shelf < 3; ++shelf) {
        const float sy = 90.0f + F(shelf) * 90.0f;
        DrawRectangleRec({60.0f, sy + 50.0f, 560.0f, 10.0f}, Color{130, 86, 52, 255});
        for (int i = 0; i < 16; ++i) {
            const float gx = 72.0f + F(i) * 34.0f;
            const float gh = tidy ? 40.0f : 26.0f + F((i * 7 + shelf * 3) % 15);
            DrawRectangleRec({gx, sy + 50.0f - gh, 26.0f, gh}, goods[(i + shelf) % 5]);
        }
    }

    // Window with the shop sign.
    const Rectangle window{w * 0.56f, 70.0f, 280.0f, 150.0f};
    DrawRectangleRec(window, Color{170, 215, 240, 255});
    DrawRectangleLinesEx(window, 6.0f, Color{130, 86, 52, 255});
    if (s_.state.HasFlag("upgrade_sign")) {
        const Rectangle sign{window.x + 20.0f, window.y - 50.0f, window.width - 40.0f, 40.0f};
        DrawRectangleRounded(sign, 0.3f, 8, Color{255, 240, 120, 255});
        DrawRectangleRoundedLinesEx(sign, 0.3f, 8, 3.0f, kAccent);
        ui::TextCentered("THE CORNER SHOP", {sign.x + sign.width / 2.0f, sign.y + sign.height / 2.0f}, 22.0f, kAccent,
                         true);
    }

    // Fridge with a soft glow.
    if (s_.state.HasFlag("fridge")) {
        const Rectangle fridge{w - 150.0f, floorY - 210.0f, 110.0f, 230.0f};
        DrawRectangleRec({fridge.x - 8.0f, fridge.y - 8.0f, fridge.width + 16.0f, fridge.height + 16.0f},
                         Color{180, 230, 255, 70});
        DrawRectangleRounded(fridge, 0.08f, 6, Color{235, 245, 250, 255});
        DrawRectangleRec({fridge.x + 10.0f, fridge.y + 14.0f, fridge.width - 20.0f, fridge.height - 28.0f},
                         Color{200, 235, 250, 255});
        for (int i = 0; i < 4; ++i) {
            const float by = fridge.y + 40.0f + F(i) * 46.0f;
            DrawRectangleRec({fridge.x + 16.0f, by, fridge.width - 32.0f, 4.0f}, Color{150, 190, 210, 255});
            DrawRectangleRec({fridge.x + 22.0f, by - 22.0f, 12.0f, 22.0f}, Color{245, 212, 66, 255});
            DrawRectangleRec({fridge.x + 44.0f, by - 22.0f, 12.0f, 22.0f}, Color{247, 198, 217, 255});
            DrawRectangleRec({fridge.x + 66.0f, by - 22.0f, 12.0f, 22.0f}, Color{245, 212, 66, 255});
        }
    }
}

void SellScene::DrawBackdrop() const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    const float horizon = h * 0.42f;
    const bool market = venue_.backdrop == "market";
    const bool shop = venue_.backdrop == "shop";
    const bool festival = s_.state.HasFlag("today.festival");
    const bool rain = venue_.outdoor && s_.state.HasFlag("today.rain");

    if (shop) {
        DrawShopInterior();
    } else if (rain) {
        DrawRectangleGradientV(0, 0, GetScreenWidth(), static_cast<int>(horizon), Color{120, 132, 150, 255},
                               Color{180, 188, 198, 255});
    } else {
        DrawRectangleGradientV(0, 0, GetScreenWidth(), static_cast<int>(horizon), Color{140, 200, 245, 255},
                               Color{215, 238, 250, 255});
        if (s_.state.HasFlag("today.hot")) {
            DrawCircleV({w - 160.0f, 100.0f}, 46.0f, Color{255, 200, 60, 255});
            DrawCircleV({w - 160.0f, 100.0f}, 60.0f, Color{255, 200, 60, 60});
        }
    }

    if (market) {
        // Other stalls along the back of the square, on paving stones.
        DrawRectangleRec({0.0f, horizon, w, h - horizon}, Color{214, 204, 186, 255});
        for (float y = horizon; y < h; y += 40.0f) {
            DrawLineEx({0.0f, y}, {w, y}, 1.0f, Color{190, 180, 162, 255});
        }
        const Color awnings[] = {Color{47, 158, 68, 255}, Color{214, 51, 108, 255}, Color{232, 89, 12, 255},
                                 Color{66, 99, 235, 255}, Color{245, 159, 0, 255}};
        for (int i = 0; i < 6; ++i) {
            DrawStall({40.0f + F(i) * 210.0f, horizon - 70.0f, 170.0f, 100.0f}, awnings[i % 5], "");
        }
        const Color flags[] = {Color{230, 73, 128, 255}, Color{250, 176, 5, 255}, Color{55, 178, 77, 255},
                               Color{34, 139, 230, 255}};
        const int count = festival ? 40 : 24;
        for (int i = 0; i < count; ++i) {
            const float x = F(i) * w / F(count);
            const float y = 40.0f + std::sin(F(i) / F(count) * PI) * 40.0f;
            DrawTriangle({x, y}, {x + w / F(count) * 0.5f, y + 22.0f}, {x + w / F(count) * 0.9f, y}, flags[i % 4]);
            DrawTriangle({x, y}, {x + w / F(count) * 0.9f, y}, {x + w / F(count) * 0.5f, y + 22.0f}, flags[i % 4]);
        }
    } else if (!shop) {
        for (int i = 0; i < 9; ++i) {
            const float x = F(i) * w / 8.0f + (i % 2 == 0 ? 0.0f : 40.0f);
            DrawCircleV({x, horizon - 10.0f}, 70.0f, Color{80, 150, 85, 255});
            DrawCircleV({x + 30.0f, horizon - 30.0f}, 50.0f, Color{100, 170, 95, 255});
        }
        DrawRectangleRec({0.0f, horizon, w, h - horizon}, Color{120, 190, 100, 255});
        DrawRectangleRec({0.0f, h * 0.70f, w, 70.0f}, Color{226, 206, 162, 255});
    }

    // Your team beside the counter, and whoever is selling behind it.
    const Vector2 counter = CounterPos();
    const std::string& seller = report_.staff;
    float x = counter.x + (market || shop ? 150.0f : 120.0f);
    for (const std::string& member : s_.state.party) {
        if (member == seller) {
            continue;
        }
        DrawCharacter({x, counter.y + 30.0f}, LookOf(s_.content.FindNpc(member)), 0.0f, 2.0f);
        x += 60.0f;
    }
    const Look sellerLook =
        seller.empty() ? PlayerLook(game::CurrentGeneration(s_.state)) : LookOf(s_.content.FindNpc(seller));
    if (market) {
        // Tall stall: the seller's face shows between the awning and the counter.
        DrawCharacter({counter.x + 10.0f, counter.y + 8.0f}, sellerLook, 0.0f, 2.2f);
        DrawStall({counter.x - 90.0f, counter.y - 150.0f, 200.0f, 190.0f}, kAccent, "THE CORNER SHOP");
    } else if (shop) {
        DrawCharacter({counter.x + 10.0f, counter.y - 10.0f}, sellerLook, 0.0f, 2.2f);
        const Rectangle desk{counter.x - 100.0f, counter.y - 30.0f, 220.0f, 70.0f};
        DrawRectangleRec({desk.x + 6.0f, desk.y + 8.0f, desk.width, desk.height}, Color{0, 0, 0, 50});
        DrawRectangleRec(desk, Color{120, 78, 46, 255});
        DrawRectangleRec({desk.x, desk.y, desk.width, 10.0f}, Color{170, 116, 70, 255});
        DrawRectangleRec({desk.x + 150.0f, desk.y - 26.0f, 50.0f, 26.0f}, Color{60, 60, 70, 255});  // till
    } else {
        DrawCharacter({counter.x + 10.0f, counter.y - 10.0f}, sellerLook, 0.0f, 2.2f);
        DrawPushcart(counter, 3.0f, s_.state.InParty("leo"));
    }
    if (rain) {
        DrawRain({0.0f, 0.0f, w, h}, static_cast<float>(GetTime()));
    }
}

void SellScene::DrawSetup(const Layout& l) const {
    ui::Panel(l.panel, kPaper, kPanelBorder, 3.0f);
    ui::Text(venue_.name, {l.panel.x + 24.0f, l.panel.y + 16.0f}, 32.0f, kInk, true);

    const bool hot = s_.state.HasFlag("today.hot");
    const bool festival = s_.state.HasFlag("today.festival");
    std::string weather = hot ? "Hot day: drinks will be popular!" : "Mild day: not very thirsty weather.";
    Color weatherColor = hot ? Color{200, 100, 20, 255} : kMuted;
    if (s_.state.HasFlag("today.rain")) {
        weather = venue_.outdoor ? "Rainy day: most people are staying at home." : "Rainy outside - but dry in here!";
        weatherColor = Color{50, 95, 160, 255};
    }
    if (festival) {
        weather = venue_.festivalCustomers > 1.0f ? "FESTIVAL DAY: the whole town is here!"
                                                  : "Festival Day: everyone is at the market today...";
        weatherColor = Color{200, 100, 20, 255};
    }
    ui::Text(weather, {l.panel.x + 24.0f, l.panel.y + 56.0f}, 19.0f, weatherColor, festival);

    // One status line: why you can't open, or what it costs.
    std::string status = game::CannotOpenReason(s_.state, s_.content, venue_);
    Color statusColor = kBad;
    if (status.empty()) {
        status = "Price above cost = profit. Too high = fewer buyers.";
        if (venue_.fee > 0) {
            status = "Fee: " + std::to_string(venue_.fee) + " coins each time you open. " + status;
        }
        if (!staff_.empty()) {
            status += "  Staff wage: " + std::to_string(game::StaffWage(s_.state)) + " coins.";
        }
        statusColor = kMuted;
    }
    ui::TextWrapped(status, {l.panel.x + 24.0f, l.panel.y + 82.0f, l.panel.width - 48.0f, 44.0f}, 16.0f, statusColor);

    const int items = static_cast<int>(items_.size());
    for (int i = 0; i < items; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        const std::string& id = items_[idx];
        const game::ItemDef* def = s_.content.FindItem(id);
        const Rectangle row = l.rows[idx];
        const bool focused = row_ == i;
        ui::Panel(row, focused ? Color{255, 236, 205, 255} : Color{255, 252, 246, 255},
                  focused ? kAccent : Color{220, 200, 180, 255}, focused ? 3.0f : 1.5f);
        DrawCircleV({row.x + 22.0f, row.y + 23.0f}, 12.0f, def ? ToColor(def->color) : GRAY);
        ui::Text(def ? def->name : id, {row.x + 44.0f, row.y + 4.0f}, 19.0f, kInk, true);
        const int cost = game::AverageCost(s_.state, id);
        std::string sub = std::to_string(s_.state.StockOf(id)) + " in stock, cost " + std::to_string(cost);
        if (def != nullptr && game::ItemValue(*def, s_.state) != def->value) {
            sub += ", worth only " + std::to_string(game::ItemValue(*def, s_.state)) + " today!";
        }
        ui::Text(sub, {row.x + 44.0f, row.y + 27.0f}, 13.0f, kMuted);

        const int price = game::PriceOf(s_.state, s_.content, id);
        ui::DrawButton(l.minus[idx], "-", false, price > 1, 24.0f);
        ui::TextCentered(std::to_string(price), {row.x + 456.0f, row.y + 19.0f}, 24.0f, kInk, true);
        ui::TextCentered("price", {row.x + 456.0f, row.y + 38.0f}, 11.0f, kMuted);
        ui::DrawButton(l.plus[idx], "+", false, price < 30, 24.0f);
        const int each = price - cost;
        ui::Text((each >= 0 ? "+" : "") + std::to_string(each), {row.x + 548.0f, row.y + 6.0f}, 19.0f,
                 each > 0 ? kGood : kBad, true);
        ui::Text("each", {row.x + 548.0f, row.y + 28.0f}, 11.0f, kMuted);
    }

    const bool canOpen = game::CannotOpenReason(s_.state, s_.content, venue_).empty();
    ui::DrawButton(l.open, "Open it yourself (" + std::to_string(venue_.hours) + " hours)", row_ == items, canOpen, 21.0f);
    ui::DrawButton(l.leave, "Leave", row_ == items + 1, true, 21.0f);
    for (std::size_t i = 0; i < staff_.size(); ++i) {
        const std::string& id = staff_[i];
        const bool available = game::StaffUnavailableReason(s_.state, s_.content, venue_, id).empty();
        ui::DrawButton(l.staff[i], "Let " + s_.content.NameOf(id) + " run it", row_ == items + 2 + static_cast<int>(i),
                       available, 18.0f);
    }
    if (!staff_.empty()) {
        // Explain why the focused staff member can't work, if they can't.
        const int focusedStaff = row_ - items - 2;
        std::string note = "Staff sell less than you, but your clock doesn't move.";
        if (focusedStaff >= 0 && focusedStaff < static_cast<int>(staff_.size())) {
            const std::string reason = game::StaffUnavailableReason(s_.state, s_.content, venue_,
                                                                    staff_[static_cast<std::size_t>(focusedStaff)]);
            if (!reason.empty()) {
                note = reason;
            }
        }
        ui::Text(note, {l.panel.x + 24.0f, l.open.y - 24.0f}, 15.0f, kMuted);
    }
}

void SellScene::DrawRunning() const {
    const Vector2 counter = CounterPos();
    const Vector2 front{counter.x - 80.0f, counter.y + 64.0f};
    const float interval = CustomerInterval();
    const game::SellResult& sales = report_.sales;

    int coins = 0;
    int sold = 0;
    for (std::size_t i = 0; i < sales.customers.size(); ++i) {
        const game::Customer& c = sales.customers[i];
        const float t = clock_ - F(static_cast<int>(i)) * interval;
        if (t < 0.0f) {
            continue;
        }
        if (t >= kWalkIn) {
            coins += c.paid;
            sold += c.outcome == game::Outcome::Bought ? 1 : 0;
        }
        if (t > kWalkIn + kBubble + kWalkOut) {
            continue;
        }
        // Customers overlap in time, so each uses one of three spots in front of the counter.
        const float slot = F(static_cast<int>(i % 3));
        const Vector2 spot{front.x - slot * 95.0f, front.y + slot * 6.0f};
        Vector2 feet;
        float bob = 0.0f;
        if (t < kWalkIn) {
            const float k = t / kWalkIn;
            feet = {-60.0f + (spot.x + 60.0f) * k, spot.y};
            bob = std::sin(t * 16.0f) * 2.0f;
        } else if (t < kWalkIn + kBubble) {
            feet = spot;
        } else {
            const float k = (t - kWalkIn - kBubble) / kWalkOut;
            feet = {spot.x - k * (spot.x + 80.0f), spot.y + k * 60.0f};
            bob = std::sin(t * 16.0f) * 2.0f;
        }
        DrawCharacter(feet, CustomerLook(i), bob, 2.2f);

        if (t >= kWalkIn && t < kWalkIn + kBubble + 0.3f) {
            std::string text;
            Color color = kInk;
            if (c.outcome == game::Outcome::Bought) {
                const game::ItemDef* def = s_.content.FindItem(c.bought);
                text = (def ? def->name : c.bought) + "! +" + std::to_string(c.paid);
                color = kGood;
            } else if (c.outcome == game::Outcome::TooExpensive) {
                text = "Too pricey!";
                color = kBad;
            } else {
                text = "Sold out?";
                color = kMuted;
            }
            const Vector2 m = ui::Measure(text, 22.0f, true);
            const float lift = i % 2 == 0 ? 0.0f : 46.0f;
            const Rectangle bubble{feet.x - m.x / 2.0f - 14.0f, feet.y - 150.0f - lift, m.x + 28.0f, 40.0f};
            ui::Panel(bubble, WHITE, color, 2.0f);
            ui::Text(text, {bubble.x + 14.0f, bubble.y + 8.0f}, 22.0f, color, true);
        }
    }

    const Rectangle tally{40.0f, 40.0f, 360.0f, 120.0f};
    ui::Panel(tally, kPanel, kPanelBorder, 3.0f);
    ui::Text("Selling...", {tally.x + 20.0f, tally.y + 14.0f}, 28.0f, kInk, true);
    DrawCoin({tally.x + 32.0f, tally.y + 74.0f}, 11.0f);
    ui::Text(std::to_string(coins) + " coins   " + std::to_string(sold) + " sold", {tally.x + 52.0f, tally.y + 60.0f},
             26.0f, kInk, true);
    ui::Text("Hold Space to speed up   Esc to skip", {tally.x + 20.0f, tally.y + 94.0f}, 15.0f, kMuted);
}

void SellScene::DrawResults(const Layout& l) const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    const game::SellResult& sales = report_.sales;
    DrawRectangleRec({0.0f, 0.0f, w, h}, Color{20, 12, 20, 110});
    const Rectangle panel{w / 2.0f - 410.0f, 20.0f, 820.0f, h - 40.0f};
    ui::Panel(panel, kPaper, kPanelBorder, 3.0f);

    const float x = panel.x + 40.0f;
    float y = panel.y + 20.0f;
    const std::string title = report_.staff.empty()
                                  ? "How did the " + venue_.name + " do?"
                                  : s_.content.NameOf(report_.staff) + " ran the " + venue_.name + " for you";
    ui::Text(title, {x, y}, 30.0f, kInk, true);
    y += 44.0f;

    std::string soldText;
    for (const auto& [item, qty] : sales.sold) {
        const game::ItemDef* def = s_.content.FindItem(item);
        soldText += (soldText.empty() ? "" : ", ") + std::to_string(qty) + " " + (def ? def->name : item);
    }
    y += ui::TextWrapped(std::to_string(sales.customers.size()) + " customers came by. Sold " +
                             std::to_string(sales.TotalSold()) + " items" + (soldText.empty() ? "." : ": " + soldText),
                         {x, y, panel.width - 80.0f, 50.0f}, 19.0f, kInk) +
         8.0f;

    const float col = x + 440.0f;
    auto line = [&](const std::string& label, const std::string& amount) {
        ui::Text(label, {x, y}, 21.0f, kInk);
        ui::Text(amount, {col, y}, 21.0f, kInk, true);
        y += 28.0f;
    };
    line("Money in", "+" + std::to_string(sales.revenue));
    line("Cost of what you sold", "-" + std::to_string(report_.costOfSold));
    if (report_.fee > 0) {
        line("Fee", "-" + std::to_string(report_.fee));
    }
    if (report_.wage > 0) {
        line(s_.content.NameOf(report_.staff) + "'s wage", "-" + std::to_string(report_.wage));
    }
    y += 4.0f;
    DrawLineEx({x, y}, {col + 120.0f, y}, 2.0f, kPanelBorder);
    y += 8.0f;
    const int profit = report_.Profit();
    ui::Text("PROFIT", {x, y}, 28.0f, kInk, true);
    ui::Text((profit >= 0 ? "+" : "") + std::to_string(profit) + " coins", {col, y}, 28.0f, profit >= 0 ? kGood : kBad,
             true);
    y += 44.0f;

    if (sales.tooExpensive > 0 || sales.soldOut > 0) {
        ui::Text("Turned away: " + std::to_string(sales.tooExpensive) + " said too expensive, " +
                     std::to_string(sales.soldOut) + " found it sold out",
                 {x, y}, 17.0f, kMuted);
        y += 24.0f;
    }
    ui::Text("Reputation " + std::string(report_.reputationChange >= 0 ? "+" : "") +
                 std::to_string(report_.reputationChange),
             {x, y}, 17.0f, report_.reputationChange >= 0 ? kGood : kBad, true);
    y += 24.0f;
    for (const std::string& bonus : report_.bonusTexts) {
        ui::Text(bonus, {x, y}, 17.0f, bonus.find("MegaMart") != std::string::npos ? kBad : kGood);
        y += 24.0f;
    }
    if (report_.skillUp) {
        ui::Text("Money sense +1", {x, y}, 17.0f, kAccent, true);
        y += 24.0f;
    }
    std::string left;
    for (const auto& [item, qty] : report_.leftover) {
        if (qty > 0) {
            const game::ItemDef* def = s_.content.FindItem(item);
            left += (left.empty() ? "" : ", ") + std::to_string(qty) + " " + (def ? def->name : item);
        }
    }
    if (!left.empty()) {
        y += ui::TextWrapped("Left over: " + left, {x, y, panel.width - 80.0f, 44.0f}, 17.0f, kMuted);
    }

    const Rectangle tip{x, y + 8.0f, panel.width - 80.0f, 58.0f};
    ui::Panel(tip, Color{255, 236, 205, 255}, kAccent, 2.0f);
    ui::TextWrapped(Lesson(), {tip.x + 16.0f, tip.y + 9.0f, tip.width - 32.0f, tip.height}, 18.0f, kInk);

    ui::DrawButton(l.done, "Done", true, true, 24.0f);
}

void SellScene::Draw(float /*alpha*/) const {
    DrawBackdrop();
    const Layout l = MakeLayout();
    switch (phase_) {
        case Phase::Setup:
            DrawSetup(l);
            break;
        case Phase::Running:
            DrawRunning();
            break;
        case Phase::Results:
            DrawResults(l);
            break;
    }
}
