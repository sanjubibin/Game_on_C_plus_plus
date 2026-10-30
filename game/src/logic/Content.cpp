#include "logic/Content.h"

#include <json/reader.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>

namespace game {

namespace {

Json::Value ReadJson(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open " + path);
    }
    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    if (!Json::parseFromStream(builder, file, &root, &errors)) {
        throw std::runtime_error(path + ": " + errors);
    }
    return root;
}

// Runs a parser on one file, prefixing any error with the file name.
void LoadFile(const std::string& path, const std::function<void(const Json::Value&)>& parse) {
    const Json::Value root = ReadJson(path);
    try {
        parse(root);
    } catch (const std::exception& e) {
        throw std::runtime_error(path + ": " + e.what());
    }
}

const Json::Value& Field(const Json::Value& v, const char* key) {
    static const Json::Value kNull;
    return v.isObject() ? v[key] : kNull;
}

std::string Str(const Json::Value& v, const char* key, const std::string& fallback = "") {
    const Json::Value& f = Field(v, key);
    return f.isString() ? f.asString() : fallback;
}

int Int(const Json::Value& v, const char* key, int fallback) {
    const Json::Value& f = Field(v, key);
    return f.isNumeric() ? f.asInt() : fallback;
}

float Float(const Json::Value& v, const char* key, float fallback) {
    const Json::Value& f = Field(v, key);
    return f.isNumeric() ? f.asFloat() : fallback;
}

bool Bool(const Json::Value& v, const char* key, bool fallback) {
    const Json::Value& f = Field(v, key);
    return f.isBool() ? f.asBool() : fallback;
}

std::vector<std::string> Strings(const Json::Value& v, const char* key) {
    std::vector<std::string> out;
    const Json::Value& f = Field(v, key);
    if (f.isString()) {
        out.push_back(f.asString());
    } else if (f.isArray()) {
        for (const Json::Value& item : f) {
            out.push_back(item.asString());
        }
    }
    return out;
}

Rgb ParseRgb(const std::string& hex, Rgb fallback) {
    if (hex.size() != 7 || hex[0] != '#') {
        return fallback;
    }
    try {
        const unsigned long value = std::stoul(hex.substr(1), nullptr, 16);
        return Rgb{static_cast<unsigned char>((value >> 16) & 0xFF),
                   static_cast<unsigned char>((value >> 8) & 0xFF),
                   static_cast<unsigned char>(value & 0xFF)};
    } catch (const std::exception&) {
        return fallback;
    }
}

Later ParseLater(const Json::Value& v) {
    Later later;
    if (v.isString()) {
        later.event = v.asString();
    } else if (v.isObject()) {
        later.event = Str(v, "event");
        later.days = Int(v, "days", 1);
    }
    return later;
}

std::vector<Line> ParseLines(const Json::Value& lines, const std::string& defaultSpeaker) {
    std::vector<Line> out;
    for (const Json::Value& line : lines) {
        if (line.isString()) {
            out.push_back({defaultSpeaker, line.asString()});
        } else {
            out.push_back({Str(line, "speaker", defaultSpeaker), Str(line, "text")});
        }
    }
    return out;
}

ChoiceDef ParseChoice(const Json::Value& v) {
    ChoiceDef choice;
    choice.label = Str(v, "label");
    choice.showWhen = Field(v, "show_when");
    choice.requirement = Field(v, "requires");
    choice.effects = Field(v, "effects");
    choice.then = Str(v, "then");
    choice.action = Str(v, "action");
    choice.later = ParseLater(Field(v, "later"));
    return choice;
}

EventDef ParseEvent(const Json::Value& v) {
    EventDef event;
    event.id = Str(v, "id");
    event.speaker = Str(v, "speaker");
    event.style = Str(v, "style", "normal");
    event.lines = ParseLines(Field(v, "lines"), event.speaker);
    event.effects = Field(v, "effects");
    for (const Json::Value& choice : Field(v, "choices")) {
        event.choices.push_back(ParseChoice(choice));
    }
    event.then = Str(v, "then");
    event.action = Str(v, "action");
    event.later = ParseLater(Field(v, "later"));
    if (event.id.empty()) {
        throw std::runtime_error("event without an id");
    }
    return event;
}

std::vector<TriggerRule> ParseTriggers(const Json::Value& rules) {
    std::vector<TriggerRule> out;
    for (const Json::Value& rule : rules) {
        out.push_back({Field(rule, "when"), Str(rule, "event"), Bool(rule, "repeat", false)});
    }
    return out;
}

void ParseItems(const Json::Value& root, Content& c) {
    for (const Json::Value& v : Field(root, "items")) {
        ItemDef item;
        item.id = Str(v, "id");
        item.name = Str(v, "name", item.id);
        item.note = Str(v, "note");
        item.cost = Int(v, "cost", 1);
        item.value = Int(v, "value", 2);
        item.shelfDays = Int(v, "shelf_days", 30);
        item.fridgeShelfDays = Int(v, "fridge_shelf_days", 0);
        for (const Json::Value& rule : Field(v, "value_when")) {
            item.valueWhen.push_back({Field(rule, "when"), Int(rule, "value", item.value), Str(rule, "reason")});
        }
        item.appeal = Float(v, "appeal", 1.0f);
        item.hotAppeal = Float(v, "hot_appeal", item.appeal);
        item.unlockFlag = Str(v, "unlock_flag");
        item.hideFlag = Str(v, "hide_flag");
        item.color = ParseRgb(Str(v, "color"), item.color);
        c.itemOrder.push_back(item.id);
        c.items[item.id] = item;
    }
}

void ParseEvents(const Json::Value& root, Content& c) {
    for (const Json::Value& v : Field(root, "events")) {
        EventDef event = ParseEvent(v);
        if (c.events.contains(event.id)) {
            throw std::runtime_error("duplicate event id '" + event.id + "'");
        }
        c.events[event.id] = std::move(event);
    }
    c.morning = ParseTriggers(Field(root, "morning"));
    c.evening = ParseTriggers(Field(root, "evening"));
}

void ParseNpcs(const Json::Value& root, Content& c) {
    for (const Json::Value& v : Field(root, "npcs")) {
        NpcDef npc;
        npc.id = Str(v, "id");
        npc.name = Str(v, "name", npc.id);
        npc.map = Str(v, "map", "town");
        npc.bonus.customers = Float(Field(v, "bonus"), "customers", 0.0f);
        npc.bonus.text = Str(Field(v, "bonus"), "text");
        const Json::Value& staff = Field(v, "staff");
        npc.staff.canWork = staff.isObject();
        npc.staff.efficiency = Float(staff, "efficiency", npc.staff.efficiency);
        npc.body = ParseRgb(Str(v, "body"), Rgb{90, 120, 200});
        npc.hair = ParseRgb(Str(v, "hair"), Rgb{60, 40, 30});
        npc.skin = ParseRgb(Str(v, "skin"), Rgb{240, 200, 160});
        npc.x = Float(v, "x", 0.0f);
        npc.y = Float(v, "y", 0.0f);
        npc.relationship = Bool(v, "relationship", false);
        npc.presentWhen = Field(v, "present_when");
        for (const Json::Value& rule : Field(v, "talk")) {
            npc.talk.push_back({Field(rule, "when"), Str(rule, "event")});
        }
        // Idle one-liners become small generated events so the dialogue system handles them.
        const std::vector<std::string> idle = Strings(v, "idle");
        for (std::size_t i = 0; i < idle.size(); ++i) {
            EventDef event;
            event.id = "idle." + npc.id + "." + std::to_string(i);
            event.speaker = npc.id;
            event.lines.push_back({npc.id, idle[i]});
            npc.idleEvents.push_back(event.id);
            c.events[event.id] = std::move(event);
        }
        c.npcs.push_back(std::move(npc));
    }
}

void ParseQuests(const Json::Value& root, Content& c) {
    for (const Json::Value& v : Field(root, "quests")) {
        QuestDef quest;
        quest.id = Str(v, "id");
        quest.chapter = Int(v, "chapter", 1);
        quest.title = Str(v, "title");
        quest.summary = Str(v, "summary");
        quest.startFlag = Str(v, "start_flag");
        for (const Json::Value& step : Field(v, "steps")) {
            quest.steps.push_back({Str(step, "text"), Str(step, "done_flag")});
        }
        c.quests.push_back(std::move(quest));
    }
}

void ParseEncounters(const Json::Value& root, Content& c) {
    for (const Json::Value& v : Field(root, "encounters")) {
        EncounterDef enc;
        enc.id = Str(v, "id");
        enc.title = Str(v, "title");
        enc.opponent = Str(v, "opponent");
        enc.intro = Str(v, "intro");
        enc.need = Str(v, "need");
        enc.moodLabel = Str(v, "mood_label", enc.moodLabel);
        enc.winText = Str(v, "win_text", enc.winText);
        enc.loseText = Str(v, "lose_text", enc.loseText);
        enc.startMood = Int(v, "start_mood", 20);
        enc.goal = Int(v, "goal", 75);
        enc.maxTurns = Int(v, "max_turns", 6);
        enc.winEvent = Str(v, "win");
        enc.loseEvent = Str(v, "lose");
        for (const Json::Value& a : Field(v, "actions")) {
            EncounterAction action;
            action.id = Str(a, "id");
            action.label = Str(a, "label");
            action.skill = Str(a, "skill");
            action.party = Str(a, "party");
            action.text = Str(a, "text");
            action.textUnrevealed = Str(a, "text_unrevealed", action.text);
            action.requirement = Field(a, "requires");
            action.effects = Field(a, "effects");
            action.base = Int(a, "base", 0);
            action.perSkill = Int(a, "per_skill", 0);
            action.baseUnrevealed = Int(a, "base_unrevealed", action.base);
            action.reveals = Bool(a, "reveals", false);
            action.needsReveal = Bool(a, "needs_reveal", false);
            action.leaves = Bool(a, "leaves", false);
            enc.actions.push_back(std::move(action));
        }
        c.encounters[enc.id] = std::move(enc);
    }
}

void ParseVenues(const Json::Value& root, Content& c) {
    for (const Json::Value& v : Field(root, "venues")) {
        VenueDef venue;
        venue.id = Str(v, "id");
        venue.name = Str(v, "name", venue.id);
        venue.backdrop = Str(v, "backdrop", "park");
        venue.staffable = Bool(v, "staffable", true);
        venue.customers = Float(v, "customers", 1.0f);
        venue.festivalCustomers = Float(v, "festival_customers", 1.0f);
        venue.fee = Int(v, "fee", 0);
        venue.hours = Int(v, "hours", 3);
        for (const Json::Value& b : Field(v, "bonuses")) {
            venue.bonuses.push_back({Field(b, "when"), Float(b, "customers", 0.0f), Str(b, "text")});
        }
        c.venues[venue.id] = std::move(venue);
    }
}

void ParseSuppliers(const Json::Value& root, Content& c) {
    for (const Json::Value& v : Field(root, "suppliers")) {
        SupplierDef supplier;
        supplier.id = Str(v, "id");
        supplier.name = Str(v, "name", supplier.id);
        supplier.npc = Str(v, "npc");
        supplier.intro = Str(v, "intro");
        for (const Json::Value& item : Field(v, "items")) {
            supplier.items.push_back(
                {Str(item, "item"), Int(item, "cost", 1), Int(item, "bulk_qty", 0), Int(item, "bulk_cost", 0)});
        }
        c.suppliers[supplier.id] = std::move(supplier);
    }
}

void ParseStart(const Json::Value& root, Content& c) {
    c.start.map = Str(root, "map", "town");
    c.start.chapters = Strings(root, "chapters");
    const Json::Value& economy = Field(root, "economy");
    c.economy.savingsInterestPercent = Int(economy, "savings_interest", c.economy.savingsInterestPercent);
    c.economy.lateFee = Int(economy, "late_fee", c.economy.lateFee);
    c.economy.weekLength = std::max(1, Int(economy, "week_length", c.economy.weekLength));
    c.start.stats = Field(root, "stats");
    c.start.flags = Strings(root, "flags");
    c.start.party = Strings(root, "party");
    c.start.events = Strings(root, "events");
    c.start.tips = Strings(root, "tips");
}

void ParseMap(const Json::Value& root, MapDef& map) {
    map.name = Str(root, "name", map.id);
    map.enterFlag = Str(root, "enter_flag");
    map.enterEvent = Str(root, "enter_event");
    map.decor = Str(root, "decor");
    map.rows = Strings(root, "tiles");
    map.height = static_cast<int>(map.rows.size());
    map.width = map.rows.empty() ? 0 : static_cast<int>(map.rows[0].size());
    for (std::size_t y = 0; y < map.rows.size(); ++y) {
        if (static_cast<int>(map.rows[y].size()) != map.width) {
            throw std::runtime_error("tile row " + std::to_string(y) + " has width " +
                                     std::to_string(map.rows[y].size()) + ", expected " +
                                     std::to_string(map.width));
        }
    }
    for (const Json::Value& v : Field(root, "buildings")) {
        Building b;
        b.id = Str(v, "id");
        b.label = Str(v, "label");
        b.action = Str(v, "action");
        b.style = Str(v, "style", "house");
        b.x = Int(v, "x", 0);
        b.y = Int(v, "y", 0);
        b.w = Int(v, "w", 1);
        b.h = Int(v, "h", 1);
        b.doorX = Int(v, "door_x", -1);
        b.wall = ParseRgb(Str(v, "wall"), Rgb{230, 220, 200});
        b.roof = ParseRgb(Str(v, "roof"), Rgb{180, 80, 70});
        map.buildings.push_back(std::move(b));
    }
    for (const Json::Value& v : Field(root, "spots")) {
        Spot s;
        s.id = Str(v, "id");
        s.label = Str(v, "label");
        s.action = Str(v, "action");
        s.draw = Str(v, "draw");
        s.x = Int(v, "x", 0);
        s.y = Int(v, "y", 0);
        s.when = Field(v, "when");
        map.spots.push_back(std::move(s));
    }
    for (const Json::Value& v : Field(root, "exits")) {
        Exit e;
        e.x = Int(v, "x", 0);
        e.y = Int(v, "y", 0);
        e.w = Int(v, "w", 1);
        e.h = Int(v, "h", 1);
        e.to = Str(v, "to");
        const Json::Value& at = Field(v, "at");
        if (at.isArray() && at.size() == 2) {
            e.toX = at[0].asInt();
            e.toY = at[1].asInt();
        }
        map.exits.push_back(std::move(e));
    }
    const Json::Value& start = Field(root, "start");
    if (start.isArray() && start.size() == 2) {
        map.startX = start[0].asInt();
        map.startY = start[1].asInt();
    }
}

}  // namespace

char MapDef::TileAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return 'X';
    }
    return rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

bool MapDef::IsSolid(char tile) {
    return tile == 'T' || tile == '~' || tile == '#' || tile == 'b' || tile == 'o' || tile == 'k' || tile == 'X';
}

Content Content::Load(const std::string& assetsDir) {
    Content c;
    const std::string data = assetsDir + "/data/";
    LoadFile(data + "items.json", [&](const Json::Value& root) { ParseItems(root, c); });
    LoadFile(data + "events.json", [&](const Json::Value& root) { ParseEvents(root, c); });
    LoadFile(data + "npcs.json", [&](const Json::Value& root) { ParseNpcs(root, c); });
    LoadFile(data + "quests.json", [&](const Json::Value& root) { ParseQuests(root, c); });
    LoadFile(data + "encounters.json", [&](const Json::Value& root) { ParseEncounters(root, c); });
    LoadFile(data + "venues.json", [&](const Json::Value& root) { ParseVenues(root, c); });
    LoadFile(data + "suppliers.json", [&](const Json::Value& root) { ParseSuppliers(root, c); });
    LoadFile(data + "start.json", [&](const Json::Value& root) { ParseStart(root, c); });

    // Every .json file in assets/maps is one area; its file name is its id.
    std::vector<std::filesystem::path> mapFiles;
    for (const auto& entry : std::filesystem::directory_iterator(assetsDir + "/maps")) {
        if (entry.path().extension() == ".json") {
            mapFiles.push_back(entry.path());
        }
    }
    std::sort(mapFiles.begin(), mapFiles.end());
    for (const std::filesystem::path& file : mapFiles) {
        MapDef map;
        map.id = file.stem().string();
        LoadFile(file.string(), [&](const Json::Value& root) { ParseMap(root, map); });
        c.maps[map.id] = std::move(map);
    }
    if (!c.maps.contains(c.start.map)) {
        throw std::runtime_error("start map '" + c.start.map + "' not found in " + assetsDir + "/maps");
    }
    return c;
}

const ItemDef* Content::FindItem(const std::string& id) const {
    const auto it = items.find(id);
    return it == items.end() ? nullptr : &it->second;
}

const EventDef* Content::FindEvent(const std::string& id) const {
    const auto it = events.find(id);
    return it == events.end() ? nullptr : &it->second;
}

const NpcDef* Content::FindNpc(const std::string& id) const {
    for (const NpcDef& npc : npcs) {
        if (npc.id == id) {
            return &npc;
        }
    }
    return nullptr;
}

const EncounterDef* Content::FindEncounter(const std::string& id) const {
    const auto it = encounters.find(id);
    return it == encounters.end() ? nullptr : &it->second;
}

const MapDef* Content::FindMap(const std::string& id) const {
    const auto it = maps.find(id);
    return it == maps.end() ? nullptr : &it->second;
}

const VenueDef* Content::FindVenue(const std::string& id) const {
    const auto it = venues.find(id);
    return it == venues.end() ? nullptr : &it->second;
}

const SupplierDef* Content::FindSupplier(const std::string& id) const {
    const auto it = suppliers.find(id);
    return it == suppliers.end() ? nullptr : &it->second;
}

const MapDef& Content::StartMap() const {
    return maps.at(start.map);
}

std::string Content::ChapterTitle(int chapter) const {
    if (chapter >= 1 && static_cast<std::size_t>(chapter) <= start.chapters.size()) {
        return start.chapters[static_cast<std::size_t>(chapter - 1)];
    }
    return "Chapter " + std::to_string(chapter);
}

std::string Content::NameOf(const std::string& characterId) const {
    if (characterId == "you") {
        return "You";
    }
    const NpcDef* npc = FindNpc(characterId);
    return npc ? npc->name : characterId;
}

}  // namespace game
