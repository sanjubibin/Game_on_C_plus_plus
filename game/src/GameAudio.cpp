#include "GameAudio.h"

#include <json/reader.h>
#include <json/writer.h>
#include <raylib.h>

#include <fstream>
#include <utility>
#include <vector>

#include "engine/Audio.h"

namespace audio = engine::audio;

namespace {

// Note frequencies (Hz).
constexpr float C3 = 130.81f, D3 = 146.83f, E3 = 164.81f, F3 = 174.61f, G3 = 196.00f, A3 = 220.00f;
constexpr float C4 = 261.63f, Eb4 = 311.13f, F4 = 349.23f, G4 = 392.00f, A4 = 440.00f, B4 = 493.88f;
constexpr float C5 = 523.25f, D5 = 587.33f, E5 = 659.25f, G5 = 783.99f, B5 = 987.77f, C6 = 1046.50f;
constexpr float E6 = 1318.51f;
constexpr float kEighth = 0.25f;  // 120 beats per minute

// (frequency, length in eighths) -> notes
std::vector<audio::Note> Phrase(const std::vector<std::pair<float, int>>& parts) {
    std::vector<audio::Note> notes;
    for (const auto& [frequency, eighths] : parts) {
        notes.push_back({frequency, kEighth * static_cast<float>(eighths)});
    }
    return notes;
}

}  // namespace

void LoadGameAudio() {
    if (!audio::Ready()) {
        return;
    }
    audio::AddSound("click", {{1400.0f, 0.025f}}, audio::Waveform::Square, 0.25f);
    audio::AddSound("buy", {{660.0f, 0.04f}, {880.0f, 0.07f}}, audio::Waveform::Sine, 0.35f);
    audio::AddSound("coin", {{B5, 0.06f}, {E6, 0.16f}}, audio::Waveform::Square, 0.3f);
    audio::AddSound("success", {{C5, 0.09f}, {E5, 0.09f}, {G5, 0.09f}, {C6, 0.24f}}, audio::Waveform::Triangle, 0.45f);
    audio::AddSound("fail", {{G4, 0.14f}, {Eb4, 0.14f}, {C4, 0.32f}}, audio::Waveform::Triangle, 0.4f);

    // A gentle eight-bar loop in C major: C, Am, F, G, C, Am, F-G, C.
    const std::vector<audio::Note> melody = Phrase({
        {E5, 2}, {G4, 1}, {C5, 1}, {E5, 2}, {D5, 1}, {C5, 1},  //
        {A4, 2}, {C5, 1}, {E5, 1}, {D5, 2}, {C5, 2},           //
        {F4, 1}, {A4, 1}, {C5, 2}, {A4, 2}, {G4, 2},           //
        {G4, 1}, {B4, 1}, {D5, 2}, {B4, 2}, {0.0f, 2},         //
        {E5, 2}, {G5, 1}, {E5, 1}, {D5, 2}, {C5, 2},           //
        {C5, 1}, {D5, 1}, {E5, 2}, {A4, 2}, {C5, 2},           //
        {F4, 2}, {A4, 2}, {G4, 2}, {B4, 2},                    //
        {C5, 4}, {0.0f, 4},
    });
    const std::vector<audio::Note> bass = Phrase({
        {C3, 4}, {G3, 4}, {A3, 4}, {E3, 4}, {F3, 4}, {C3, 4}, {G3, 4}, {D3, 4},
        {C3, 4}, {G3, 4}, {A3, 4}, {E3, 4}, {F3, 4}, {G3, 4}, {C3, 4}, {C3, 4},
    });
    audio::SetMusic({{melody, audio::Waveform::Triangle, 0.22f}, {bass, audio::Waveform::Triangle, 0.3f}}, 0.35f);
}

Settings LoadSettings(const std::string& path) {
    Settings settings;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return settings;
    }
    Json::CharReaderBuilder reader;
    Json::Value root;
    std::string errors;
    if (Json::parseFromStream(reader, in, &root, &errors) && root.isObject()) {
        settings.music = root.get("music", true).asBool();
        settings.sound = root.get("sound", true).asBool();
        settings.fullscreen = root.get("fullscreen", false).asBool();
    }
    return settings;
}

void SaveSettings(const std::string& path, const Settings& settings) {
    Json::Value root;
    root["music"] = settings.music;
    root["sound"] = settings.sound;
    root["fullscreen"] = settings.fullscreen;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "  ";
    out << Json::writeString(writer, root);
}

void ApplySettings(const Settings& settings) {
    audio::SetMusicEnabled(settings.music);
    audio::SetSoundEnabled(settings.sound);
    if (settings.fullscreen != IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) {
        ToggleBorderlessWindowed();
    }
}
