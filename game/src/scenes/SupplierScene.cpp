#include "scenes/SupplierScene.h"

#include <algorithm>

#include "Flow.h"
#include "Visuals.h"
#include "engine/Application.h"
#include "engine/Ui.h"
#include "logic/Selling.h"

namespace ui = engine::ui;
using namespace visuals;

namespace {

constexpr int kMaxQty = 30;

float F(int value) {
    return static_cast<float>(value);
}

}  // namespace

SupplierScene::SupplierScene(engine::Application& app, GameSession& session) : Scene(app), s_(session) {
    for (const std::string& id : s_.content.itemOrder) {
        if (game::ItemAvailable(s_.content.items.at(id), s_.state)) {
            items_.push_back(id);
        }
    }
}

SupplierScene::Layout SupplierScene::MakeLayout() const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    Layout l;
    l.panel = {w / 2.0f - 470.0f, h / 2.0f - 330.0f, 940.0f, 660.0f};
    float y = l.panel.y + 120.0f;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const Rectangle row{l.panel.x + 30.0f, y, l.panel.width - 60.0f, 72.0f};
        l.rows.push_back(row);
        l.minus.push_back({row.x + 560.0f, row.y + 14.0f, 44.0f, 44.0f});
        l.plus.push_back({row.x + 680.0f, row.y + 14.0f, 44.0f, 44.0f});
        y += 80.0f;
    }
    l.buy = {l.panel.x + l.panel.width - 470.0f, l.panel.y + l.panel.height - 76.0f, 250.0f, 54.0f};
    l.leave = {l.panel.x + l.panel.width - 200.0f, l.panel.y + l.panel.height - 76.0f, 170.0f, 54.0f};
    return l;
}

int SupplierScene::Total() const {
    int total = 0;
    for (const auto& [id, qty] : qty_) {
        if (const game::ItemDef* item = s_.content.FindItem(id)) {
            total += qty * item->cost;
        }
    }
    return total;
}

void SupplierScene::Buy() {
    const int total = Total();
    if (total <= 0 || total > s_.state.Get("money")) {
        return;
    }
    const flow::QuestSnapshot before = flow::TakeQuestSnapshot(s_);
    std::string bought;
    for (const std::string& id : items_) {
        const int qty = qty_[id];
        if (qty <= 0) {
            continue;
        }
        const game::ItemDef* item = s_.content.FindItem(id);
        s_.state.AddStock({id, qty, item->cost, s_.state.day, false});
        bought += (bought.empty() ? "" : ", ") + std::to_string(qty) + " " + item->name;
    }
    s_.state.Add("money", -total);
    s_.state.SetFlag("bought_stock");
    s_.state.minutes += 30;
    s_.state.todayLog.push_back("Bought " + bought + " for " + std::to_string(total) + " coins.");
    s_.AddToast("Coins -" + std::to_string(total));
    flow::ToastQuestChanges(s_, before);
    App().Scenes().Pop();
}

void SupplierScene::Update(float /*dt*/) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        App().Scenes().Pop();
        return;
    }
    const Layout l = MakeLayout();
    const int items = static_cast<int>(items_.size());
    const int rows = items + 2;

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) row_ = (row_ + rows - 1) % rows;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) row_ = (row_ + 1) % rows;

    auto change = [&](int index, int delta) {
        int& q = qty_[items_[static_cast<std::size_t>(index)]];
        q = std::clamp(q + delta, 0, kMaxQty);
    };
    if (row_ < items) {
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT) || IsKeyPressed(KEY_D)) change(row_, 1);
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT) || IsKeyPressed(KEY_A)) change(row_, -1);
    }
    for (int i = 0; i < items; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        if (ui::Hovered(l.rows[idx]) && GetMouseDelta().y != 0.0f) row_ = i;
        if (ui::Clicked(l.minus[idx])) change(i, -1);
        if (ui::Clicked(l.plus[idx])) change(i, 1);
    }
    if (ui::Clicked(l.buy) || (row_ == items && ui::ConfirmPressed())) {
        Buy();
    } else if (ui::Clicked(l.leave) || (row_ == items + 1 && ui::ConfirmPressed())) {
        App().Scenes().Pop();
    } else if (row_ < items && ui::ConfirmPressed()) {
        row_ = items;  // Enter on an item jumps to the Buy button
    }
}

void SupplierScene::Draw(float /*alpha*/) const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    DrawRectangleRec({0.0f, 0.0f, w, h}, Color{20, 12, 20, 150});
    const Layout l = MakeLayout();
    ui::Panel(l.panel, kPaper, kPanelBorder, 3.0f);

    const game::NpcDef* lin = s_.content.FindNpc("mrs_lin");
    DrawPortrait({l.panel.x + 64.0f, l.panel.y + 60.0f}, 40.0f, LookOf(lin));
    ui::Text("Lin's Wholesale", {l.panel.x + 120.0f, l.panel.y + 22.0f}, 32.0f, kInk, true);
    ui::Text("You buy at COST PRICE here. Sell for more to make a PROFIT.", {l.panel.x + 120.0f, l.panel.y + 64.0f},
             19.0f, kMuted);
    DrawCoin({l.panel.x + l.panel.width - 180.0f, l.panel.y + 40.0f}, 11.0f);
    ui::Text(std::to_string(s_.state.Get("money")) + " coins", {l.panel.x + l.panel.width - 160.0f, l.panel.y + 26.0f},
             26.0f, kInk, true);

    const int items = static_cast<int>(items_.size());
    for (int i = 0; i < items; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        const game::ItemDef& item = s_.content.items.at(items_[idx]);
        const Rectangle row = l.rows[idx];
        const bool focused = row_ == i;
        ui::Panel(row, focused ? Color{255, 236, 205, 255} : Color{255, 252, 246, 255},
                  focused ? kAccent : Color{220, 200, 180, 255}, focused ? 3.0f : 1.5f);
        DrawCircleV({row.x + 32.0f, row.y + 36.0f}, 16.0f, ToColor(item.color));
        ui::Text(item.name, {row.x + 62.0f, row.y + 8.0f}, 24.0f, kInk, true);
        ui::Text("Cost " + std::to_string(item.cost) + " each", {row.x + 330.0f, row.y + 12.0f}, 20.0f, kInk, true);
        ui::Text("People usually pay about " + std::to_string(item.value) + " coins", {row.x + 62.0f, row.y + 42.0f},
                 17.0f, kGood);

        const int qty = qty_.count(item.id) ? qty_.at(item.id) : 0;
        ui::DrawButton(l.minus[idx], "-", false, qty > 0, 28.0f);
        ui::TextCentered(std::to_string(qty), {row.x + 642.0f, row.y + 36.0f}, 30.0f, kInk, true);
        ui::DrawButton(l.plus[idx], "+", false, qty < kMaxQty, 28.0f);
        ui::Text("= " + std::to_string(qty * item.cost), {row.x + 750.0f, row.y + 22.0f}, 24.0f, kInk, true);
    }

    // Notes for the focused item: how long it lasts and anything special.
    if (row_ < items) {
        const game::ItemDef& focused = s_.content.items.at(items_[static_cast<std::size_t>(row_)]);
        ui::TextWrapped(focused.note, {l.panel.x + 36.0f, l.panel.y + l.panel.height - 130.0f, l.panel.width - 72.0f, 40.0f},
                        18.0f, kMuted);
    }

    const int total = Total();
    const int money = s_.state.Get("money");
    const bool affordable = total <= money;
    const float footerY = l.panel.y + l.panel.height - 76.0f;
    ui::Text("Total: " + std::to_string(total) + " coins", {l.panel.x + 36.0f, footerY}, 28.0f, kInk, true);
    if (!affordable) {
        ui::Text("Not enough coins!", {l.panel.x + 36.0f, footerY + 34.0f}, 19.0f, kBad, true);
    } else if (total > 0) {
        ui::Text("You'll have " + std::to_string(money - total) + " coins left.", {l.panel.x + 36.0f, footerY + 34.0f},
                 19.0f, kMuted);
    } else {
        ui::Text("Use Left / Right or the buttons to choose amounts.", {l.panel.x + 36.0f, footerY + 34.0f}, 19.0f,
                 kMuted);
    }
    ui::DrawButton(l.buy, "Buy (30 min)", row_ == items, total > 0 && affordable, 24.0f);
    ui::DrawButton(l.leave, "Leave", row_ == items + 1, true, 24.0f);
}
