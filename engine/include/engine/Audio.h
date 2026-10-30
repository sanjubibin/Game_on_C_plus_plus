#pragma once

#include <string>
#include <vector>

// Tiny synthesizer: sound effects and a looping music track are built from notes at start-up,
// so the game needs no audio files. Every call is a no-op when there is no audio device.
namespace engine::audio {

enum class Waveform { Sine, Square, Triangle };

struct Note {
    float frequency = 0.0f;  // Hz; 0 is a rest
    float seconds = 0.1f;
};

bool Init();
void Shutdown();
bool Ready();

// Builds a short sound from notes and registers it under a name.
void AddSound(const std::string& name, const std::vector<Note>& notes, Waveform waveform, float volume);
void Play(const std::string& name);

// Builds a looping background track by mixing several voices.
struct Voice {
    std::vector<Note> notes;
    Waveform waveform = Waveform::Triangle;
    float volume = 0.3f;
};
void SetMusic(const std::vector<Voice>& voices, float masterVolume);
void Update();  // keeps the music looping; called once per frame by Application

void SetSoundEnabled(bool enabled);
void SetMusicEnabled(bool enabled);
bool SoundEnabled();
bool MusicEnabled();

}  // namespace engine::audio
