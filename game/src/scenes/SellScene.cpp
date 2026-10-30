#include "scenes/SellScene.h"

#include <algorithm>
#include <cmath>

#include "Visuals.h"
#include "engine/Application.h"
#include "engine/Ui.h"

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
    : Scene(app), s_(session), venue_(venue), items_(game::ItemsInStock(session.state, session.content)) {
    row_ = static_cast<int>(items_.size());  // start on the main button
}

void SellScene::DevAutoRun(float elapsedSeconds) {
    Run(false);
    clock_ = elapsedSeconds;
}

SellScene::Layout SellScene::MakeLayout() const {
    const float h = F(GetScreenHeight());
    Layout l;
    l.panel = {40.0f, 30.0f, 640.0f, h - 60.0f};
    float y = l.panel.y + 140.0f;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const Rectangle row{l.panel.x + 20.0f, y, l.panel.width - 40.0f, 64.0f};
        l.rows.push_back(row);
        l.minus.push_back({row.x + 380.0f, row.y + 12.0f, 40.0f, 40.0f});
        l.plus.push_back({row.x + 496.0f, row.y + 12.0f, 40.0f, 40.0f});
        y += 70.0f;
    }
    const float by = l.panel.y + l.panel.height - 130.0f;
    l.open = {l.panel.x + 20.0f, by, 300.0f, 54.0f};
    l.helper = {l.panel.x + 330.0f, by, 290.0f, 54.0f};
    l.leave = {l.panel.x + 20.0f, by + 64.0f, 160.0f, 48.0f};
    const float w = F(GetScreenWidth());
    l.done = {w / 2.0f - 110.0f, h - 110.0f, 220.0f, 56.0f};
    return l;
}

void SellScene::Run(bool helperRuns) {
    questsBefore_ = flow::TakeQuestSnapshot(s_);
    report_ = game::RunSellSession(s_.state, s_.content, venue_, helperRuns);
    clock_ = 0.0f;
    phase_ = helperRuns ? Phase::Results : Phase::Running;
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
        return "You earned more than your stock cost, but the fee ate your profit. Fixed costs need enough sales!";
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
        if (qty > 0 && def != nullptr && def->shelfDays <= 1) {
            return "Some of your stock won't last until tomorrow. Buy only what you think you can sell.";
        }
    }
    return "Nice work! Fair prices keep customers coming back.";
}

void SellScene::Update(float dt) {
    const Layout l = MakeLayout();
    const int items = static_cast<int>(items_.size());

    switch (phase_) {
        case Phase::Setup: {
            if (IsKeyPressed(KEY_ESCAPE)) {
                App().Scenes().Pop();
                return;
            }
            const int rows = items + 3;  // items, open, helper, leave
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) row_ = (row_ + rows - 1) % rows;
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) row_ = (row_ + 1) % rows;
            auto change = [&](int index, int delta) {
                const std::string& id = items_[static_cast<std::size_t>(index)];
                s_.state.prices[id] = std::clamp(game::PriceOf(s_.state, s_.content, id) + delta, 1, 20);
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
            const bool canOpen = game::CannotOpenReason(s_.state, s_.content, venue_).empty();
            const bool confirm = ui::ConfirmPressed();
            if ((ui::Clicked(l.open) || (confirm && row_ == items)) && canOpen) {
                Run(false);
            } else if ((ui::Clicked(l.helper) || (confirm && row_ == items + 1)) &&
                       game::HelperCanRun(s_.state, s_.content, venue_)) {
                Run(true);
            } else if (ui::Clicked(l.leave) || (confirm && row_ == items + 2)) {
                App().Scenes().Pop();
            } else if (confirm && row_ < items) {
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

void SellScene::DrawBackdrop() const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    const float horizon = h * 0.42f;
    const bool market = venue_.backdrop == "market";
    const bool festival = s_.state.HasFlag("today.festival");

    DrawRectangleGradientV(0, 0, GetScreenWidth(), static_cast<int>(horizon), Color{140, 200, 245, 255},
                           Color{215, 238, 250, 255});
    if (s_.state.HasFlag("today.hot")) {
        DrawCircleV({w - 160.0f, 100.0f}, 46.0f, Color{255, 200, 60, 255});
        DrawCircleV({w - 160.0f, 100.0f}, 60.0f, Color{255, 200, 60, 60});
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
        // Bunting
        const Color flags[] = {Color{230, 73, 128, 255}, Color{250, 176, 5, 255}, Color{55, 178, 77, 255},
                               Color{34, 139, 230, 255}};
        const int count = festival ? 40 : 24;
        for (int i = 0; i < count; ++i) {
            const float x = F(i) * w / F(count);
            const float y = 40.0f + std::sin(F(i) / F(count) * PI) * 40.0f;
            DrawTriangle({x, y}, {x + w / F(count) * 0.5f, y + 22.0f}, {x + w / F(count) * 0.9f, y}, flags[i % 4]);
            DrawTriangle({x, y}, {x + w / F(count) * 0.9f, y}, {x + w / F(count) * 0.5f, y + 22.0f}, flags[i % 4]);
        }
    } else {
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
    const std::string seller = report_.helperRan ? venue_.helper : "";
    float x = counter.x + (market ? 150.0f : 120.0f);
    for (const std::string& member : s_.state.party) {
        if (member == seller) {
            continue;
        }
        DrawCharacter({x, counter.y + 30.0f}, LookOf(s_.content.FindNpc(member)), 0.0f, 2.0f);
        x += 60.0f;
    }
    const Look sellerLook = seller.empty() ? PlayerLook() : LookOf(s_.content.FindNpc(seller));
    if (market) {
        // Tall stall: the seller's face shows between the awning and the counter.
        DrawCharacter({counter.x + 10.0f, counter.y + 8.0f}, sellerLook, 0.0f, 2.2f);
        DrawStall({counter.x - 90.0f, counter.y - 150.0f, 200.0f, 190.0f}, kAccent, "THE CORNER SHOP");
    } else {
        DrawCharacter({counter.x + 10.0f, counter.y - 10.0f}, sellerLook, 0.0f, 2.2f);
        DrawPushcart(counter, 3.0f, s_.state.InParty("leo"));
    }
}

void SellScene::DrawSetup(const Layout& l) const {
    ui::Panel(l.panel, kPaper, kPanelBorder, 3.0f);
    ui::Text(venue_.name, {l.panel.x + 24.0f, l.panel.y + 18.0f}, 34.0f, kInk, true);

    const bool hot = s_.state.HasFlag("today.hot");
    const bool festival = s_.state.HasFlag("today.festival");
    std::string weather = hot ? "Hot day: drinks will be popular!" : "Mild day: not very thirsty weather.";
    if (festival) {
        weather = venue_.festivalCustomers > 1.0f ? "FESTIVAL DAY: the whole town is here!"
                                                  : "Festival Day: everyone is at the market today...";
    }
    ui::Text(weather, {l.panel.x + 24.0f, l.panel.y + 62.0f}, 20.0f,
             hot || festival ? Color{200, 100, 20, 255} : kMuted, festival);
    std::string info = "Price above cost = profit. Too high = fewer buyers.";
    if (venue_.fee > 0) {
        info = "Fee: " + std::to_string(venue_.fee) + " coins each time you open. " + info;
    }
    ui::TextWrapped(info, {l.panel.x + 24.0f, l.panel.y + 92.0f, l.panel.width - 48.0f, 40.0f}, 17.0f, kMuted);

    const int items = static_cast<int>(items_.size());
    for (int i = 0; i < items; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        const std::string& id = items_[idx];
        const game::ItemDef* def = s_.content.FindItem(id);
        const Rectangle row = l.rows[idx];
        const bool focused = row_ == i;
        ui::Panel(row, focused ? Color{255, 236, 205, 255} : Color{255, 252, 246, 255},
                  focused ? kAccent : Color{220, 200, 180, 255}, focused ? 3.0f : 1.5f);
        DrawCircleV({row.x + 26.0f, row.y + 32.0f}, 14.0f, def ? ToColor(def->color) : GRAY);
        ui::Text(def ? def->name : id, {row.x + 50.0f, row.y + 8.0f}, 22.0f, kInk, true);
        const int cost = game::AverageCost(s_.state, id);
        ui::Text(std::to_string(s_.state.StockOf(id)) + " in stock, cost " + std::to_string(cost) + " each",
                 {row.x + 50.0f, row.y + 38.0f}, 15.0f, kMuted);

        const int price = game::PriceOf(s_.state, s_.content, id);
        ui::DrawButton(l.minus[idx], "-", false, price > 1, 26.0f);
        ui::TextCentered(std::to_string(price), {row.x + 458.0f, row.y + 26.0f}, 28.0f, kInk, true);
        ui::TextCentered("price", {row.x + 458.0f, row.y + 52.0f}, 13.0f, kMuted);
        ui::DrawButton(l.plus[idx], "+", false, price < 20, 26.0f);
        const int each = price - cost;
        ui::Text((each >= 0 ? "+" : "") + std::to_string(each), {row.x + 552.0f, row.y + 12.0f}, 22.0f,
                 each > 0 ? kGood : kBad, true);
        ui::Text("each", {row.x + 552.0f, row.y + 40.0f}, 13.0f, kMuted);
    }

    std::string status = game::CannotOpenReason(s_.state, s_.content, venue_);
    const std::string helperName = venue_.helper.empty() ? "" : s_.content.NameOf(venue_.helper);
    if (status.empty() && !venue_.helper.empty() && !game::HelperCanRun(s_.state, s_.content, venue_)) {
        if (s_.state.HasFlag("today.helper_ran")) {
            status = "A helper already ran a stand for you today.";
        } else if (s_.state.HasFlag(venue_.helper + "_upset")) {
            status = helperName + " is too upset to help right now.";
        }
    }
    if (!status.empty()) {
        ui::TextWrapped(status, {l.panel.x + 24.0f, l.open.y - 46.0f, l.panel.width - 48.0f, 40.0f}, 18.0f, kBad, -1,
                        true);
    }
    const bool canOpen = game::CannotOpenReason(s_.state, s_.content, venue_).empty();
    ui::DrawButton(l.open, "Open (" + std::to_string(venue_.hours) + " hours)", row_ == items, canOpen, 22.0f);
    if (!venue_.helper.empty()) {
        ui::DrawButton(l.helper, "Let " + helperName + " run it", row_ == items + 1,
                       game::HelperCanRun(s_.state, s_.content, venue_), 22.0f);
        ui::Text(helperName + " sells less, but you're free to explore.", {l.panel.x + 196.0f, l.leave.y + 14.0f},
                 16.0f, kMuted);
    }
    ui::DrawButton(l.leave, "Leave", row_ == items + 2, true, 22.0f);
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
    const Rectangle panel{w / 2.0f - 410.0f, 24.0f, 820.0f, h - 48.0f};
    ui::Panel(panel, kPaper, kPanelBorder, 3.0f);

    const float x = panel.x + 40.0f;
    float y = panel.y + 22.0f;
    const std::string title = report_.helperRan
                                  ? s_.content.NameOf(venue_.helper) + " ran the " + venue_.name + " for you"
                                  : "How did the " + venue_.name + " do?";
    ui::Text(title, {x, y}, 32.0f, kInk, true);
    y += 48.0f;

    std::string soldText;
    for (const auto& [item, qty] : sales.sold) {
        const game::ItemDef* def = s_.content.FindItem(item);
        soldText += (soldText.empty() ? "" : ", ") + std::to_string(qty) + " " + (def ? def->name : item);
    }
    ui::Text(std::to_string(sales.customers.size()) + " customers came by. Sold " + std::to_string(sales.TotalSold()) +
                 " items" + (soldText.empty() ? "." : ": " + soldText),
             {x, y}, 20.0f, kInk);
    y += 38.0f;

    const float col = x + 440.0f;
    ui::Text("Money in", {x, y}, 22.0f, kInk);
    ui::Text("+" + std::to_string(sales.revenue), {col, y}, 22.0f, kInk, true);
    y += 30.0f;
    ui::Text("Cost of what you sold", {x, y}, 22.0f, kInk);
    ui::Text("-" + std::to_string(report_.costOfSold), {col, y}, 22.0f, kInk, true);
    y += 30.0f;
    if (report_.fee > 0) {
        ui::Text("Stall fee", {x, y}, 22.0f, kInk);
        ui::Text("-" + std::to_string(report_.fee), {col, y}, 22.0f, kInk, true);
        y += 30.0f;
    }
    y += 4.0f;
    DrawLineEx({x, y}, {col + 120.0f, y}, 2.0f, kPanelBorder);
    y += 8.0f;
    const int profit = report_.Profit();
    ui::Text("PROFIT", {x, y}, 28.0f, kInk, true);
    ui::Text((profit >= 0 ? "+" : "") + std::to_string(profit) + " coins", {col, y}, 28.0f, profit >= 0 ? kGood : kBad,
             true);
    y += 46.0f;

    if (sales.tooExpensive > 0 || sales.soldOut > 0) {
        ui::Text("Turned away: " + std::to_string(sales.tooExpensive) + " said too expensive, " +
                     std::to_string(sales.soldOut) + " found it sold out",
                 {x, y}, 18.0f, kMuted);
        y += 26.0f;
    }
    ui::Text("Reputation " + std::string(report_.reputationChange >= 0 ? "+" : "") +
                 std::to_string(report_.reputationChange),
             {x, y}, 18.0f, report_.reputationChange >= 0 ? kGood : kBad, true);
    y += 26.0f;
    for (const std::string& bonus : report_.bonusTexts) {
        ui::Text(bonus, {x, y}, 18.0f, kGood);
        y += 26.0f;
    }
    if (report_.skillUp) {
        ui::Text("Money sense +1", {x, y}, 18.0f, kAccent, true);
        y += 26.0f;
    }
    std::string left;
    for (const auto& [item, qty] : report_.leftover) {
        if (qty > 0) {
            const game::ItemDef* def = s_.content.FindItem(item);
            left += (left.empty() ? "" : ", ") + std::to_string(qty) + " " + (def ? def->name : item);
        }
    }
    if (!left.empty()) {
        ui::Text("Left over: " + left, {x, y}, 18.0f, kMuted);
        y += 26.0f;
    }

    const Rectangle tip{x, y + 8.0f, panel.width - 80.0f, 62.0f};
    ui::Panel(tip, Color{255, 236, 205, 255}, kAccent, 2.0f);
    ui::TextWrapped(Lesson(), {tip.x + 16.0f, tip.y + 10.0f, tip.width - 32.0f, tip.height}, 19.0f, kInk);

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
