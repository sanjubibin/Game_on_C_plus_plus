// Unit tests for the rendering-free game logic. Plain asserts, no framework needed.

#include <json/reader.h>

#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>

#include "logic/Content.h"
#include "logic/Encounter.h"
#include "logic/GameState.h"
#include "logic/Rules.h"
#include "logic/SaveGame.h"
#include "logic/Selling.h"
#include "logic/ShopSim.h"

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        ++g_checks;                                                                    \
        if (!(cond)) {                                                                 \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                              \
        }                                                                              \
    } while (0)

Json::Value Parse(const std::string& text) {
    Json::CharReaderBuilder builder;
    Json::Value value;
    std::string errors;
    std::istringstream in(text);
    Json::parseFromStream(builder, in, &value, &errors);
    return value;
}

void TestClamping() {
    game::GameState s;
    s.Set("money", -5);
    CHECK(s.Get("money") == 0);
    s.Set("maya.trust", 150);
    CHECK(s.Get("maya.trust") == 100);
    s.Add("skill.empathy", 20);
    CHECK(s.Get("skill.empathy") == 10);
    s.Set("reputation", -3);
    CHECK(s.Get("reputation") == 0);
}

void TestConditions() {
    game::GameState s;
    s.day = 3;
    s.minutes = 14 * 60;
    s.SetFlag("got_pushcart");
    s.Set("money", 25);
    s.party = {"maya"};

    CHECK(game::CheckCondition(Json::Value(), s));
    CHECK(game::CheckCondition(Parse(R"({"flag": "got_pushcart"})"), s));
    CHECK(!game::CheckCondition(Parse(R"({"flag": ["got_pushcart", "rent_paid"]})"), s));
    CHECK(game::CheckCondition(Parse(R"({"not_flag": "rent_paid"})"), s));
    CHECK(game::CheckCondition(Parse(R"({"day_min": 3, "day_max": 3})"), s));
    CHECK(!game::CheckCondition(Parse(R"({"day_min": 4})"), s));
    CHECK(game::CheckCondition(Parse(R"({"hour_min": 14, "hour_below": 15})"), s));
    CHECK(game::CheckCondition(Parse(R"({"min": {"money": 25}})"), s));
    CHECK(!game::CheckCondition(Parse(R"({"min": {"money": 26}})"), s));
    CHECK(game::CheckCondition(Parse(R"({"below": {"money": 26}})"), s));
    CHECK(game::CheckCondition(Parse(R"({"in_party": "maya", "not_in_party": "leo"})"), s));
    CHECK(!game::CheckCondition(Parse(R"({"unknown_key": 1})"), s));
}

void TestEffects() {
    game::GameState s;
    s.Set("money", 30);
    game::ApplyEffects(Parse(R"({
        "add": {"maya.trust": 5},
        "set_flag": ["a", "b"],
        "join_party": "leo",
        "buy_stock": {"item": "biscuits", "qty": 20, "unit_cost": 1, "dodgy": true},
        "advance_minutes": 120,
        "log": "hello",
        "schedule": {"event": "later_event", "days": 2}
    })"), s);
    CHECK(s.Get("maya.trust") == 5);
    CHECK(s.HasFlag("a") && s.HasFlag("b"));
    CHECK(s.InParty("leo"));
    CHECK(s.Get("money") == 10);
    CHECK(s.StockOf("biscuits") == 20);
    CHECK(s.minutes == 8 * 60 + 120);
    CHECK(s.todayLog.size() == 1);
    CHECK(s.scheduled.size() == 1 && s.scheduled[0].day == 3);

    game::ApplyEffects(Parse(R"({"remove_dodgy": true, "clear_flag": "a"})"), s);
    CHECK(s.StockOf("biscuits") == 0);
    CHECK(!s.HasFlag("a"));
}

void TestStockFifo() {
    game::GameState s;
    s.AddStock({"apples", 3, 1, 1, false});
    s.AddStock({"apples", 2, 2, 2, false});
    s.AddStock({"apples", 1, 2, 2, false});  // merges with the previous batch
    CHECK(s.stock.size() == 2);
    CHECK(s.StockOf("apples") == 6);
    // Oldest first: 3 at 1 coin, then 1 at 2 coins.
    CHECK(s.TakeStock("apples", 4) == 5);
    CHECK(s.StockOf("apples") == 2);
    CHECK(s.TakeStock("apples", 10) == 4);
    CHECK(s.stock.empty());
}

void TestShopSim() {
    game::SellParams p;
    p.items = {{"lemonade", 3, 3, 10, 1.0f}, {"biscuits", 4, 4, 2, 1.0f}};
    p.reputation = 50;
    p.seed = 42;

    const game::SellResult a = game::SimulateSales(p);
    const game::SellResult b = game::SimulateSales(p);
    CHECK(a.revenue == b.revenue);  // deterministic
    CHECK(static_cast<int>(a.customers.size()) == game::CustomerCount(p));
    CHECK(a.sold.count("biscuits") == 0 || a.sold.at("biscuits") <= 2);  // never beyond stock
    int paid = 0;
    for (const game::Customer& c : a.customers) {
        paid += c.paid;
    }
    CHECK(paid == a.revenue);
    CHECK(a.TotalSold() + a.tooExpensive + a.soldOut == static_cast<int>(a.customers.size()));

    // Nobody pays more than 1.5x the usual value.
    p.items = {{"lemonade", 50, 3, 10, 1.0f}};
    const game::SellResult pricey = game::SimulateSales(p);
    CHECK(pricey.revenue == 0);
    CHECK(pricey.tooExpensive == static_cast<int>(pricey.customers.size()));

    // A bright sign brings more customers.
    game::SellParams more = p;
    more.customerBonus = 0.5f;
    CHECK(game::CustomerCount(more) > game::CustomerCount(p));
}

void TestDayCycle() {
    game::Content content;
    game::ItemDef lemonade;
    lemonade.id = "lemonade";
    lemonade.name = "Lemonade";
    lemonade.shelfDays = 1;
    content.items["lemonade"] = lemonade;

    game::GameState s;
    s.SetFlag("today.something");
    s.SetFlag("story");
    s.AddStock({"lemonade", 4, 1, 1, false});
    s.scheduled.push_back({2, "due"});
    s.scheduled.push_back({5, "not_yet"});

    game::EndDay(s);
    const std::vector<std::string> events = game::StartDay(s, content);
    CHECK(s.day == 2);
    CHECK(s.minutes == 8 * 60);
    CHECK(!s.HasFlag("today.something"));
    CHECK(s.HasFlag("story"));
    CHECK(s.StockOf("lemonade") == 0);  // went flat overnight
    CHECK(events.size() == 1 && events[0] == "due");
    CHECK(s.scheduled.size() == 1);
}

void TestSaveRoundTrip() {
    game::GameState s;
    s.day = 4;
    s.minutes = 600;
    s.seed = 987654321u;
    s.Set("money", 42);
    s.SetFlag("rent_paid");
    s.seenEvents.insert("intro");
    s.party = {"maya", "leo"};
    s.AddStock({"biscuits", 5, 2, 3, true});
    s.prices["biscuits"] = 4;
    s.scheduled.push_back({6, "x"});
    s.todayLog.push_back("line");

    const std::string path =
        (std::filesystem::temp_directory_path() / "fun_cpp_save_test.json").string();
    std::string error;
    CHECK(game::SaveGame(path, s, &error));
    game::GameState loaded;
    CHECK(game::LoadGame(path, loaded, &error));
    std::filesystem::remove(path);

    CHECK(loaded.day == 4 && loaded.minutes == 600 && loaded.seed == 987654321u);
    CHECK(loaded.Get("money") == 42);
    CHECK(loaded.HasFlag("rent_paid"));
    CHECK(loaded.seenEvents.contains("intro"));
    CHECK(loaded.party.size() == 2 && loaded.party[1] == "leo");
    CHECK(loaded.stock.size() == 1 && loaded.stock[0].dodgy && loaded.stock[0].qty == 5);
    CHECK(loaded.prices["biscuits"] == 4);
    CHECK(loaded.scheduled.size() == 1 && loaded.scheduled[0].event == "x");
    CHECK(loaded.todayLog.size() == 1);
}

void TestRealContent() {
    game::Content content;
    try {
        content = game::Content::Load(FUN_ASSETS_DIR);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Content failed to load: %s\n", e.what());
        CHECK(false);
        return;
    }
    const std::vector<std::string> problems = game::ValidateContent(content);
    for (const std::string& p : problems) {
        std::fprintf(stderr, "content: %s\n", p.c_str());
    }
    CHECK(problems.empty());
    CHECK(content.StartMap().width == 40 && content.StartMap().height == 26);
    CHECK(content.FindMap("market") != nullptr);
    CHECK(content.FindVenue("pushcart") != nullptr && content.FindVenue("stall") != nullptr);
    CHECK(content.quests.size() >= 3);
    CHECK(content.FindEncounter("leo_alone") != nullptr);
}

void TestChapterOneWiring() {
    game::Content content;
    try {
        content = game::Content::Load(FUN_ASSETS_DIR);
    } catch (const std::exception&) {
        CHECK(false);
        return;
    }
    auto talk = [&](const game::GameState& s, const std::string& npc) {
        const game::NpcDef* def = content.FindNpc(npc);
        return def ? game::PickTalkEvent(*def, s) : std::string("<missing>");
    };
    auto visibleChoices = [&](const game::GameState& s, const std::string& event) {
        std::vector<std::string> labels;
        for (const game::ChoiceDef& c : content.FindEvent(event)->choices) {
            if (game::CheckCondition(c.showWhen, s)) {
                labels.push_back(c.label);
            }
        }
        return labels;
    };

    game::GameState s;
    s.SetFlag("first_sale");
    s.day = 4;
    s.seenEvents = {"morning_day2", "morning_stranger_rumor"};
    std::vector<std::string> morning = game::StartDay(s, content);
    CHECK(morning.size() == 1 && morning[0] == "morning_market_news");

    // Mr. Okafor: introduction first, then the offer. A discount swaps the price shown.
    CHECK(talk(s, "okafor") == "okafor_intro");
    s.seenEvents.insert("okafor_intro");
    CHECK(talk(s, "okafor") == "okafor_offer");
    std::vector<std::string> choices = visibleChoices(s, "okafor_offer");
    CHECK(choices.size() == 3 && choices[1] == "Rent the stall (60 coins)");
    s.SetFlag("stall_discount");
    s.SetFlag("okafor_negotiated");
    choices = visibleChoices(s, "okafor_offer");
    CHECK(choices.size() == 2 && choices[0] == "Rent the stall (45 coins)");

    // Kofi is friendly until your stall competes with his.
    CHECK(talk(s, "kofi") == "kofi_hello");
    const game::NpcDef* grumble = content.FindNpc("grumble");
    CHECK(!game::CheckCondition(grumble->presentWhen, s));
    s.SetFlag("stall_rented");
    CHECK(talk(s, "kofi") == "kofi_confront");
    CHECK(!talk(s, "okafor").starts_with("okafor_"));
    s.SetFlag("stall_sale");
    CHECK(game::CheckCondition(grumble->presentWhen, s));

    // New stock unlocks; apples go once you've promised Kofi.
    CHECK(game::ItemAvailable(content.items.at("sandwiches"), s));
    CHECK(!game::ItemAvailable(content.items.at("cookies"), s));
    CHECK(game::ItemAvailable(content.items.at("apples"), s));
    s.SetFlag("kofi_friend");
    CHECK(!game::ItemAvailable(content.items.at("apples"), s));

    // Festival Day comes first on Day 14, and the chapter ends that evening.
    s.day = 14;
    morning = game::StartDay(s, content);
    CHECK(!morning.empty() && morning[0] == "festival_morning");
    s.SetFlag("stall_festival_sale");
    CHECK(game::PickEveningEvent(s, content) == "chapter1_end_great");
    game::GameState missed = s;
    missed.ClearFlag("stall_festival_sale");
    CHECK(game::PickEveningEvent(missed, content) == "chapter1_end_ok");
}

void TestSellingSession() {
    game::Content content;
    game::ItemDef lemonade;
    lemonade.id = "lemonade";
    lemonade.name = "Lemonade";
    lemonade.cost = 1;
    lemonade.value = 3;
    content.items["lemonade"] = lemonade;
    content.itemOrder = {"lemonade"};
    game::NpcDef leo;
    leo.id = "leo";
    leo.name = "Leo";
    leo.bonus = {0.15f, "Leo's sign"};
    content.npcs.push_back(leo);

    game::VenueDef stall;
    stall.id = "stall";
    stall.name = "Stall";
    stall.helper = "maya";
    stall.fee = 5;
    stall.customers = 1.7f;
    stall.bonuses.push_back({Parse(R"({"flag": "kofi_friend"})"), 0.1f, "Kofi"});

    game::GameState s;
    s.Set("money", 20);
    s.Set("reputation", 40);
    s.party = {"maya", "leo"};
    s.minutes = 10 * 60;
    s.AddStock({"lemonade", 30, 1, 1, false});
    s.prices["lemonade"] = 2;  // below the usual value: everyone buys

    CHECK(game::CannotOpenReason(s, content, stall).empty());
    CHECK(game::HelperCanRun(s, content, stall));
    const game::SessionReport r = game::RunSellSession(s, content, stall, false);
    CHECK(r.fee == 5);
    CHECK(r.sales.tooExpensive == 0);
    CHECK(r.costOfSold == r.sales.TotalSold());
    CHECK(s.Get("money") == 20 + r.sales.revenue - 5);
    CHECK(r.Profit() == r.sales.revenue - r.costOfSold - 5);
    CHECK(s.minutes == 13 * 60);
    CHECK(s.HasFlag("first_sale") && s.HasFlag("stall_sale"));
    CHECK(r.bonusTexts.size() == 1);  // Leo, but not Kofi yet
    CHECK(r.leftover.at("lemonade") == 30 - r.sales.TotalSold());

    // A helper keeps your clock still and can only run it once a day.
    s.SetFlag("today.festival");
    const int before = s.minutes;
    const game::SessionReport helped = game::RunSellSession(s, content, stall, true);
    CHECK(helped.helperRan);
    CHECK(s.minutes == before);
    CHECK(s.HasFlag("today.helper_ran") && s.HasFlag("stall_festival_sale"));
    CHECK(!game::HelperCanRun(s, content, stall));

    // Can't open without the fee or after 17:00.
    game::GameState poor = s;
    poor.Set("money", 2);
    CHECK(!game::CannotOpenReason(poor, content, stall).empty());
    game::GameState late = s;
    late.Set("money", 50);
    late.minutes = 17 * 60;
    CHECK(!game::CannotOpenReason(late, content, stall).empty());

    // Unlock and hide flags control what the wholesaler sells.
    game::ItemDef cookies;
    cookies.unlockFlag = "mom_recipe";
    cookies.hideFlag = "never";
    CHECK(!game::ItemAvailable(cookies, s));
    s.SetFlag("mom_recipe");
    CHECK(game::ItemAvailable(cookies, s));
    s.SetFlag("never");
    CHECK(!game::ItemAvailable(cookies, s));
}

void TestEncounterRules() {
    game::Content content;
    try {
        content = game::Content::Load(FUN_ASSETS_DIR);
    } catch (const std::exception&) {
        CHECK(false);
        return;
    }
    const game::EncounterDef* leo = content.FindEncounter("leo_alone");
    if (leo == nullptr) {
        CHECK(false);
        return;
    }
    auto action = [&](const std::string& id) -> const game::EncounterAction& {
        for (const game::EncounterAction& a : leo->actions) {
            if (a.id == id) {
                return a;
            }
        }
        return leo->actions.front();
    };
    game::GameState s;
    s.Set("skill.empathy", 1);
    s.Set("skill.creativity", 1);
    s.Set("skill.communication", 1);
    s.party = {"maya"};

    // Listening first makes the invitation land.
    game::EncounterState good = game::BeginEncounter(*leo);
    for (const char* id : {"listen", "joke", "maya", "invite"}) {
        game::UseAction(*leo, good, action(id), s);
    }
    CHECK(good.result == game::EncounterResult::Won);

    // Inviting before listening falls flat, and each idea works only once.
    game::EncounterState rushed = game::BeginEncounter(*leo);
    game::UseAction(*leo, rushed, action("invite"), s);
    CHECK(rushed.lastDelta == action("invite").baseUnrevealed);
    CHECK(!game::CanUse(rushed, action("invite"), s));
    for (const char* id : {"listen", "joke", "maya"}) {
        game::UseAction(*leo, rushed, action(id), s);
    }
    CHECK(rushed.result == game::EncounterResult::Lost);

    // Without Maya on the team her action is unavailable.
    game::GameState alone = s;
    alone.party.clear();
    CHECK(!game::CanUse(game::BeginEncounter(*leo), action("maya"), alone));

    // Leaving ends it straight away.
    game::EncounterState left = game::BeginEncounter(*leo);
    game::UseAction(*leo, left, action("leave"), s);
    CHECK(left.result == game::EncounterResult::Lost);
}

void TestLifeWheelAndBedtime() {
    game::Content content;
    try {
        content = game::Content::Load(FUN_ASSETS_DIR);
    } catch (const std::exception&) {
        CHECK(false);
        return;
    }
    game::GameState s;
    s.Set("money", 250);
    s.Set("health", 70);
    s.Set("happiness", 60);
    s.Set("reputation", 45);
    s.Set("skill.empathy", 3);
    s.Set("skill.money_sense", 2);
    s.Set("maya.trust", 80);
    s.Set("maya.fun", 40);
    s.party = {"maya"};
    const game::LifeWheel wheel = game::ComputeLifeWheel(s, content);
    CHECK(wheel.money == 100);
    CHECK(wheel.health == 70);
    CHECK(wheel.relationships == 80);  // only Maya has been met
    CHECK(wheel.happiness == 50);
    CHECK(wheel.knowledge == 10);
    CHECK(wheel.community == 45);

    s.minutes = 18 * 60;
    game::ApplyBedtime(s, false);
    CHECK(s.Get("health") == 75);
    game::ApplyBedtime(s, true);
    CHECK(s.Get("health") == 67);
    s.Set("health", 30);
    CHECK(game::IsTired(s));

    game::ApplyEffects(Parse(R"({"memory": "Met Leo"})"), s);
    game::ApplyEffects(Parse(R"({"memory": "Met Leo"})"), s);
    CHECK(s.memories.size() == 1);
}

// Walks the main story beats with the real content and checks who says what.
void TestStoryWiring() {
    game::Content content;
    try {
        content = game::Content::Load(FUN_ASSETS_DIR);
    } catch (const std::exception&) {
        CHECK(false);
        return;
    }
    auto talk = [&](const game::GameState& s, const std::string& npc) {
        const game::NpcDef* def = content.FindNpc(npc);
        return def ? game::PickTalkEvent(*def, s) : std::string("<missing>");
    };
    auto play = [&](game::GameState& s, const std::string& event, int choice = -1) {
        const game::EventDef* def = content.FindEvent(event);
        if (def == nullptr) {
            CHECK(false);
            return;
        }
        s.seenEvents.insert(event);
        game::ApplyEffects(def->effects, s);
        if (choice >= 0) {
            game::ApplyEffects(def->choices[static_cast<std::size_t>(choice)].effects, s);
        }
    };

    game::GameState s;
    for (const std::string& stat : content.start.stats.getMemberNames()) {
        s.Set(stat, content.start.stats[stat].asInt());
    }
    s.party = content.start.party;
    game::StartDay(s, content);
    play(s, "intro");

    CHECK(talk(s, "grandpa") == "grandpa_pushcart");
    CHECK(talk(s, "dad") == "dad_worried");
    CHECK(talk(s, "leo") == "leo_meet");
    play(s, "grandpa_pushcart", 0);
    CHECK(s.HasFlag("got_pushcart"));
    CHECK(talk(s, "grandpa").starts_with("idle."));
    CHECK(talk(s, "mom") == "mom_budget_tip");

    // The stranger only shows up from day 3.
    const game::NpcDef* stranger = content.FindNpc("stranger");
    CHECK(!game::CheckCondition(stranger->presentWhen, s));
    s.day = 3;
    CHECK(game::CheckCondition(stranger->presentWhen, s));
    play(s, "stranger_offer", 2);  // "I'll ask my grandpa first."
    CHECK(talk(s, "grandpa") == "grandpa_scam_story");
    play(s, "grandpa_scam_story");
    CHECK(s.HasFlag("scam_resolved"));
    CHECK(!game::CheckCondition(stranger->presentWhen, s));

    // Saving 50 coins lets you pay the rent.
    s.Set("money", 50);
    CHECK(talk(s, "dad") == "dad_rent_offer");
    play(s, "dad_rent_offer", 0);
    CHECK(s.HasFlag("rent_paid") && s.Get("money") == 0);
    CHECK(talk(s, "dad") == "dad_proud");

    // Evenings: first sale dinner, then Maya's story the next evening, then the week's end.
    s.SetFlag("first_sale");
    CHECK(game::PickEveningEvent(s, content) == "dinner_first_sale");
    s.seenEvents.insert("dinner_first_sale");
    CHECK(game::PickEveningEvent(s, content) == "maya_took_coins");
    s.seenEvents.insert("maya_took_coins");
    s.day = 7;
    CHECK(game::PickEveningEvent(s, content) == "week_end_success");

    // Winning Leo over makes him a follower.
    play(s, "leo_joins");
    CHECK(s.InParty("leo"));
    CHECK(talk(s, "leo").starts_with("idle."));
}

}  // namespace

int main() {
    TestClamping();
    TestConditions();
    TestEffects();
    TestStockFifo();
    TestShopSim();
    TestDayCycle();
    TestSaveRoundTrip();
    TestRealContent();
    TestStoryWiring();
    TestChapterOneWiring();
    TestSellingSession();
    TestEncounterRules();
    TestLifeWheelAndBedtime();

    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
