// audio.cpp - sintetizador sencillo para ambiente y efectos
#include "audio.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

static constexpr double kPi = 3.14159265358979323846;
static float midiHz(float m) { return (float)(440.0 * std::pow(2.0, (m - 69.0) / 12.0)); }
static float softClip(float x) { return std::tanh(x * 1.35f); }

namespace {
struct Rng {
    uint32_t s = 0x12345678;
    float noise() {
        s = s * 1664525u + 1013904223u;
        return ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
    }
};
}

// Añade una nota con envolvente, permitiendo que el final se enrolle al principio.
static void addTone(std::vector<float>& buf, int sr, double start, double dur,
                    float f0, float f1, float amp, int timbre,
                    float attack = 0.01f, float release = 0.5f, bool decay = false) {
    if (buf.empty() || dur <= 0.0f) return;
    size_t N = buf.size();
    size_t s0 = (size_t)(start * sr);
    size_t n = (size_t)(dur * sr);
    double phase = 0.0;
    for (size_t i = 0; i < n; i++) {
        float t = (float)i / sr;
        float u = (float)i / (float)n;
        float f = f0 + (f1 - f0) * u;
        phase += 2.0 * kPi * f / sr;
        float env = 1.0f;
        if (attack > 0) env *= std::min(1.0f, t / attack);
        if (release > 0) env *= std::min(1.0f, (float)((dur - t) / release));
        if (decay) env *= std::exp(-t * 2.1f);
        float v = 0.0f;
        switch (timbre) {
            case 0: v = std::sin(phase); break;
            case 1: // campana
                v = std::sin(phase) + 0.44f * std::sin(phase * 2.0) +
                    0.20f * std::sin(phase * 3.01) + 0.09f * std::sin(phase * 4.03);
                break;
            case 2: // madera suave
                v = std::sin(phase) + 0.30f * std::sin(phase * 2.0) +
                    0.12f * std::sin(phase * 3.0);
                break;
            case 3: // aire
                v = std::sin(phase) * 0.7f + std::sin(phase * 0.5) * 0.3f;
                break;
            default: v = std::sin(phase); break;
        }
        buf[(s0 + i) % N] += v * env * amp;
    }
}

static void addNoise(std::vector<float>& buf, int sr, double start, double dur,
                     float amp, float cutoff, bool highpass = false) {
    if (buf.empty()) return;
    size_t N = buf.size();
    size_t s0 = (size_t)(start * sr);
    size_t n = (size_t)(dur * sr);
    Rng rng;
    rng.s = 0x9E3779B9u ^ (uint32_t)(start * 1000);
    float lp = 0;
    for (size_t i = 0; i < n; i++) {
        float t = (float)i / sr;
        float env = std::min(1.0f, t * 60.0f) * std::max(0.0f, 1.0f - t / (float)dur);
        env *= env;
        float nz = rng.noise();
        lp += cutoff * (nz - lp);
        float v = highpass ? (nz - lp) : lp;
        buf[(s0 + i) % N] += v * env * amp;
    }
}

bool Audio::init() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_Log("Audio no disponible: %s", SDL_GetError());
        return false;
    }
    SDL_AudioSpec want{};
    want.freq = 44100;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = sdlCallback;
    want.userdata = this;
    dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &spec_, 0);
    if (!dev_) {
        SDL_Log("No se pudo abrir audio: %s", SDL_GetError());
        return false;
    }
    SDL_PauseAudioDevice(dev_, 1);
    lock_ = SDL_CreateMutex();
    for (int i = 0; i < 3; i++) {
        musicGain_[i].store(0.0f);
        musicTarget_[i].store(0.0f);
    }
    voices_.reserve(16);
    generateMusic();
    generateSfx();
    SDL_PauseAudioDevice(dev_, 0);
    update(0.1f, 0, false);
    return true;
}

void Audio::shutdown() {
    if (dev_) {
        SDL_PauseAudioDevice(dev_, 1);
        SDL_CloseAudioDevice(dev_);
        dev_ = 0;
    }
    if (lock_) { SDL_DestroyMutex(lock_); lock_ = nullptr; }
    voices_.clear();
}

void Audio::sdlCallback(void* user, Uint8* stream, int len) {
    Audio* self = (Audio*)user;
    int frames = len / (int)(sizeof(float) * 2);
    float* out = (float*)stream;
    std::memset(out, 0, len);
    self->mix(out, frames);
}

void Audio::mix(float* out, int frames) {
    int n = (int)musicLen_;
    if (n <= 0) {
        // aún sin música: silencio
        return;
    }
    float g0 = musicGain_[0].load(std::memory_order_relaxed);
    float g1 = musicGain_[1].load(std::memory_order_relaxed);
    float g2 = musicGain_[2].load(std::memory_order_relaxed);
    for (int i = 0; i < frames; i++) {
        float music = music_[0][musicPos_] * g0 +
                      music_[1][musicPos_] * g1 +
                      music_[2][musicPos_] * g2;
        music *= 0.62f;
        float sfx = 0.0f;
        for (auto& v : voices_) {
            if (!v.buf || v.pos >= v.buf->size()) continue;
            sfx += (*v.buf)[v.pos] * v.gain;
            v.pos++;
        }
        // limpiar voces terminadas de vez en cuando
        if ((musicPos_ & 255) == 0) {
            voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
                [](const Voice& v) { return !v.buf || v.pos >= v.buf->size(); }), voices_.end());
        }
        float s = softClip(music + sfx * 0.9f);
        out[i * 2 + 0] = s;
        out[i * 2 + 1] = s;
        musicPos_ = (musicPos_ + 1) % musicLen_;
    }
}

void Audio::update(float dt, int fireflies, bool finalScene) {
    float targets[3] = {0.48f, 0.0f, 0.0f};
    if (fireflies >= 1) targets[1] = 0.48f;
    if (fireflies >= 2) targets[2] = 0.36f;
    if (fireflies >= 3) { targets[1] = 0.55f; targets[2] = 0.46f; }
    if (finalScene) { targets[0] = 0.62f; targets[1] = 0.60f; targets[2] = 0.55f; }
    float k = std::min(1.0f, dt * 1.4f);
    for (int i = 0; i < 3; i++) {
        float g = musicGain_[i].load(std::memory_order_relaxed);
        g += (targets[i] - g) * k;
        musicGain_[i].store(g, std::memory_order_relaxed);
    }
}

void Audio::play(Sfx s) {
    if (!dev_ || paused_) return;
    int idx = (int)s;
    if (idx < 0 || idx >= (int)Sfx::Count || sfx_[idx].empty()) return;
    SDL_LockAudioDevice(dev_);
    if (voices_.size() < 12) {
        Voice v;
        v.buf = &sfx_[idx];
        v.pos = 0;
        v.gain = 1.0f;
        voices_.push_back(v);
    }
    SDL_UnlockAudioDevice(dev_);
}

void Audio::setPaused(bool paused) {
    if (!dev_) return;
    paused_ = paused;
    SDL_PauseAudioDevice(dev_, paused ? 1 : 0);
}

void Audio::generateMusic() {
    int sr = spec_.freq;
    double beat = 60.0 / 62.0;
    double bar = 4.0 * beat;
    double total = bar * 8.0;
    size_t N = (size_t)(total * sr);
    musicLen_ = N;
    musicPos_ = 0;
    for (int i = 0; i < 3; i++) music_[i].assign(N, 0.0f);

    struct Chord { int root; int notes[3]; };
    Chord prog[4] = {
        {45, {57, 60, 64}},  // Am
        {41, {53, 57, 60}},  // F
        {48, {55, 60, 64}},  // C
        {43, {55, 59, 62}}   // G
    };
    // Pad: un acorde cada dos compases
    for (int c = 0; c < 4; c++) {
        double t = c * bar * 2.0;
        double dur = bar * 2.0;
        float rootF = midiHz((float)(prog[c].root - 12));
        float rl = 0.062f;
        addTone(music_[0], sr, t, dur, rootF, rootF, rl, 3, 1.4f, 2.2f);
        addTone(music_[0], sr, t, dur, rootF * 1.003f, rootF * 1.003f, rl * 0.45f, 3, 1.8f, 2.2f);
        for (int k = 0; k < 3; k++) {
            float f = midiHz((float)prog[c].notes[k]);
            addTone(music_[0], sr, t, dur, f, f, 0.052f, 2, 1.7f, 2.4f);
            addTone(music_[0], sr, t, dur, f * 0.998f, f * 0.998f, 0.026f, 0, 2.0f, 2.4f);
        }
    }

    // Melodía de campanas, una nota por compás
    int melody[8] = {76, 72, 74, 69, 67, 76, 74, 72}; // E5 C5 D5 A4 G4 E5 D5 C5
    for (int b = 0; b < 8; b++) {
        double t = b * bar;
        float f = midiHz((float)melody[b]);
        addTone(music_[1], sr, t, 3.1, f, f, 0.095f, 1, 0.004f, 1.5f, true);
        addTone(music_[1], sr, t + 0.52, 2.6, f, f, 0.028f, 1, 0.004f, 1.3f, true);
        if (b % 2 == 1) {
            float h = midiHz((float)(melody[b] + 12));
            addTone(music_[1], sr, t + 0.26, 2.8, h, h, 0.022f, 0, 0.01f, 1.4f, true);
        }
    }

    // Capa alta: brillo lejano
    int spark[4] = {88, 84, 79, 86}; // E6 C6 G5 D6
    for (int k = 0; k < 4; k++) {
        double t = (k * 2 + 1) * bar;
        float f = midiHz((float)spark[k]);
        addTone(music_[2], sr, t, 4.5, f, f, 0.020f, 0, 0.6f, 2.0f, true);
        addTone(music_[2], sr, t + 0.18, 3.5, f * 1.5f, f * 1.5f, 0.010f, 1, 0.005f, 1.5f, true);
    }
    float air = midiHz(81);
    addTone(music_[2], sr, 0, total, air, air, 0.006f, 3, 3.0f, 3.0f);
}

void Audio::generateSfx() {
    int sr = spec_.freq;
    auto make = [&](Sfx id, double dur, auto fn) {
        auto& b = sfx_[(int)id];
        b.assign((size_t)(dur * sr), 0.0f);
        fn(b);
    };

    make(Sfx::Jump, 0.22, [&](std::vector<float>& b) {
        addTone(b, sr, 0.0, 0.16, 300, 640, 0.22f, 0, 0.005f, 0.12f, true);
        addTone(b, sr, 0.0, 0.10, 600, 900, 0.06f, 0, 0.002f, 0.08f, true);
        addNoise(b, sr, 0.0, 0.06, 0.012f, 0.35f, true);
    });
    make(Sfx::Land, 0.20, [&](std::vector<float>& b) {
        addTone(b, sr, 0.0, 0.15, 110, 62, 0.26f, 0, 0.003f, 0.12f, true);
        addNoise(b, sr, 0.0, 0.10, 0.045f, 0.10f);
    });
    make(Sfx::Collect, 1.8, [&](std::vector<float>& b) {
        int notes[4] = {84, 88, 91, 96};
        for (int i = 0; i < 4; i++) {
            float f = midiHz((float)notes[i]);
            addTone(b, sr, i * 0.085, 1.5, f, f, 0.14f, 1, 0.003f, 1.0f, true);
            addTone(b, sr, i * 0.085 + 0.25, 1.1, f, f, 0.035f, 0, 0.01f, 0.8f, true);
        }
        addNoise(b, sr, 0.0, 0.8, 0.010f, 0.5f, true);
    });
    make(Sfx::Blip, 0.05, [&](std::vector<float>& b) {
        addTone(b, sr, 0.0, 0.035, 660, 700, 0.055f, 0, 0.002f, 0.03f);
    });
    make(Sfx::Checkpoint, 2.4, [&](std::vector<float>& b) {
        int notes[4] = {57, 60, 64, 69};
        for (int i = 0; i < 4; i++) {
            float f = midiHz((float)notes[i]);
            addTone(b, sr, 0.0, 2.1, f, f, 0.045f, 2, 0.25f, 1.2f);
        }
        addTone(b, sr, 0.12, 2.0, midiHz(81), midiHz(81), 0.09f, 1, 0.01f, 1.5f, true);
        addTone(b, sr, 0.32, 1.6, midiHz(88), midiHz(88), 0.045f, 0, 0.01f, 1.2f, true);
    });
    make(Sfx::Splash, 0.6, [&](std::vector<float>& b) {
        addNoise(b, sr, 0.0, 0.45, 0.16f, 0.16f, false);
        addNoise(b, sr, 0.02, 0.35, 0.08f, 0.55f, true);
        addTone(b, sr, 0.0, 0.4, 260, 90, 0.12f, 0, 0.01f, 0.35f, true);
    });
    make(Sfx::Release, 5.0, [&](std::vector<float>& b) {
        int notes[7] = {45, 52, 57, 60, 64, 69, 81};
        for (int i = 0; i < 7; i++) {
            float f = midiHz((float)notes[i]);
            addTone(b, sr, 0.0, 4.8, f, f, 0.055f, 2, 0.5f, 3.0f);
            addTone(b, sr, 0.0, 4.8, f * 1.002f, f * 1.002f, 0.028f, 0, 0.8f, 3.0f);
        }
        for (int i = 0; i < 6; i++) {
            float f = midiHz((float)(84 + (i % 4) * 3));
            addTone(b, sr, i * 0.22, 3.0, f, f, 0.055f, 1, 0.004f, 2.0f, true);
        }
        addNoise(b, sr, 0.0, 3.5, 0.020f, 0.45f, true);
    });
    make(Sfx::Ui, 0.07, [&](std::vector<float>& b) {
        addTone(b, sr, 0.0, 0.05, 880, 880, 0.035f, 0, 0.003f, 0.04f);
    });
    make(Sfx::Step, 0.06, [&](std::vector<float>& b) {
        addNoise(b, sr, 0.0, 0.045, 0.015f, 0.3f, false);
    });
}
