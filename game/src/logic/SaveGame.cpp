#include "logic/SaveGame.h"

#include <json/reader.h>
#include <json/writer.h>

#include <exception>
#include <filesystem>
#include <fstream>

namespace game {

namespace {

constexpr int kSaveVersion = 1;

Json::Value StringArray(const auto& strings) {
    Json::Value out(Json::arrayValue);
    for (const std::string& s : strings) {
        out.append(s);
    }
    return out;
}

void Fail(std::string* error, const std::string& message) {
    if (error) {
        *error = message;
    }
}

}  // namespace

bool SaveGame(const std::string& path, const GameState& state, std::string* error) {
    Json::Value root;
    root["version"] = kSaveVersion;
    root["day"] = state.day;
    root["minutes"] = state.minutes;
    root["seed"] = static_cast<Json::UInt>(state.seed);
    root["money_at_day_start"] = state.moneyAtDayStart;

    Json::Value stats(Json::objectValue);
    for (const auto& [key, value] : state.stats) {
        stats[key] = value;
    }
    root["stats"] = stats;

    Json::Value prices(Json::objectValue);
    for (const auto& [item, price] : state.prices) {
        prices[item] = price;
    }
    root["prices"] = prices;

    root["flags"] = StringArray(state.flags);
    root["seen_events"] = StringArray(state.seenEvents);
    root["party"] = StringArray(state.party);
    root["today_log"] = StringArray(state.todayLog);

    Json::Value stock(Json::arrayValue);
    for (const StockBatch& batch : state.stock) {
        Json::Value b;
        b["item"] = batch.item;
        b["qty"] = batch.qty;
        b["unit_cost"] = batch.unitCost;
        b["day_bought"] = batch.dayBought;
        b["dodgy"] = batch.dodgy;
        stock.append(b);
    }
    root["stock"] = stock;

    Json::Value scheduled(Json::arrayValue);
    for (const ScheduledEvent& e : state.scheduled) {
        Json::Value s;
        s["day"] = e.day;
        s["event"] = e.event;
        scheduled.append(s);
    }
    root["scheduled"] = scheduled;

    Json::Value memories(Json::arrayValue);
    for (const Memory& m : state.memories) {
        Json::Value entry;
        entry["day"] = m.day;
        entry["text"] = m.text;
        memories.append(entry);
    }
    root["memories"] = memories;

    try {
        const std::filesystem::path file(path);
        if (file.has_parent_path()) {
            std::filesystem::create_directories(file.parent_path());
        }
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        if (!out) {
            Fail(error, "Cannot write " + path);
            return false;
        }
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "  ";
        out << Json::writeString(writer, root);
        return static_cast<bool>(out);
    } catch (const std::exception& e) {
        Fail(error, e.what());
        return false;
    }
}

bool LoadGame(const std::string& path, GameState& state, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        Fail(error, "Cannot open " + path);
        return false;
    }
    Json::CharReaderBuilder reader;
    Json::Value root;
    std::string errors;
    if (!Json::parseFromStream(reader, in, &root, &errors)) {
        Fail(error, errors);
        return false;
    }
    if (root["version"].asInt() != kSaveVersion) {
        Fail(error, "Unsupported save version");
        return false;
    }

    try {
        GameState loaded;
        loaded.day = root["day"].asInt();
        loaded.minutes = root["minutes"].asInt();
        loaded.seed = root["seed"].asUInt();
        loaded.moneyAtDayStart = root["money_at_day_start"].asInt();
        for (const std::string& key : root["stats"].getMemberNames()) {
            loaded.stats[key] = root["stats"][key].asInt();
        }
        for (const std::string& item : root["prices"].getMemberNames()) {
            loaded.prices[item] = root["prices"][item].asInt();
        }
        for (const Json::Value& f : root["flags"]) {
            loaded.flags.insert(f.asString());
        }
        for (const Json::Value& e : root["seen_events"]) {
            loaded.seenEvents.insert(e.asString());
        }
        for (const Json::Value& p : root["party"]) {
            loaded.party.push_back(p.asString());
        }
        for (const Json::Value& line : root["today_log"]) {
            loaded.todayLog.push_back(line.asString());
        }
        for (const Json::Value& b : root["stock"]) {
            loaded.stock.push_back({b["item"].asString(), b["qty"].asInt(), b["unit_cost"].asInt(),
                                    b["day_bought"].asInt(), b["dodgy"].asBool()});
        }
        for (const Json::Value& s : root["scheduled"]) {
            loaded.scheduled.push_back({s["day"].asInt(), s["event"].asString()});
        }
        for (const Json::Value& m : root["memories"]) {
            loaded.memories.push_back({m["day"].asInt(), m["text"].asString()});
        }
        state = std::move(loaded);
        return true;
    } catch (const std::exception& e) {
        Fail(error, std::string("Damaged save: ") + e.what());
        return false;
    }
}

bool SaveExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

}  // namespace game
