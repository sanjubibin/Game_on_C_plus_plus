#include <raylib.h>

#include <cstdlib>
#include <exception>
#include <memory>
#include <string>

#include "Flow.h"
#include "GameAudio.h"
#include "GameSession.h"
#include "Visuals.h"
#include "engine/Application.h"
#include "engine/Audio.h"
#include "engine/Ui.h"
#include "logic/Legacy.h"
#include "logic/Rules.h"
#include "scenes/ChapterEndScene.h"
#include "scenes/DaySummaryScene.h"
#include "scenes/DialogueScene.h"
#include "scenes/JournalScene.h"
#include "scenes/LegacyScene.h"
#include "scenes/SellScene.h"
#include "scenes/TitleScene.h"
#include "scenes/WorldScene.h"

namespace ui = engine::ui;

namespace {

// Development options:
//   --start <continue|town|park|evening|market|festival|supplier|sell|sell-run|sell-results|
//            stall|stall-run|stall-results|encounter[:id]|journal[:tab]|summary|chapter-end|title|
//            ch2|shop|shop-results|bank|farm|farm-supplier|ch3|city|city-market|ch3-evening|
//            legacy|gen2|gen2-market|
//            dialogue:<event>|choices:<event>>
//   --screenshot <file.png> [--frames N]   save frame N and quit
struct Options {
    std::string start;
    std::string screenshot;
    int frames = 90;
};

Options ParseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string key = argv[i];
        const std::string value = argv[i + 1];
        if (key == "--start") {
            options.start = value;
        } else if (key == "--screenshot") {
            options.screenshot = value;
        } else if (key == "--frames") {
            options.frames = std::atoi(value.c_str());
        }
    }
    return options;
}

class ErrorScene : public engine::Scene {
public:
    ErrorScene(engine::Application& app, std::string message)
        : Scene(app), message_(std::move(message)) {}

    void Update(float /*dt*/) override {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
            App().Quit();
        }
    }

    void Draw(float /*alpha*/) const override {
        const float w = static_cast<float>(GetScreenWidth());
        const float h = static_cast<float>(GetScreenHeight());
        ClearBackground(Color{60, 24, 34, 255});
        ui::Text("Could not load the game data", {40.0f, 40.0f}, 36.0f, WHITE, true);
        ui::TextWrapped(message_, {40.0f, 110.0f, w - 80.0f, h - 200.0f}, 22.0f, Color{255, 220, 220, 255});
        ui::Text("Press Esc to quit", {40.0f, h - 60.0f}, 20.0f, Color{255, 200, 200, 255});
    }

private:
    std::string message_;
};

void DevStart(engine::Application& app, GameSession& session, const std::string& what) {
    session.devMode = true;
    if (what == "title") {
        app.Scenes().Push(std::make_unique<TitleScene>(app, session));
        return;
    }
    if (what == "continue") {
        app.Scenes().Push(std::make_unique<TitleScene>(app, session));
        std::string error;
        if (!flow::ContinueGame(app, session, &error)) {
            TraceLog(LOG_WARNING, "Continue failed: %s", error.c_str());
        }
        return;
    }

    flow::InitNewState(session);
    game::StartDay(session.state, session.content);
    session.state.seenEvents.insert("intro");
    session.state.SetFlag("intro_done");
    flow::PlaceAtHome(session);
    app.Scenes().Push(std::make_unique<WorldScene>(app, session));

    auto stock = [&session]() {
        game::GameState& st = session.state;
        st.SetFlag("got_pushcart");
        st.SetFlag("bought_stock");
        st.seenEvents.insert("grandpa_pushcart");
        st.AddStock({"lemonade", 12, 1, st.day, false});
        st.AddStock({"biscuits", 6, 2, st.day, false});
        st.Set("money", 24);
    };
    // A mid-chapter state: stall rented, friends on the team, a few memories.
    auto midChapter = [&session, &stock]() {
        game::GameState& st = session.state;
        stock();
        st.day = 10;
        for (const char* flag : {"first_sale", "market_news", "visited_market", "met_okafor", "stall_rented",
                                 "mom_recipe", "met_leo", "leo_joined", "priya_joined"}) {
            st.SetFlag(flag);
        }
        for (const char* event : {"okafor_intro", "kofi_hello", "leo_meet", "priya_hello", "mom_recipe", "market_arrival"}) {
            st.seenEvents.insert(event);
        }
        st.JoinParty("leo");
        st.JoinParty("priya");
        st.AddStock({"cookies", 8, 2, st.day, false});
        st.AddStock({"sandwiches", 6, 3, st.day, false});
        st.Set("money", 41);
        st.Set("skill.money_sense", 3);
        st.Set("skill.communication", 2);
        st.Set("skill.empathy", 2);
        st.Set("reputation", 48);
        st.memories = {{1, "Grandpa gave us his old pushcart."}, {1, "Made our very first sale!"},
                       {2, "Listened to Maya's side of the story."}, {3, "Made a new friend: Leo."},
                       {4, "Spotted a scam all by myself."}, {6, "Helped pay the family's rent."},
                       {8, "Rented a real market stall!"}};
    };
    auto toMarket = [&session]() {
        session.currentMap = "market";
        session.playerPos = {12.5f * visuals::kTile, 12.5f * visuals::kTile + 8.0f};
    };
    // Chapter 2 underway: partners in the shop, a loan, a fridge, fair pay, a farm visit.
    auto chapterTwo = [&session, &midChapter]() {
        game::GameState& st = session.state;
        midChapter();
        st.day = 20;
        st.Set("chapter", 2);
        for (const char* flag : {"shop_partner", "bank_news", "met_adeyemi", "has_loan", "loan_decided", "fridge",
                                 "upgrade_bought", "upgrade_sign", "farm_news", "visited_farm", "met_ama", "kofi_friend",
                                 "price_war", "price_war_over"}) {
            st.SetFlag(flag);
        }
        for (const char* event : {"ch2_start", "adeyemi_intro", "ama_intro", "staff_pay_talk", "farm_arrival"}) {
            st.seenEvents.insert(event);
        }
        st.Set("staff.wage", 3);
        st.Set("bank.savings", 35);
        st.Set("bank.interest_earned", 2);
        st.Set("bank.loan", 66);
        st.Set("bank.payment", 22);
        st.Set("money", 58);
        st.AddStock({"milkshakes", 10, 2, st.day, false});
        st.AddStock({"strawberries", 6, 2, st.day, false});
        st.memories.push_back({15, "Became Dad's partner in the corner shop."});
        st.memories.push_back({18, "Took our first loan - with a plan to pay it back."});
        st.memories.push_back({18, "Invested in a fridge for cold drinks."});
    };

    if (what == "town") {
        return;
    }
    // Chapter 3 underway: the city shop is open with Priya managing it.
    auto chapterThree = [&session, &chapterTwo]() {
        game::GameState& st = session.state;
        chapterTwo();
        st.day = 33;
        st.Set("chapter", 3);
        for (const char* flag : {"city_news", "visited_city", "met_osei", "branch_open", "manager_priya", "manager_chosen",
                                 "bulk_news", "met_haddad", "priya_joined"}) {
            st.SetFlag(flag);
        }
        for (const char* event : {"ch3_start", "osei_intro", "haddad_intro", "city_arrival", "ch3_bulk_tip"}) {
            st.seenEvents.insert(event);
        }
        st.Set("branch.rent", 15);
        st.Set("money", 96);
        st.AddStock({"pretzels", 24, 1, st.day, false});
        st.memories.push_back({30, "Opened our second shop, in Riverside City!"});
    };
    if (what == "ch2") {
        chapterTwo();
        return;
    }
    if (what == "ch3" || what == "city" || what == "city-market") {
        chapterThree();
        if (what != "ch3") {
            session.currentMap = "city";
            session.playerPos = {14.5f * visuals::kTile, 12.5f * visuals::kTile + 8.0f};
        }
        if (what == "city-market") {
            flow::RunAction(app, session, "open_supplier:city");
        }
        return;
    }
    // The second generation: chapter 3 finished, then twenty years later.
    auto secondGeneration = [&session, &chapterThree]() {
        chapterThree();
        game::GameState& st = session.state;
        st.day = 43;
        st.Set("chapter", 4);
        for (const char* flag : {"leo_joined", "kofi_friend", "maya_forgiven", "fair_done"}) {
            st.SetFlag(flag);
        }
        return game::StartNextGeneration(st, session.content);
    };
    if (what == "legacy") {
        game::LegacyReport report = secondGeneration();
        app.Scenes().Push(std::make_unique<LegacyScene>(app, session, std::move(report), std::function<void()>{}));
        return;
    }
    if (what == "gen2" || what == "gen2-market") {
        secondGeneration();
        game::GameState& st = session.state;
        st.names["parent"] = "Dad";
        flow::SyncNames(session);
        for (const char* flag : {"g2_started", "got_pushcart", "g2_project_news"}) {
            st.SetFlag(flag);
        }
        st.seenEvents.insert("g2_intro");
        st.day = 4;
        flow::PlaceAtHome(session);
        if (what == "gen2-market") {
            session.currentMap = "market";
            session.playerPos = {13.5f * visuals::kTile, 10.5f * visuals::kTile + 8.0f};
        }
        return;
    }
    if (what == "ch3-evening") {
        chapterThree();
        session.state.minutes = 18 * 60;
        flow::StartEvening(app, session, false);
        return;
    }
    if (what == "shop" || what == "shop-results") {
        chapterTwo();
        auto scene = std::make_unique<SellScene>(app, session, *session.content.FindVenue("shop"));
        if (what == "shop-results") {
            scene->DevAutoRun(1000.0f);
        }
        app.Scenes().Push(std::move(scene));
        return;
    }
    if (what == "bank") {
        chapterTwo();
        toMarket();
        flow::RunAction(app, session, "open_bank");
        return;
    }
    if (what == "farm" || what == "farm-supplier") {
        chapterTwo();
        session.currentMap = "farm";
        session.playerPos = {19.5f * visuals::kTile, 9.5f * visuals::kTile + 8.0f};
        if (what == "farm-supplier") {
            flow::RunAction(app, session, "open_supplier:farm");
        }
        return;
    }
    if (what == "exit-test") {
        // Standing on the town's east exit: the first update travels to the market.
        session.playerPos = {39.5f * visuals::kTile, 9.5f * visuals::kTile + 8.0f};
    } else if (what == "market" || what == "festival") {
        midChapter();
        toMarket();
        if (what == "festival") {
            session.state.day = 14;
            session.state.SetFlag("today.festival");
            session.state.SetFlag("stall_sale");
        }
    } else if (what == "stall" || what == "stall-run" || what == "stall-results") {
        midChapter();
        toMarket();
        session.state.SetFlag("kofi_friend");
        auto scene = std::make_unique<SellScene>(app, session, *session.content.FindVenue("stall"));
        if (what != "stall") {
            scene->DevAutoRun(what == "stall-run" ? 5.0f : 1000.0f);
        }
        app.Scenes().Push(std::move(scene));
    } else if (what == "chapter-end") {
        midChapter();
        session.state.memories.push_back({14, "Sold at the Market Stall on Festival Day!"});
        app.Scenes().Push(std::make_unique<ChapterEndScene>(app, session));
    } else if (what == "park") {
        stock();
        session.playerPos = {20.5f * visuals::kTile, 18.5f * visuals::kTile + 8.0f};
        session.state.minutes = 11 * 60;
    } else if (what == "evening") {
        stock();
        session.state.minutes = 19 * 60;
    } else if (what == "supplier") {
        flow::RunAction(app, session, "open_supplier:lin");
    } else if (what == "sell") {
        stock();
        app.Scenes().Push(std::make_unique<SellScene>(app, session, *session.content.FindVenue("pushcart")));
    } else if (what == "sell-run" || what == "sell-results") {
        stock();
        auto scene = std::make_unique<SellScene>(app, session, *session.content.FindVenue("pushcart"));
        scene->DevAutoRun(what == "sell-run" ? 5.0f : 1000.0f);
        app.Scenes().Push(std::move(scene));
    } else if (what.starts_with("encounter")) {
        midChapter();
        flow::RunAction(app, session, "encounter:" + (what.size() > 10 ? what.substr(10) : std::string("leo_alone")));
    } else if (what.starts_with("journal")) {
        midChapter();
        auto journal = std::make_unique<JournalScene>(app, session);
        if (what.size() > 8) {
            journal->SetTab(std::atoi(what.substr(8).c_str()));
        }
        app.Scenes().Push(std::move(journal));
    } else if (what == "summary") {
        stock();
        session.state.todayLog = {"Bought 12 Lemonade, 6 Biscuits for 24 coins.",
                                  "Pushcart: sold 14 items, profit 21 coins.", "You listened to Maya's side of the story."};
        session.state.Set("money", 45);
        app.Scenes().Push(std::make_unique<DaySummaryScene>(app, session));
    } else if (what.starts_with("dialogue:")) {
        flow::PlayEvents(app, session, {what.substr(9)});
    } else if (what.starts_with("choices:")) {
        auto dialogue = std::make_unique<DialogueScene>(app, session, std::vector<std::string>{what.substr(8)},
                                                        std::function<void()>{});
        dialogue->DevSkipLines();
        app.Scenes().Push(std::move(dialogue));
    } else {
        TraceLog(LOG_WARNING, "Unknown --start option '%s'", what.c_str());
    }
}

}  // namespace

int main(int argc, char** argv) {
    const Options options = ParseOptions(argc, argv);
    GameSession session;  // declared first so it outlives every scene

    engine::AppConfig config;
    config.title = "Corner Shop Quest";
    config.screenshotPath = options.screenshot;
    config.screenshotFrame = options.frames;
    engine::Application app(config);

    // Fonts are GPU textures: unload them before the window closes.
    struct FontGuard {
        ~FontGuard() { ui::UnloadFonts(); }
    } fontGuard;
    ui::LoadFonts({"assets/fonts/ui.ttf", "C:/Windows/Fonts/segoeui.ttf"},
                  {"assets/fonts/ui-bold.ttf", "C:/Windows/Fonts/segoeuib.ttf"});

    // Sounds are GPU-free but still need the device closed before the window goes.
    struct AudioGuard {
        ~AudioGuard() { engine::audio::Shutdown(); }
    } audioGuard;
    const bool screenshotRun = !options.screenshot.empty();
    if (!screenshotRun) {  // screenshot runs stay silent and windowed
        engine::audio::Init();
        LoadGameAudio();
        session.settings = LoadSettings(session.settingsPath);
        ApplySettings(session.settings);
    }

    try {
        session.content = game::Content::Load("assets");
    } catch (const std::exception& e) {
        app.Scenes().Push(std::make_unique<ErrorScene>(app, e.what()));
        app.Run();
        return 1;
    }
    for (const std::string& problem : game::ValidateContent(session.content)) {
        TraceLog(LOG_WARNING, "Content: %s", problem.c_str());
    }

    if (options.start.empty()) {
        app.Scenes().Push(std::make_unique<TitleScene>(app, session));
    } else {
        DevStart(app, session, options.start);
    }
    app.Run();
    return 0;
}
