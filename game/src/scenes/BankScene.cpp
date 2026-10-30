#include "scenes/BankScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "Visuals.h"
#include "engine/Application.h"
#include "logic/Bank.h"

namespace ui = engine::ui;
using namespace visuals;

namespace {

constexpr std::array<const char*, 4> kButtons = {"Save it", "Take it out", "Pay back early", "Leave"};
constexpr int kStep = 5;

float F(int value) {
    return static_cast<float>(value);
}

// A tree that grows with your savings: a picture of compound interest.
void DrawSavingsTree(Vector2 ground, int savings) {
    const float growth = std::clamp(std::sqrt(F(savings)) / 12.0f, 0.15f, 1.0f);
    const float trunkH = 40.0f + 90.0f * growth;
    DrawEllipse(static_cast<int>(ground.x), static_cast<int>(ground.y), 70.0f, 10.0f, Color{0, 0, 0, 40});
    DrawRectangleRec({ground.x - 7.0f * growth - 3.0f, ground.y - trunkH, 14.0f * growth + 6.0f, trunkH},
                     Color{130, 86, 52, 255});
    const float r = 26.0f + 52.0f * growth;
    const Vector2 top{ground.x, ground.y - trunkH - r * 0.4f};
    DrawCircleV(top, r, Color{56, 142, 72, 255});
    DrawCircleV({top.x - r * 0.45f, top.y + r * 0.2f}, r * 0.7f, Color{64, 156, 80, 255});
    DrawCircleV({top.x + r * 0.45f, top.y + r * 0.25f}, r * 0.65f, Color{72, 166, 86, 255});
    // One golden "coin fruit" per 10 coins saved (up to 12).
    const int fruits = std::min(12, savings / 10);
    for (int i = 0; i < fruits; ++i) {
        const float a = F(i) * 2.4f;
        const float d = r * (0.25f + 0.55f * F((i * 37) % 10) / 10.0f);
        DrawCoin({top.x + std::cos(a) * d, top.y + std::sin(a) * d * 0.8f}, 6.0f);
    }
}

}  // namespace

BankScene::BankScene(engine::Application& app, GameSession& session) : Scene(app), s_(session) {
    menu_.focused = 0;
}

BankScene::Layout BankScene::MakeLayout() const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    Layout l;
    l.panel = {w / 2.0f - 500.0f, h / 2.0f - 320.0f, 1000.0f, 640.0f};
    l.minus = {l.panel.x + 360.0f, l.panel.y + 300.0f, 44.0f, 44.0f};
    l.plus = {l.panel.x + 500.0f, l.panel.y + 300.0f, 44.0f, 44.0f};
    l.buttons.push_back({l.panel.x + 260.0f, l.panel.y + 360.0f, 180.0f, 50.0f});
    l.buttons.push_back({l.panel.x + 450.0f, l.panel.y + 360.0f, 180.0f, 50.0f});
    l.buttons.push_back({l.panel.x + 680.0f, l.panel.y + 470.0f, 280.0f, 50.0f});
    l.buttons.push_back({l.panel.x + l.panel.width - 200.0f, l.panel.y + l.panel.height - 76.0f, 160.0f, 50.0f});
    return l;
}

int BankScene::RepayAmount() const {
    const int payment = std::max(1, s_.state.Get("bank.payment"));
    return std::min({payment, s_.state.Get("bank.loan"), s_.state.Get("money")});
}

void BankScene::Update(float /*dt*/) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        App().Scenes().Pop();
        return;
    }
    const Layout l = MakeLayout();
    const int most = std::max(s_.state.Get("money"), s_.state.Get("bank.savings"));
    auto change = [&](int delta) { amount_ = std::clamp(amount_ + delta, 1, std::max(1, most)); };
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT) || IsKeyPressed(KEY_D) || ui::Clicked(l.plus)) {
        change(kStep);
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT) || IsKeyPressed(KEY_A) || ui::Clicked(l.minus)) {
        change(-kStep);
    }

    const bool hasLoan = s_.state.Get("bank.loan") > 0;
    const std::vector<bool> enabled = {amount_ <= s_.state.Get("money"), amount_ <= s_.state.Get("bank.savings"),
                                       hasLoan && RepayAmount() > 0, true};
    switch (menu_.Update(l.buttons, enabled)) {
        case 0:
            if (game::Deposit(s_.state, amount_)) {
                s_.AddToast("Saved " + std::to_string(amount_) + " coins");
            }
            break;
        case 1:
            if (game::Withdraw(s_.state, amount_)) {
                s_.AddToast("Took out " + std::to_string(amount_) + " coins");
            }
            break;
        case 2: {
            const int paid = RepayAmount();
            if (game::RepayLoan(s_.state, paid)) {
                s_.AddToast("Paid back " + std::to_string(paid) + " coins early");
            }
            break;
        }
        case 3:
            App().Scenes().Pop();
            break;
        default:
            break;
    }
}

void BankScene::Draw(float /*alpha*/) const {
    const float w = F(GetScreenWidth());
    const float h = F(GetScreenHeight());
    DrawRectangleRec({0.0f, 0.0f, w, h}, Color{20, 12, 20, 150});
    const Layout l = MakeLayout();
    ui::Panel(l.panel, kPaper, kPanelBorder, 3.0f);
    const game::GameState& st = s_.state;
    const game::EconomyDef& economy = s_.content.economy;

    DrawPortrait({l.panel.x + 64.0f, l.panel.y + 60.0f}, 40.0f, LookOf(s_.content.FindNpc("adeyemi")));
    ui::Text("Town Bank - your bank book", {l.panel.x + 120.0f, l.panel.y + 22.0f}, 32.0f, kInk, true);
    ui::Text("Savers EARN interest. Borrowers PAY interest.", {l.panel.x + 120.0f, l.panel.y + 64.0f}, 19.0f, kMuted);

    // --- Savings ---
    const float sx = l.panel.x + 40.0f;
    DrawSavingsTree({sx + 100.0f, l.panel.y + 300.0f}, st.Get("bank.savings"));
    ui::Text("Savings", {sx + 230.0f, l.panel.y + 120.0f}, 22.0f, kAccent, true);
    ui::Text(std::to_string(st.Get("bank.savings")) + " coins", {sx + 230.0f, l.panel.y + 148.0f}, 34.0f, kInk, true);
    ui::Text("In your pocket: " + std::to_string(st.Get("money")) + " coins", {sx + 230.0f, l.panel.y + 192.0f}, 20.0f,
             kInk);
    const int next = game::NextWeekStart(st.day, economy);
    const int interest = game::SavingsInterest(st.Get("bank.savings"), economy.savingsInterestPercent);
    ui::Text("Every " + std::to_string(economy.weekLength) + " days the bank adds " +
                 std::to_string(economy.savingsInterestPercent) + "% interest.",
             {sx + 230.0f, l.panel.y + 222.0f}, 17.0f, kMuted);
    ui::Text("Next interest: Day " + std::to_string(next) + " (+" + std::to_string(interest) + " coins).  Earned so far: " +
                 std::to_string(st.Get("bank.interest_earned")) + ".",
             {sx + 230.0f, l.panel.y + 244.0f}, 17.0f, kGood);

    ui::Text("Amount", {sx + 230.0f, l.panel.y + 312.0f}, 20.0f, kInk);
    ui::DrawButton(l.minus, "-", false, amount_ > 1, 26.0f);
    ui::TextCentered(std::to_string(amount_), {l.panel.x + 452.0f, l.panel.y + 322.0f}, 28.0f, kInk, true);
    ui::DrawButton(l.plus, "+", false, true, 26.0f);

    // --- Loan ---
    const float lx = l.panel.x + 680.0f;
    ui::Text("Loan", {lx, l.panel.y + 120.0f}, 22.0f, kAccent, true);
    const int owed = st.Get("bank.loan");
    if (owed > 0) {
        ui::Text("Still owed: " + std::to_string(owed) + " coins", {lx, l.panel.y + 150.0f}, 24.0f, kInk, true);
        ui::Text("Weekly payment: " + std::to_string(st.Get("bank.payment")) + " coins", {lx, l.panel.y + 186.0f}, 19.0f,
                 kInk);
        ui::Text("Taken on Day " + std::to_string(next), {lx, l.panel.y + 212.0f}, 19.0f, kInk);
        ui::TextWrapped("Keep enough coins in your pocket on bank day, or a late fee of " +
                            std::to_string(economy.lateFee) + " coins is added.",
                        {lx, l.panel.y + 244.0f, 280.0f, 80.0f}, 16.0f, kMuted);
        ui::TextWrapped("Paying back early means less to worry about later.", {lx, l.panel.y + 420.0f, 280.0f, 40.0f},
                        16.0f, kMuted);
    } else {
        ui::TextWrapped("No loans. You don't owe the bank anything.", {lx, l.panel.y + 150.0f, 280.0f, 60.0f}, 20.0f,
                        kInk);
        ui::TextWrapped("Need money for something big? Ms. Adeyemi can tell you about loans.",
                        {lx, l.panel.y + 214.0f, 280.0f, 60.0f}, 16.0f, kMuted);
    }

    for (std::size_t i = 0; i < l.buttons.size(); ++i) {
        const bool focused = static_cast<int>(i) == menu_.focused;
        bool enabled = true;
        std::string label = kButtons[i];
        if (i == 0) {
            enabled = amount_ <= st.Get("money");
        } else if (i == 1) {
            enabled = amount_ <= st.Get("bank.savings");
        } else if (i == 2) {
            enabled = owed > 0 && RepayAmount() > 0;
            if (enabled) {
                label = "Pay back " + std::to_string(RepayAmount()) + " now";
            }
        }
        if (i == 2 && owed <= 0) {
            continue;
        }
        ui::DrawButton(l.buttons[i], label, focused, enabled, 21.0f);
    }
    ui::Text("Left / Right change the amount", {l.panel.x + 40.0f, l.panel.y + l.panel.height - 60.0f}, 16.0f, kMuted);
}
