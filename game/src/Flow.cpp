#include "Flow.h"

#include <raylib.h>

#include <memory>
#include <random>

#include "Visuals.h"
#include "engine/Audio.h"
#include "logic/Legacy.h"
#include "logic/Rules.h"
#include "logic/SaveGame.h"
#include "logic/Selling.h"
#include "scenes/BankScene.h"
#include "scenes/ChapterEndScene.h"
#include "scenes/DaySummaryScene.h"
#include "scenes/DialogueScene.h"
#include "scenes/EncounterScene.h"
#include "scenes/LegacyScene.h"
#include "scenes/SellScene.h"
#include "scenes/SupplierScene.h"
#include "scenes/WorldScene.h"

namespace flow {

void InitNewState(GameSession& session) {
    session.state = game::GameState{};
    std::random_device random;
    session.state.seed = random();

    const game::StartDef& start = session.content.start;
    if (start.stats.isObject()) {
        for (const std::string& stat : start.stats.getMemberNames()) {
            session.state.Set(stat, start.stats[stat].asInt());
        }
    }
    for (const std::string& flag : start.flags) {
        session.state.SetFlag(flag);
    }
    for (const std::string& member : start.party) {
        session.state.JoinParty(member);
    }
    session.toasts.clear();
    SyncNames(session);
}

void SyncNames(GameSession& session) {
    if (session.baseNames.empty()) {
        for (const game::NpcDef& npc : session.content.npcs) {
            session.baseNames[npc.id] = npc.name;
        }
    }
    for (game::NpcDef& npc : session.content.npcs) {
        const auto it = session.state.names.find(npc.id);
        npc.name = it != session.state.names.end() ? it->second : session.baseNames[npc.id];
    }
}

void NewGeneration(engine::Application& app, GameSession& session) {
    game::LegacyReport report = game::StartNextGeneration(session.state, session.content);
    SyncNames(session);
    std::vector<std::string> events = {"g2_intro"};
    const std::vector<std::string> morning = game::StartDay(session.state, session.content);
    events.insert(events.end(), morning.begin(), morning.end());
    PlaceAtHome(session);
    app.Scenes().Push(std::make_unique<LegacyScene>(app, session, std::move(report), [&app, &session, events]() {
        PlayEvents(app, session, events);
    }));
}

void PlaceAtHome(GameSession& session) {
    session.currentMap = session.content.start.map;
    const game::MapDef& map = session.content.StartMap();
    session.playerPos = {(static_cast<float>(map.startX) + 0.5f) * visuals::kTile,
                         (static_cast<float>(map.startY) + 0.5f) * visuals::kTile + 8.0f};
    session.trail.clear();
}

void NewGame(engine::Application& app, GameSession& session) {
    InitNewState(session);
    std::vector<std::string> events = session.content.start.events;
    const std::vector<std::string> morning = game::StartDay(session.state, session.content);
    events.insert(events.end(), morning.begin(), morning.end());
    PlaceAtHome(session);
    app.Scenes().Replace(std::make_unique<WorldScene>(app, session));
    PlayEvents(app, session, events);
}

bool ContinueGame(engine::Application& app, GameSession& session, std::string* error) {
    if (!game::LoadGame(session.savePath, session.state, error)) {
        return false;
    }
    // Saves from before chapters were numbered: finishing chapter 1 means you're in chapter 2.
    game::GameState& st = session.state;
    if (st.Get("chapter") < 2 &&
        (st.seenEvents.contains("chapter1_end_great") || st.seenEvents.contains("chapter1_end_ok"))) {
        st.Set("chapter", 2);
    }
    session.toasts.clear();
    SyncNames(session);
    const std::vector<std::string> events = game::StartDay(session.state, session.content);
    PlaceAtHome(session);
    app.Scenes().Replace(std::make_unique<WorldScene>(app, session));
    PlayEvents(app, session, events);
    return true;
}

void PlayEvents(engine::Application& app, GameSession& session, std::vector<std::string> events,
                std::function<void()> onDone) {
    std::erase_if(events, [&](const std::string& id) {
        if (session.content.FindEvent(id) == nullptr) {
            TraceLog(LOG_WARNING, "Unknown event '%s'", id.c_str());
            return true;
        }
        return false;
    });
    if (events.empty()) {
        if (onDone) {
            onDone();
        }
        return;
    }
    app.Scenes().Push(
        std::make_unique<DialogueScene>(app, session, std::move(events), std::move(onDone)));
}

void RunAction(engine::Application& app, GameSession& session, const std::string& action) {
    if (action.empty()) {
        return;
    }
    if (action.starts_with("open_supplier:")) {
        if (const game::SupplierDef* supplier = session.content.FindSupplier(action.substr(14))) {
            app.Scenes().Push(std::make_unique<SupplierScene>(app, session, *supplier));
        }
    } else if (action == "open_bank") {
        app.Scenes().Push(std::make_unique<BankScene>(app, session));
    } else if (action.starts_with("open_sell:")) {
        if (const game::VenueDef* venue = session.content.FindVenue(action.substr(10))) {
            app.Scenes().Push(std::make_unique<SellScene>(app, session, *venue));
        }
    } else if (action == "new_generation") {
        NewGeneration(app, session);
    } else if (action == "chapter_end") {
        app.Scenes().Push(std::make_unique<ChapterEndScene>(app, session));
    } else if (action.starts_with("travel:")) {
        game::TravelTarget target;
        if (game::ParseTravel(action, target)) {
            TravelTo(app, session, target.map, target.x, target.y);
        }
    } else if (action == "end_day") {
        StartEvening(app, session);
    } else if (action == "home") {
        if (session.state.Hour() >= 17) {
            StartEvening(app, session);
        } else {
            PlayEvents(app, session, {"home_early"});
        }
    } else if (action.starts_with("event:")) {
        PlayEvents(app, session, {action.substr(6)});
    } else if (action.starts_with("encounter:")) {
        if (const game::EncounterDef* enc = session.content.FindEncounter(action.substr(10))) {
            app.Scenes().Push(std::make_unique<EncounterScene>(app, session, *enc));
        }
    } else {
        TraceLog(LOG_WARNING, "Unknown action '%s'", action.c_str());
    }
}

void TalkTo(engine::Application& app, GameSession& session, const game::NpcDef& npc) {
    const std::string event = game::PickTalkEvent(npc, session.state);
    if (!event.empty()) {
        PlayEvents(app, session, {event});
    }
}

void TravelTo(engine::Application& app, GameSession& session, const std::string& mapId, int x, int y) {
    const game::MapDef* map = session.content.FindMap(mapId);
    if (map == nullptr) {
        return;
    }
    const QuestSnapshot before = TakeQuestSnapshot(session);
    session.currentMap = mapId;
    session.playerPos = {(static_cast<float>(x) + 0.5f) * visuals::kTile,
                         (static_cast<float>(y) + 0.5f) * visuals::kTile + 8.0f};
    session.trail.clear();
    session.fadeStart = GetTime();
    session.AddToast(map->name);

    const bool firstVisit = !map->enterFlag.empty() && !session.state.HasFlag(map->enterFlag);
    if (firstVisit) {
        session.state.SetFlag(map->enterFlag);
    }
    ToastQuestChanges(session, before);
    if (firstVisit && !map->enterEvent.empty()) {
        PlayEvents(app, session, {map->enterEvent});
    }
}

void StartEvening(engine::Application& app, GameSession& session, bool late) {
    game::ApplyBedtime(session.state, late);
    game::RunBranchDay(session.state, session.content);  // the city shop's evening shift
    PlaceAtHome(session);
    session.state.minutes = std::max(session.state.minutes, 18 * 60);
    auto showSummary = [&app, &session]() {
        app.Scenes().Push(std::make_unique<DaySummaryScene>(app, session));
    };
    const std::string event = game::PickEveningEvent(session.state, session.content);
    if (event.empty()) {
        showSummary();
    } else {
        PlayEvents(app, session, {event}, showSummary);
    }
}

void FinishDay(engine::Application& app, GameSession& session) {
    game::EndDay(session.state);
    if (!session.devMode) {
        std::string error;
        if (!game::SaveGame(session.savePath, session.state, &error)) {
            session.AddToast("Could not save: " + error);
        }
    }
    const std::vector<std::string> events = game::StartDay(session.state, session.content);
    PlaceAtHome(session);
    app.Scenes().Pop();  // the summary screen
    PlayEvents(app, session, events);
}

void ApplyEffects(GameSession& session, const Json::Value& effects) {
    const auto statsBefore = session.state.stats;
    const QuestSnapshot questsBefore = TakeQuestSnapshot(session);
    const int moneyBefore = session.state.Get("money");
    game::ApplyEffects(effects, session.state);
    if (effects.isObject() && effects.isMember("name")) {
        SyncNames(session);
    }
    if (session.state.Get("money") > moneyBefore) {
        engine::audio::Play("coin");
    }
    for (const std::string& change :
         game::DiffStats(statsBefore, session.state.stats, session.content)) {
        session.AddToast(change);
    }
    ToastQuestChanges(session, questsBefore);
}

QuestSnapshot TakeQuestSnapshot(const GameSession& session) {
    QuestSnapshot snapshot;
    for (const game::QuestDef& quest : session.content.quests) {
        if (game::GetQuestStatus(quest, session.state) == game::QuestStatus::Hidden) {
            snapshot.progress.push_back(-1);
            continue;
        }
        const game::QuestStep* step = game::CurrentStep(quest, session.state);
        snapshot.progress.push_back(step == nullptr
                                        ? static_cast<int>(quest.steps.size())
                                        : static_cast<int>(step - quest.steps.data()));
    }
    return snapshot;
}

void ToastQuestChanges(GameSession& session, const QuestSnapshot& before) {
    const QuestSnapshot after = TakeQuestSnapshot(session);
    for (std::size_t i = 0; i < after.progress.size() && i < before.progress.size(); ++i) {
        const game::QuestDef& quest = session.content.quests[i];
        const int total = static_cast<int>(quest.steps.size());
        const int was = before.progress[i];
        const int now = after.progress[i];
        if (now == was) {
            continue;
        }
        if (now == total) {
            session.AddToast("Quest complete: " + quest.title);
            engine::audio::Play("success");
        } else if (was < 0) {
            session.AddToast("New quest: " + quest.title);
        } else if (now > was) {
            session.AddToast("Quest updated: " + quest.title);
        }
    }
}

}  // namespace flow
