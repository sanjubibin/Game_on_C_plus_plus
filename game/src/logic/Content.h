#pragma once

#include <json/value.h>

#include <map>
#include <string>
#include <vector>

namespace game {

// All authored game data, loaded from assets/data/*.json and assets/maps/*.json.
// Conditions ("when", "requires", "show_when") and effects stay as JSON and are interpreted
// by Rules.

struct Rgb {
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
};

// While the condition holds, customers think the item is worth a different amount
// (e.g. MegaMart's cheap lemonade during a price war).
struct ValueRule {
    Json::Value when;
    int value = 0;
    std::string reason;
};

struct ItemDef {
    std::string id;
    std::string name;
    std::string note;
    int cost = 1;        // reference price at Lin's Wholesale
    int value = 2;       // what customers usually think it is worth
    int shelfDays = 30;  // days before it goes off
    int fridgeShelfDays = 0;  // shelf life once the shop has a fridge (0 = no change)
    float appeal = 1.0f;
    float hotAppeal = 1.0f;  // appeal on hot days
    std::string unlockFlag;  // only sold once this flag is set
    std::string hideFlag;    // no longer sold once this flag is set
    std::vector<ValueRule> valueWhen;
    Rgb color;
};

struct SupplierItem {
    std::string item;
    int cost = 1;
};

// A place that sells stock: Lin's Wholesale, Green Hill Farm, ...
struct SupplierDef {
    std::string id;
    std::string name;
    std::string npc;  // shown on the supplier screen
    std::string intro;
    std::vector<SupplierItem> items;
};

struct Line {
    std::string speaker;  // character id, or empty for narration
    std::string text;
};

struct Later {
    std::string event;
    int days = 1;
};

struct ChoiceDef {
    std::string label;
    Json::Value showWhen;     // condition; the choice is hidden when not met
    Json::Value requirement;  // condition; the choice is shown disabled when not met
    Json::Value effects;
    std::string then;
    std::string action;
    Later later;
};

struct EventDef {
    std::string id;
    std::string speaker;
    std::string style = "normal";  // "normal", "narration" or "their_side"
    std::vector<Line> lines;
    Json::Value effects;  // applied when the event starts
    std::vector<ChoiceDef> choices;
    std::string then;
    std::string action;
    Later later;
};

struct TalkRule {
    Json::Value when;
    std::string event;
};

// What a companion adds when they are on your team.
struct PartyBonus {
    float customers = 0.0f;  // e.g. 0.15 = 15% more customers
    std::string text;
};

// A companion who can run a venue for you for a daily wage.
struct StaffProfile {
    bool canWork = false;
    float efficiency = 0.6f;  // share of your own sales they manage at fair pay
};

struct NpcDef {
    std::string id;
    std::string name;
    std::string map = "town";
    Rgb body;
    Rgb hair;
    Rgb skin;
    float x = 0.0f;  // tile coordinates
    float y = 0.0f;
    bool relationship = false;  // shown on the People page and counted in the Life Wheel
    Json::Value presentWhen;
    std::vector<TalkRule> talk;
    std::vector<std::string> idleEvents;  // generated from "idle" lines
    PartyBonus bonus;
    StaffProfile staff;
};

struct QuestStep {
    std::string text;
    std::string doneFlag;
};

struct QuestDef {
    std::string id;
    int chapter = 1;
    std::string title;
    std::string summary;
    std::string startFlag;
    std::vector<QuestStep> steps;
};

struct EncounterAction {
    std::string id;
    std::string label;
    std::string skill;  // skill that boosts the effect
    std::string party;  // companion required for this action
    std::string text;
    std::string textUnrevealed;
    Json::Value requirement;  // e.g. enough coins for a free drink
    Json::Value effects;      // applied when used, e.g. the cost of that drink
    int base = 0;
    int perSkill = 0;
    int baseUnrevealed = 0;
    bool reveals = false;
    bool needsReveal = false;
    bool leaves = false;
};

struct EncounterDef {
    std::string id;
    std::string title;
    std::string opponent;
    std::string intro;
    std::string need;
    std::string moodLabel = "Mood";
    std::string winText = "You did it!";
    std::string loseText = "Not this time...";
    int startMood = 20;
    int goal = 75;
    int maxTurns = 6;
    std::vector<EncounterAction> actions;
    std::string winEvent;
    std::string loseEvent;
};

struct Building {
    std::string id;
    std::string label;
    std::string action;
    std::string style = "house";  // "house" or "stall"
    int x = 0;
    int y = 0;
    int w = 1;
    int h = 1;
    int doorX = -1;  // -1: no door
    Rgb wall;
    Rgb roof;
};

struct Spot {
    std::string id;
    std::string label;
    std::string action;
    std::string draw;  // "cart", "stall", "sign" or empty
    int x = 0;
    int y = 0;
    Json::Value when;
};

// Walking into the rectangle moves the player to another map.
struct Exit {
    int x = 0;
    int y = 0;
    int w = 1;
    int h = 1;
    std::string to;
    int toX = 0;
    int toY = 0;
};

struct MapDef {
    std::string id;
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<std::string> rows;
    std::vector<Building> buildings;
    std::vector<Spot> spots;
    std::vector<Exit> exits;
    int startX = 0;
    int startY = 0;
    std::string enterFlag;   // set the first time the player enters
    std::string enterEvent;  // played the first time the player enters
    std::string decor;       // e.g. "bunting"

    char TileAt(int x, int y) const;  // 'X' outside the map
    // Trees, water, hedges, benches, fountains and the outside of the map block movement.
    static bool IsSolid(char tile);
};

struct VenueBonus {
    Json::Value when;
    float customers = 0.0f;
    std::string text;
};

// A place to sell: the pushcart, the market stall, ...
struct VenueDef {
    std::string id;
    std::string name;
    std::string backdrop;   // "park", "market" or "shop"
    bool staffable = true;  // staff can run it for you
    float customers = 1.0f;
    float festivalCustomers = 1.0f;
    int fee = 0;  // paid every time you open
    int hours = 3;
    std::vector<VenueBonus> bonuses;
};

struct TriggerRule {
    Json::Value when;
    std::string event;
    bool repeat = false;
};

struct StartDef {
    std::string map = "town";
    Json::Value stats;
    std::vector<std::string> flags;
    std::vector<std::string> party;
    std::vector<std::string> events;
    std::vector<std::string> tips;
    std::vector<std::string> chapters;  // chapter titles, chapter 1 first
};

// Money rules that are tuned, not authored per event.
struct EconomyDef {
    int savingsInterestPercent = 5;  // paid on savings every week
    int lateFee = 3;                 // added to a loan when a payment is missed
    int weekLength = 7;
};

struct Content {
    std::map<std::string, ItemDef> items;
    std::vector<std::string> itemOrder;
    std::map<std::string, EventDef> events;
    std::vector<NpcDef> npcs;
    std::vector<QuestDef> quests;
    std::map<std::string, EncounterDef> encounters;
    std::map<std::string, VenueDef> venues;
    std::map<std::string, SupplierDef> suppliers;
    std::vector<TriggerRule> morning;
    std::vector<TriggerRule> evening;
    std::map<std::string, MapDef> maps;
    StartDef start;
    EconomyDef economy;

    // Loads everything under assetsDir. Throws std::runtime_error naming the bad file.
    static Content Load(const std::string& assetsDir);

    const ItemDef* FindItem(const std::string& id) const;
    const EventDef* FindEvent(const std::string& id) const;
    const NpcDef* FindNpc(const std::string& id) const;
    const EncounterDef* FindEncounter(const std::string& id) const;
    const MapDef* FindMap(const std::string& id) const;
    const VenueDef* FindVenue(const std::string& id) const;
    const SupplierDef* FindSupplier(const std::string& id) const;
    const MapDef& StartMap() const;
    std::string NameOf(const std::string& characterId) const;
    std::string ChapterTitle(int chapter) const;
};

}  // namespace game
