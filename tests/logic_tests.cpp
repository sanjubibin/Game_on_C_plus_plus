// Unit tests for the rendering-free game logic. Plain asserts, no framework needed.

#include <json/reader.h>

#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>

#include "logic/Bank.h"
#include "logic/Content.h"
#include "logic/Encounter.h"
#include "logic/GameState.h"
#include "logic/Legacy.h"
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

void TestChapterTwoWiring() {
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
    auto play = [&](game::GameState& s, const std::string& event, int choice = -1) {
        const game::EventDef* def = content.FindEvent(event);
        if (def == nullptr) {
            CHECK(false);
            return;
        }
        s.seenEvents.insert(event);
        game::ApplyEffects(def->effects, s);
        game::Schedule(s, def->later);
        if (choice >= 0) {
            game::ApplyEffects(def->choices[static_cast<std::size_t>(choice)].effects, s);
        }
    };

    CHECK(content.ChapterTitle(2) == "The Corner Shop");
    game::GameState s;
    s.Set("chapter", 2);
    s.Set("money", 40);
    s.day = 14;
    s.seenEvents = {"festival_morning", "morning_day2", "morning_stranger_rumor", "morning_market_news"};
    game::EndDay(s);
    std::vector<std::string> morning = game::StartDay(s, content);
    CHECK(morning.size() == 1 && morning[0] == "ch2_start");
    CHECK(visibleChoices(s, "shop_door").size() == 1);  // just looking, before the partnership
    play(s, "ch2_partner");
    CHECK(s.HasFlag("shop_partner"));
    CHECK(visibleChoices(s, "shop_door").size() == 3);
    CHECK(talk(s, "dad") == "dad_partner");

    const game::QuestDef* tracked = game::TrackedQuest(s, content);
    CHECK(tracked != nullptr && tracked->chapter == 2);

    // Upgrades: the fridge option disappears once bought.
    CHECK(visibleChoices(s, "upgrade_menu").size() == 5);
    play(s, "dad_fridge_plan");
    CHECK(s.HasFlag("bank_news"));
    CHECK(visibleChoices(s, "upgrade_menu").size() == 4);

    // The bank: introduction, then a loan after the interview.
    CHECK(talk(s, "adeyemi") == "adeyemi_intro");
    s.seenEvents.insert("adeyemi_intro");
    CHECK(talk(s, "adeyemi") == "adeyemi_menu");
    CHECK(visibleChoices(s, "adeyemi_menu").size() == 3);
    play(s, "loan_offer_good", 0);
    CHECK(s.Get("money") == 120 && s.Get("bank.loan") == 88 && s.Get("bank.payment") == 22);
    CHECK(visibleChoices(s, "adeyemi_menu").size() == 2);  // no second loan
    play(s, "upgrade_menu", 0);
    CHECK(s.HasFlag("fridge") && s.Get("money") == 40);
    CHECK(game::ItemAvailable(content.items.at("milkshakes"), s));

    // The farm: strawberries only once you've met Farmer Ama.
    CHECK(!game::ItemAvailable(content.items.at("strawberries"), s));
    CHECK(talk(s, "ama") == "ama_intro");
    play(s, "ama_intro");
    CHECK(game::ItemAvailable(content.items.at("strawberries"), s));
    CHECK(talk(s, "ama") == "ama_harvest");
    play(s, "ama_harvest", 0);
    CHECK(s.StockOf("strawberries") == 6 && talk(s, "ama").starts_with("idle."));

    // Fair pay sets the wage policy.
    play(s, "staff_pay_talk", 1);
    CHECK(s.Get("staff.wage") == 3 && s.Get("staff.morale_pct") == 100);

    // Price war: lemonade is worth less... unless you have a fridge.
    game::GameState noFridge = s;
    noFridge.ClearFlag("fridge");
    play(noFridge, "price_war_start");
    CHECK(game::ItemValue(content.items.at("lemonade"), noFridge) == 2);
    CHECK(game::ItemValue(content.items.at("lemonade"), s) == 3);
    CHECK(noFridge.scheduled.size() == 1 && noFridge.scheduled[0].event == "price_war_ends");

    // A week starts: the loan payment comes out first thing.
    s.day = 21;
    s.seenEvents.insert({"ch2_start", "morning_farm_news", "staff_pay_talk", "price_war_start", "price_war_start_fridge"});
    game::EndDay(s);
    game::StartDay(s, content);
    CHECK(s.Get("bank.loan") == 66 && s.Get("money") == 18);

    // Day 28 ends the chapter.
    s.day = 28;
    CHECK(game::PickEveningEvent(s, content) == "chapter2_end");
}

void TestChainTier() {
    // Bulk prices kick in at the threshold.
    const game::SupplierItem sandwiches{"sandwiches", 3, 20, 2};
    CHECK(sandwiches.UnitCostFor(19) == 3 && sandwiches.UnitCostFor(20) == 2 && sandwiches.UnitCostFor(50) == 2);
    const game::SupplierItem plain{"lemonade", 1, 0, 0};
    CHECK(!plain.HasBulkPrice() && plain.UnitCostFor(100) == 1);

    // Rent comes out on bank day: pocket first, then savings, else it's late.
    game::EconomyDef economy;
    game::GameState s;
    s.SetFlag("branch_open");
    s.Set("branch.rent", 20);
    s.Set("money", 30);
    game::ProcessWeek(s, economy);
    CHECK(s.Get("money") == 10);
    s.Set("bank.savings", 15);
    game::ProcessWeek(s, economy);  // 10 from pocket, 10 from savings (savings earn 1 first)
    CHECK(s.Get("money") == 0 && s.Get("bank.savings") == 6 && !s.HasFlag("rent_late"));
    s.Set("reputation", 50);
    game::ProcessWeek(s, economy);  // 0 + 6 isn't enough
    CHECK(s.HasFlag("rent_late") && s.Get("reputation") == 45 && s.Get("bank.savings") == 6);

    // The manager runs the city shop in the evening, for the team wage.
    game::Content content;
    game::ItemDef pretzels;
    pretzels.id = "pretzels";
    pretzels.name = "Pretzels";
    pretzels.value = 5;
    content.items["pretzels"] = pretzels;
    content.itemOrder = {"pretzels"};
    game::NpcDef priya;
    priya.id = "priya";
    priya.name = "Priya";
    priya.staff = {true, 0.85f};
    content.npcs.push_back(priya);
    game::VenueDef branch;
    branch.id = "branch";
    branch.name = "City Shop";
    branch.customers = 1.8f;
    content.venues["branch"] = branch;

    game::GameState day;
    day.Set("money", 20);
    day.Set("staff.wage", 3);
    day.SetFlag("branch_open");
    CHECK(game::BranchManager(day).empty());
    CHECK(!game::RunBranchDay(day, content));  // no manager yet
    CHECK(day.todayLog.size() == 1);
    day.SetFlag("manager_chosen");
    day.SetFlag("manager_priya");
    CHECK(game::BranchManager(day) == "priya");
    CHECK(!game::RunBranchDay(day, content));  // no stock
    day.AddStock({"pretzels", 40, 1, 1, false});
    day.prices["pretzels"] = 4;
    const int minutes = day.minutes;
    CHECK(game::RunBranchDay(day, content));
    CHECK(day.minutes == minutes && day.HasFlag("today.staff_priya") && day.StockOf("pretzels") < 40);
    CHECK(!game::RunBranchDay(day, content));  // only once a day
    game::GameState upset = day;
    upset.flags.erase("today.staff_priya");
    upset.flags.erase("today.staffed_branch");
    upset.SetFlag("priya_upset");
    CHECK(!game::RunBranchDay(upset, content));

    // Travel actions.
    game::TravelTarget target;
    CHECK(game::ParseTravel("travel:city:4:12", target) && target.map == "city" && target.x == 4 && target.y == 12);
    CHECK(!game::ParseTravel("travel:city:4", target) && !game::ParseTravel("event:x", target));
}

void TestChapterThreeWiring() {
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

    CHECK(content.ChapterTitle(3) == "Two Shops");
    CHECK(content.FindMap("city") != nullptr && content.FindSupplier("city") != nullptr);

    game::GameState s;
    s.Set("chapter", 3);
    s.day = 28;
    s.seenEvents = {"ch2_start", "morning_farm_news", "staff_pay_talk", "price_war_start", "price_war_start_fridge",
                    "morning_day2", "morning_stranger_rumor", "morning_market_news"};
    game::EndDay(s);
    std::vector<std::string> morning = game::StartDay(s, content);
    CHECK(morning.size() == 1 && morning[0] == "ch3_start");
    play(s, "ch3_start");
    CHECK(s.HasFlag("city_news"));
    const game::QuestDef* tracked = game::TrackedQuest(s, content);
    CHECK(tracked != nullptr && tracked->id == "two_shops");

    // The bus costs coins; walking home is free but slow.
    s.Set("money", 1);
    const game::EventDef* bus = content.FindEvent("bus_stop_city");
    CHECK(!game::CheckCondition(bus->choices[0].requirement, s));
    CHECK(game::CheckCondition(bus->choices[1].requirement, s));

    // Mrs. Osei: the discount swaps which rent option you see.
    s.Set("money", 100);
    CHECK(talk(s, "osei") == "osei_intro");
    s.seenEvents.insert("osei_intro");
    CHECK(visibleChoices(s, "osei_offer").size() == 3);
    play(s, "osei_discount");
    std::vector<std::string> offer = visibleChoices(s, "osei_offer");
    CHECK(offer.size() == 2 && offer[0] == "Take the shop (40 deposit, 15 a week)");
    play(s, "osei_offer", 2);
    CHECK(s.HasFlag("branch_open") && s.Get("branch.rent") == 15 && s.Get("money") == 60);

    // Choosing Priya when Leo is on the team hurts Leo's feelings - and he won't work.
    s.party = {"maya", "leo", "priya"};
    std::vector<std::string> managers = visibleChoices(s, "choose_manager");
    CHECK(managers.size() == 3);  // Priya, Leo, Grandpa
    play(s, "choose_manager", 1);
    CHECK(game::BranchManager(s) == "priya" && s.HasFlag("leo_upset"));
    CHECK(talk(s, "leo") == "leo_hurt");
    play(s, "leo_valued");
    CHECK(!s.HasFlag("leo_upset") && s.HasFlag("head_of_design"));
    CHECK(!talk(s, "leo").starts_with("leo_hurt"));

    // The City Market has bulk prices; pretzels unlock after meeting Mr. Haddad.
    const game::SupplierDef* city = content.FindSupplier("city");
    bool anyBulk = false;
    for (const game::SupplierItem& item : city->items) {
        anyBulk = anyBulk || item.HasBulkPrice();
    }
    CHECK(anyBulk);
    CHECK(!game::ItemAvailable(content.items.at("pretzels"), s));
    play(s, "haddad_intro");
    CHECK(game::ItemAvailable(content.items.at("pretzels"), s));

    // Day 42 ends the chapter - with or without a second shop.
    s.day = 42;
    CHECK(game::PickEveningEvent(s, content) == "chapter3_end_branch");
    game::GameState noBranch = s;
    noBranch.ClearFlag("branch_open");
    CHECK(game::PickEveningEvent(noBranch, content) == "chapter3_end_small");
}

void TestGenerations() {
    game::Content content;
    game::NpcDef leo;
    leo.id = "leo";
    leo.name = "Leo";
    leo.relationship = true;
    content.npcs.push_back(leo);
    game::NpcDef nia;
    nia.id = "nia";
    nia.name = "Nia";
    nia.relationship = true;
    nia.generation = 2;
    content.npcs.push_back(nia);
    content.morning.push_back({Json::Value(), "first_gen_morning", false, 1});
    content.morning.push_back({Json::Value(), "second_gen_morning", false, 2});
    content.events["first_gen_morning"].id = "first_gen_morning";
    content.events["second_gen_morning"].id = "second_gen_morning";
    game::QuestDef oldQuest;
    oldQuest.id = "old";
    oldQuest.steps.push_back({"step", "never"});
    content.quests.push_back(oldQuest);

    game::GameState s;
    CHECK(game::CurrentGeneration(s) == 1);
    CHECK(game::GetQuestStatus(content.quests[0], s) == game::QuestStatus::Active);
    s.day = 43;
    s.Set("money", 150);
    s.Set("bank.savings", 70);
    s.Set("bank.loan", 20);
    s.Set("reputation", 70);
    s.Set("skill.money_sense", 6);
    s.Set("skill.empathy", 4);
    s.Set("leo.trust", 85);
    s.party = {"maya", "leo"};
    s.seenEvents.insert("leo_meet");
    for (const char* flag : {"shop_partner", "branch_open", "leo_joined", "kofi_friend", "mom_recipe", "first_sale",
                             "pushcart_sale", "manager_priya", "today.hot", "visited_city", "has_loan"}) {
        s.SetFlag(flag);
    }
    s.AddStock({"lemonade", 5, 1, 40, false});

    const game::LegacyReport report = game::StartNextGeneration(s, content);
    CHECK(report.generation == 1 && report.wealth == 200);
    CHECK(report.gift == 40 && report.childSavings == 50);
    CHECK(report.familyReputation == 50);
    CHECK(report.familyTalent == "Money sense");
    bool leoLine = false;
    for (const std::string& line : report.legacies) {
        leoLine = leoLine || line.find("Leo became an artist") != std::string::npos;
    }
    CHECK(leoLine);

    // A fresh start for the child...
    CHECK(game::CurrentGeneration(s) == 2 && s.day == 1 && s.minutes == 8 * 60);
    CHECK(s.Get("money") == 40 && s.Get("bank.savings") == 50 && s.Get("bank.loan") == 0);
    CHECK(s.Get("skill.money_sense") == 2 && s.Get("skill.empathy") == 1);
    CHECK(s.party.size() == 1 && s.party[0] == "nia" && s.stock.empty());
    for (const char* gone : {"first_sale", "pushcart_sale", "manager_priya", "today.hot", "shop_partner", "branch_open"}) {
        CHECK(!s.HasFlag(gone));
    }
    // ...in a town that remembers.
    for (const char* kept : {"leo_joined", "kofi_friend", "mom_recipe", "visited_city", "next_generation", "legacy_branch"}) {
        CHECK(s.HasFlag(kept));
    }
    CHECK(s.Get("leo2.trust") == 85);
    CHECK(s.seenEvents.contains("leo_meet"));

    // Content belongs to its generation.
    CHECK(game::GetQuestStatus(content.quests[0], s) == game::QuestStatus::Hidden);
    CHECK(!game::HasMet(content.npcs[0], s, content));  // grown-up Leo is a different character now
    CHECK(game::HasMet(content.npcs[1], s, content));   // Nia is on the team
    const std::vector<std::string> morning = game::StartDay(s, content);
    CHECK(morning.size() == 1 && morning[0] == "second_gen_morning");

    // What the child calls their parent.
    game::ApplyEffects(Parse(R"({"name": {"parent": "Dad"}})"), s);
    CHECK(s.names.at("parent") == "Dad");
}

void TestChapterFourWiring() {
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
            game::Schedule(s, def->choices[static_cast<std::size_t>(choice)].later);
        }
    };

    CHECK(content.ChapterTitle(4) == "Roots & Wings");
    game::GameState s;
    s.Set("chapter", 4);
    s.day = 42;
    s.seenEvents = {"ch3_start", "ch3_bulk_tip", "fair_soon", "ch2_start", "morning_farm_news", "staff_pay_talk",
                    "price_war_start", "price_war_start_fridge", "morning_day2", "morning_stranger_rumor",
                    "morning_market_news"};
    game::EndDay(s);
    std::vector<std::string> morning = game::StartDay(s, content);
    CHECK(morning.size() == 1 && morning[0] == "generation_turn");
    // "Not yet" asks again a week later.
    play(s, "generation_turn", 1);
    CHECK(s.scheduled.size() == 1 && s.scheduled[0].day == 50 && s.scheduled[0].event == "generation_turn");

    s.SetFlag("maya_forgiven");
    s.SetFlag("kofi_friend");
    s.Set("money", 120);
    game::StartNextGeneration(s, content);
    CHECK(game::CurrentGeneration(s) == 2);
    play(s, "g2_intro", 1);
    CHECK(s.names.at("parent") == "Dad" && s.HasFlag("g2_started"));
    play(s, "g2_nia_joins");
    const game::QuestDef* tracked = game::TrackedQuest(s, content);
    CHECK(tracked != nullptr && tracked->id == "roots_and_wings");

    // The grown-ups remember what you did twenty years ago.
    CHECK(talk(s, "aunt_maya") == "g2_maya_story_forgiven");
    CHECK(talk(s, "kofi2") == "g2_kofi_story_friend");
    CHECK(talk(s, "leo2") == "g2_leo_story");  // you never invited Leo in this playthrough
    CHECK(talk(s, "great_grandpa") == "g2_gg_story");

    // Giving back: choose a project, save 40 coins, donate.
    s.day = 2;
    game::EndDay(s);
    morning = game::StartDay(s, content);
    CHECK(morning.size() == 1 && morning[0] == "g2_project_news");
    play(s, "g2_project_news");
    CHECK(talk(s, "parent") == "g2_choose_project");
    play(s, "g2_choose_project", 2);
    s.Set("money", 10);
    CHECK(talk(s, "parent") == "g2_saving_up");
    s.Set("money", 45);
    CHECK(talk(s, "parent") == "g2_donate");
    const int reputation = s.Get("reputation");
    play(s, "g2_donate", 0);
    CHECK(s.Get("money") == 5 && s.Get("reputation") == reputation + 15 && s.HasFlag("g2_donated"));

    // Festival Day and the end of the chapter.
    s.day = 13;
    game::EndDay(s);
    morning = game::StartDay(s, content);
    CHECK(!morning.empty() && morning[0] == "g2_festival_morning");
    CHECK(game::PickEveningEvent(s, content) == "g2_end_donated");
}

void TestChapterFiveWiring() {
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
    auto nextDay = [&](game::GameState& s) {
        game::EndDay(s);
        return game::StartDay(s, content);
    };

    CHECK(content.ChapterTitle(5) == "Rainy Days");
    CHECK(content.ChapterTitle(6) == "To Be Continued");

    // No rain before Chapter 5.
    game::GameState early;
    early.Set("chapter", 4);
    for (int day = 1; day <= 60; ++day) {
        early.day = day;
        game::StartDay(early, content);
        CHECK(!early.HasFlag("today.rain"));
    }

    game::GameState s;
    s.Set("generation", 2);
    s.Set("chapter", 5);
    s.SetFlag("pushcart_sale");
    s.party = {"nia"};
    s.day = 14;
    s.seenEvents = {"g2_project_news", "g2_festival_soon", "g2_festival_morning", "g2_end_donated"};
    s.SetFlag("g2_festival_done");

    // Day 15: rain, and a plan for bad days.
    std::vector<std::string> morning = nextDay(s);
    CHECK(morning.size() == 1 && morning[0] == "ch5_start");
    play(s, "ch5_start");
    CHECK(s.HasFlag("today.rain") && !s.HasFlag("today.hot"));
    const game::QuestDef* tracked = game::TrackedQuest(s, content);
    CHECK(tracked != nullptr && tracked->id == "rainy_day_fund");
    CHECK(talk(s, "parent") == "risk_talk");
    CHECK(talk(s, "leo2") == "eco_offer" || talk(s, "leo2") == "g2_leo_story");

    // Day 16: Baker Rosa's standing order.
    morning = nextDay(s);
    CHECK(morning.size() == 1 && morning[0] == "rosa_news");
    play(s, "rosa_news");
    CHECK(talk(s, "rosa") == "rosa_order_offer");
    play(s, "rosa_order_offer", 0);
    CHECK(s.HasFlag("contract_rosa") && talk(s, "rosa") != "rosa_order_offer");

    // Day 17: delivery day. Without stock you can't deliver; with it, coins in and stock out.
    morning = nextDay(s);
    CHECK(morning.size() == 2 && morning[0] == "rosa_delivery_day" && morning[1] == "storm_warning");
    play(s, "rosa_delivery_day");
    CHECK(talk(s, "rosa") == "rosa_delivery");
    const game::EventDef* delivery = content.FindEvent("rosa_delivery");
    CHECK(delivery != nullptr && delivery->choices.size() == 3);
    CHECK(!game::CheckCondition(delivery->choices[0].requirement, s));
    CHECK(game::DescribeUnmet(delivery->choices[0].requirement, s, content) == "Needs 10 Lemonade");
    CHECK(game::CheckCondition(delivery->choices[0].showWhen, s) &&
          !game::CheckCondition(delivery->choices[1].showWhen, s));
    s.AddStock({"lemonade", 10, 1, s.day, false});
    s.Set("money", 0);
    play(s, "rosa_delivery", 0);
    CHECK(s.Get("money") == 30 && s.StockOf("lemonade") == 0 && s.Get("contract.deliveries") == 1);
    CHECK(game::PickEveningEvent(s, content) != "rosa_missed");

    // Insurance before the storm.
    play(s, "risk_talk", 0);
    CHECK(s.HasFlag("insured") && s.Get("bank.premium") == 2 && s.Get("money") == 28);
    CHECK(talk(s, "parent") != "risk_talk");

    // Days 18-19, then Nia's secret in the evening of Day 19.
    nextDay(s);
    nextDay(s);
    CHECK(game::PickEveningEvent(s, content) == "nia_discount");
    play(s, "nia_discount", 0);
    CHECK(s.HasFlag("nia_upset") && talk(s, "nia") == "nia_makeup");
    play(s, "nia_makeup", 0);
    play(s, "nia_after", 0);
    CHECK(s.HasFlag("nia_resolved") && s.HasFlag("pay_what_you_can") && !s.HasFlag("nia_upset"));

    // Day 20: the storm, and a missed delivery.
    morning = nextDay(s);
    CHECK(morning.size() == 2 && morning[0] == "rosa_delivery_day" && morning[1] == "storm");
    play(s, "rosa_delivery_day");
    const game::EventDef* storm = content.FindEvent("storm");
    CHECK(storm != nullptr && game::CheckCondition(storm->choices[0].showWhen, s));
    play(s, "storm", 0);
    CHECK(s.HasFlag("storm_over") && s.HasFlag("cart_ok") && !s.HasFlag("cart_broken"));
    CHECK(game::PickEveningEvent(s, content) == "rosa_missed");
    play(s, "rosa_missed");
    CHECK(s.Get("contract.misses") == 1 && !s.HasFlag("today.delivery_due"));

    // Day 22 is bank day: the premium is paid. Day 23: a second miss cancels the order.
    nextDay(s);
    s.Set("money", 10);
    nextDay(s);
    CHECK(s.Get("money") == 8 && s.HasFlag("insured"));
    morning = nextDay(s);
    CHECK(!morning.empty() && morning[0] == "rosa_delivery_day");
    play(s, "rosa_delivery_day");
    CHECK(game::PickEveningEvent(s, content) == "rosa_cancel");
    play(s, "rosa_cancel");
    CHECK(!s.HasFlag("contract_rosa") && talk(s, "rosa") != "rosa_order_offer");

    // Day 28: the chapter ends, once.
    while (s.day < 28) {
        nextDay(s);
    }
    CHECK(game::PickEveningEvent(s, content) == "ch5_end_good");
    play(s, "ch5_end_good");
    s.Set("chapter", 6);
    CHECK(game::PickEveningEvent(s, content) == "dinner_g2" || game::PickEveningEvent(s, content) == "dinner_rain");

    // Without a plan or coins, the storm breaks the pushcart until you pay for a new wheel.
    game::GameState broke;
    broke.Set("generation", 2);
    broke.Set("chapter", 5);
    broke.SetFlag("ch5_started");
    play(broke, "storm", 3);
    CHECK(broke.HasFlag("cart_broken") && talk(broke, "parent") == "cart_repair");
    CHECK(talk(broke, "grandpa2") == "g2_grandpa_story");
    broke.seenEvents.insert("g2_grandpa_story");
    CHECK(talk(broke, "grandpa2") == "shop_chores");
    play(broke, "shop_chores", 0);
    CHECK(broke.Get("money") == 6 && talk(broke, "grandpa2") != "shop_chores");
    broke.Set("money", 30);
    play(broke, "cart_repair", 0);
    CHECK(!broke.HasFlag("cart_broken") && broke.HasFlag("cart_ok") && broke.Get("money") == 0);
}

void TestRainyDays() {
    // Rain never falls on a hot day, and roughly one in six days is rainy.
    game::GameState weather;
    weather.seed = 1234;
    int rainy = 0;
    for (int day = 1; day <= 300; ++day) {
        weather.day = day;
        CHECK(!(game::IsRainyDay(weather) && game::IsHotDay(weather)));
        rainy += game::IsRainyDay(weather) ? 1 : 0;
    }
    CHECK(rainy > 20 && rainy < 90);

    // Rain halves the customers at outdoor venues only.
    game::Content content;
    game::ItemDef lemonade;
    lemonade.id = "lemonade";
    lemonade.value = 3;
    content.items["lemonade"] = lemonade;
    content.itemOrder = {"lemonade"};
    game::VenueDef cart;
    cart.id = "cart";
    cart.name = "Cart";
    game::VenueDef shop = cart;
    shop.id = "shop";
    shop.outdoor = false;

    game::GameState dry;
    dry.Set("reputation", 50);
    dry.AddStock({"lemonade", 100, 1, 1, false});
    game::GameState wet = dry;
    wet.SetFlag("today.rain");
    const auto customers = [&](game::GameState state, const game::VenueDef& venue) {
        return static_cast<int>(game::RunSellSession(state, content, venue).sales.customers.size());
    };
    CHECK(customers(wet, cart) < customers(dry, cart));
    CHECK(customers(wet, shop) == customers(dry, shop));

    // Selling stock to a business: stock out, coins in, never more than you have.
    game::GameState order;
    order.AddStock({"lemonade", 7, 1, 1, false});
    game::ApplyEffects(Parse(R"({"sell_stock": {"item": "lemonade", "qty": 10, "price": 3}})"), order);
    CHECK(order.StockOf("lemonade") == 0 && order.Get("money") == 21);

    // Insurance premiums come out on bank day; if they can't be paid, the cover stops.
    game::EconomyDef economy;
    game::GameState insured;
    insured.SetFlag("insured");
    insured.Set("bank.premium", 2);
    insured.Set("money", 5);
    game::ProcessWeek(insured, economy);
    CHECK(insured.Get("money") == 3 && insured.HasFlag("insured"));
    insured.Set("money", 0);
    game::ProcessWeek(insured, economy);
    CHECK(!insured.HasFlag("insured"));
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
    leo.staff = {true, 0.7f};
    content.npcs.push_back(leo);
    game::NpcDef maya;
    maya.id = "maya";
    maya.name = "Maya";
    maya.staff = {true, 0.6f};
    content.npcs.push_back(maya);

    game::VenueDef stall;
    stall.id = "stall";
    stall.name = "Stall";
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
    CHECK(game::StaffCandidates(s, content).size() == 2);
    CHECK(game::StaffUnavailableReason(s, content, stall, "maya").empty());
    const game::SessionReport r = game::RunSellSession(s, content, stall);
    CHECK(r.fee == 5 && r.wage == 0 && r.staff.empty());
    CHECK(r.sales.tooExpensive == 0);
    CHECK(r.costOfSold == r.sales.TotalSold());
    CHECK(s.Get("money") == 20 + r.sales.revenue - 5);
    CHECK(r.Profit() == r.sales.revenue - r.costOfSold - 5);
    CHECK(s.minutes == 13 * 60);
    CHECK(s.HasFlag("first_sale") && s.HasFlag("stall_sale"));
    CHECK(r.bonusTexts.size() == 1);  // Leo, but not Kofi yet
    CHECK(r.leftover.at("lemonade") == 30 - r.sales.TotalSold());

    // Staff keep your clock still, are paid up front, and work once a day.
    s.AddStock({"lemonade", 30, 1, 1, false});
    s.Set("staff.wage", 3);
    s.SetFlag("today.festival");
    const int before = s.minutes;
    const int moneyBefore = s.Get("money");
    const game::SessionReport staffed = game::RunSellSession(s, content, stall, "maya");
    CHECK(staffed.staff == "maya" && staffed.wage == 3);
    CHECK(s.minutes == before);
    CHECK(s.Get("money") == moneyBefore + staffed.sales.revenue - 5 - 3);
    CHECK(s.HasFlag("today.staff_maya") && s.HasFlag("stall_festival_sale"));
    CHECK(!game::StaffUnavailableReason(s, content, stall, "maya").empty());  // already worked
    CHECK(!game::StaffUnavailableReason(s, content, stall, "leo").empty());   // stall already staffed
    s.SetFlag("leo_upset");
    game::VenueDef cart;
    cart.id = "cart";
    cart.name = "Cart";
    CHECK(!game::StaffUnavailableReason(s, content, cart, "leo").empty());  // upset
    s.ClearFlag("leo_upset");
    CHECK(game::StaffUnavailableReason(s, content, cart, "leo").empty());

    // Happier staff sell more: same seed, higher morale, more customers.
    game::GameState low = s;
    game::GameState high = s;
    low.Set("staff.morale_pct", 75);
    high.Set("staff.morale_pct", 115);
    const int lowCustomers = static_cast<int>(game::RunSellSession(low, content, cart, "leo").sales.customers.size());
    const int highCustomers = static_cast<int>(game::RunSellSession(high, content, cart, "leo").sales.customers.size());
    CHECK(highCustomers > lowCustomers);

    // Can't open without the fee or after 17:00.
    game::GameState poor = s;
    poor.Set("money", 2);
    CHECK(!game::CannotOpenReason(poor, content, stall).empty());
    game::GameState late = s;
    late.Set("money", 50);
    late.minutes = 17 * 60;
    CHECK(!game::CannotOpenReason(late, content, stall).empty());

    // Unlock and hide flags control what suppliers sell.
    game::ItemDef cookies;
    cookies.unlockFlag = "mom_recipe";
    cookies.hideFlag = "never";
    CHECK(!game::ItemAvailable(cookies, s));
    s.SetFlag("mom_recipe");
    CHECK(game::ItemAvailable(cookies, s));
    s.SetFlag("never");
    CHECK(!game::ItemAvailable(cookies, s));
}

void TestBank() {
    game::EconomyDef economy;  // 5% interest, late fee 3, 7-day weeks
    CHECK(!game::IsWeekStart(1, economy) && game::IsWeekStart(8, economy) && game::IsWeekStart(15, economy));
    CHECK(!game::IsWeekStart(9, economy));
    CHECK(game::NextWeekStart(9, economy) == 15 && game::NextWeekStart(8, economy) == 15);
    CHECK(game::SavingsInterest(100, 5) == 5);
    CHECK(game::SavingsInterest(12, 5) == 1);  // small savings still grow
    CHECK(game::SavingsInterest(5, 5) == 0);
    CHECK(game::LoanTotal(80, 10) == 88 && game::LoanTotal(80, 25) == 100);
    CHECK(game::WeeklyPayment(88, 4) == 22 && game::WeeklyPayment(10, 4) == 3);

    game::GameState s;
    s.Set("money", 50);
    CHECK(game::Deposit(s, 40) && s.Get("money") == 10 && s.Get("bank.savings") == 40);
    CHECK(!game::Deposit(s, 11));
    CHECK(!game::Withdraw(s, 41));
    CHECK(game::Withdraw(s, 10) && s.Get("bank.savings") == 30 && s.Get("money") == 20);

    game::TakeLoan(s, 80, 10, 4);
    CHECK(s.Get("money") == 100 && s.Get("bank.loan") == 88 && s.Get("bank.payment") == 22);

    // A week passes: interest on savings, then the payment.
    game::ProcessWeek(s, economy);
    CHECK(s.Get("bank.savings") == 31 && s.Get("bank.interest_earned") == 1);
    CHECK(s.Get("money") == 78 && s.Get("bank.loan") == 66);

    // Too little money: a late fee instead of a payment.
    s.Set("money", 5);
    game::ProcessWeek(s, economy);
    CHECK(s.Get("bank.loan") == 69 && s.HasFlag("loan_missed") && s.Get("money") == 5);

    // Paying it all back clears the weekly payment and leaves a memory.
    s.Set("money", 100);
    CHECK(game::RepayLoan(s, 500));
    CHECK(s.Get("bank.loan") == 0 && s.Get("bank.payment") == 0 && s.Get("money") == 31);
    CHECK(!s.memories.empty());

    // Through effects, as events use it.
    game::GameState e;
    game::ApplyEffects(Parse(R"({"take_loan": {"amount": 80, "rate": 25, "weeks": 4},
                                  "set": {"staff.wage": 3}})"), e);
    CHECK(e.Get("money") == 80 && e.Get("bank.loan") == 100 && e.Get("bank.payment") == 25);
    CHECK(e.Get("staff.wage") == 3);
}

void TestItemRules() {
    game::ItemDef lemonade;
    lemonade.value = 3;
    lemonade.shelfDays = 1;
    lemonade.fridgeShelfDays = 3;
    lemonade.valueWhen.push_back({Parse(R"({"flag": "price_war", "not_flag": "fridge"})"), 2, ""});
    game::GameState s;
    CHECK(game::ItemValue(lemonade, s) == 3 && game::ShelfDays(lemonade, s) == 1);
    s.SetFlag("price_war");
    CHECK(game::ItemValue(lemonade, s) == 2);
    s.SetFlag("fridge");  // cold drinks beat cheap warm ones
    CHECK(game::ItemValue(lemonade, s) == 3 && game::ShelfDays(lemonade, s) == 3);
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
    TestChapterTwoWiring();
    TestChapterThreeWiring();
    TestChapterFourWiring();
    TestChapterFiveWiring();
    TestSellingSession();
    TestBank();
    TestItemRules();
    TestChainTier();
    TestGenerations();
    TestRainyDays();
    TestEncounterRules();
    TestLifeWheelAndBedtime();

    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
