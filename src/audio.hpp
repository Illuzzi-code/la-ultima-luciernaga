// audio.hpp - música y efectos sintetizados (sin dependencias externas)
#pragma once
#include <SDL.h>
#include <atomic>
#include <vector>

enum class Sfx { Jump, Land, Collect, Blip, Checkpoint, Splash, Release, Ui, Step, Count };

class Audio {
public:
    bool init();
    void shutdown();
    void update(float dt, int fireflies, bool finalScene);
    void play(Sfx s);
    void setPaused(bool paused);
    bool enabled() const { return dev_ != 0; }

private:
    static void sdlCallback(void* user, Uint8* stream, int len);
    void mix(float* out, int frames);
    void generateMusic();
    void generateSfx();

    SDL_AudioDeviceID dev_ = 0;
    SDL_AudioSpec spec_{};
    SDL_mutex* lock_ = nullptr;

    std::vector<float> music_[3];
    std::vector<float> sfx_[(int)Sfx::Count];
    std::atomic<float> musicTarget_[3];
    std::atomic<float> musicGain_[3];

    struct Voice {
        const std::vector<float>* buf = nullptr;
        size_t pos = 0;
        float gain = 1.0f;
    };
    std::vector<Voice> voices_;

    size_t musicPos_ = 0;
    size_t musicLen_ = 0;
    bool paused_ = false;
};
