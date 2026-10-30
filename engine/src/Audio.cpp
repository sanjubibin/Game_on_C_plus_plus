#include "engine/Audio.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <map>

namespace engine::audio {

namespace {

constexpr unsigned int kSampleRate = 44100;
constexpr float kAttack = 0.004f;   // seconds; short fades stop clicks between notes
constexpr float kRelease = 0.03f;

bool g_ready = false;
bool g_soundOn = true;
bool g_musicOn = true;
std::map<std::string, Sound> g_sounds;
Sound g_music{};
bool g_hasMusic = false;

float Oscillate(Waveform waveform, float phase) {
    switch (waveform) {
        case Waveform::Sine:
            return std::sin(2.0f * PI * phase);
        case Waveform::Square:
            return phase < 0.5f ? 0.6f : -0.6f;  // squares are loud; keep them gentle
        case Waveform::Triangle:
            return 4.0f * std::fabs(phase - 0.5f) - 1.0f;
    }
    return 0.0f;
}

// Adds the notes into `mix`, growing it as needed.
void Render(const std::vector<Note>& notes, Waveform waveform, float volume, std::vector<float>& mix) {
    std::size_t pos = 0;
    for (const Note& note : notes) {
        const auto count = static_cast<std::size_t>(note.seconds * static_cast<float>(kSampleRate));
        if (mix.size() < pos + count) {
            mix.resize(pos + count, 0.0f);
        }
        if (note.frequency > 0.0f) {
            const float attack = kAttack * static_cast<float>(kSampleRate);
            const float release = kRelease * static_cast<float>(kSampleRate);
            for (std::size_t i = 0; i < count; ++i) {
                const float t = static_cast<float>(i);
                const float phase = std::fmod(note.frequency * t / static_cast<float>(kSampleRate), 1.0f);
                const float envelope =
                    std::min(1.0f, t / attack) * std::min(1.0f, static_cast<float>(count - i) / release);
                mix[pos + i] += Oscillate(waveform, phase) * envelope * volume;
            }
        }
        pos += count;
    }
}

Sound ToSound(const std::vector<float>& mix) {
    std::vector<short> pcm(mix.size());
    for (std::size_t i = 0; i < mix.size(); ++i) {
        pcm[i] = static_cast<short>(std::clamp(mix[i], -1.0f, 1.0f) * 32767.0f);
    }
    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(pcm.size());
    wave.sampleRate = kSampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = pcm.data();
    return LoadSoundFromWave(wave);  // copies the samples
}

}  // namespace

bool Init() {
    if (!g_ready) {
        InitAudioDevice();
        g_ready = IsAudioDeviceReady();
    }
    return g_ready;
}

void Shutdown() {
    if (!g_ready) {
        return;
    }
    for (auto& [name, sound] : g_sounds) {
        UnloadSound(sound);
    }
    g_sounds.clear();
    if (g_hasMusic) {
        UnloadSound(g_music);
        g_hasMusic = false;
    }
    CloseAudioDevice();
    g_ready = false;
}

bool Ready() {
    return g_ready;
}

void AddSound(const std::string& name, const std::vector<Note>& notes, Waveform waveform, float volume) {
    if (!g_ready) {
        return;
    }
    std::vector<float> mix;
    Render(notes, waveform, volume, mix);
    if (const auto it = g_sounds.find(name); it != g_sounds.end()) {
        UnloadSound(it->second);
    }
    g_sounds[name] = ToSound(mix);
}

void Play(const std::string& name) {
    if (!g_ready || !g_soundOn) {
        return;
    }
    if (const auto it = g_sounds.find(name); it != g_sounds.end()) {
        PlaySound(it->second);
    }
}

void SetMusic(const std::vector<Voice>& voices, float masterVolume) {
    if (!g_ready) {
        return;
    }
    std::vector<float> mix;
    for (const Voice& voice : voices) {
        Render(voice.notes, voice.waveform, voice.volume, mix);
    }
    if (g_hasMusic) {
        UnloadSound(g_music);
    }
    g_music = ToSound(mix);
    SetSoundVolume(g_music, masterVolume);
    g_hasMusic = true;
}

void Update() {
    if (!g_ready || !g_hasMusic) {
        return;
    }
    const bool playing = IsSoundPlaying(g_music);
    if (g_musicOn && !playing) {
        PlaySound(g_music);
    } else if (!g_musicOn && playing) {
        StopSound(g_music);
    }
}

void SetSoundEnabled(bool enabled) {
    g_soundOn = enabled;
}

void SetMusicEnabled(bool enabled) {
    g_musicOn = enabled;
}

bool SoundEnabled() {
    return g_soundOn;
}

bool MusicEnabled() {
    return g_musicOn;
}

}  // namespace engine::audio
