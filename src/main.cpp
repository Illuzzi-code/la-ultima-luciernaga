// La última luciérnaga
// Un cuento corto en C++ / SDL2
#include <SDL.h>
#include "gfx.hpp"
#include "audio.hpp"
#include "level.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <random>
#include <string>
#include <vector>

#if defined(__ANDROID__)
#define LUCIERNAGA_ANDROID 1
#else
#define LUCIERNAGA_ANDROID 0
#endif

static constexpr float PI = 3.14159265358979323846f;

static float clampf(float v, float a, float b) { return std::max(a, std::min(b, v)); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float smoothstep(float a, float b, float x) {
    float t = clampf((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
static int utf8Count(const std::string& s) {
    int n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) n++;
    return n;
}

enum class GameState { Title, Intro, Play, Ending, Epilogue, Credits };

struct Line {
    std::string speaker;
    std::string text;
    int portrait = -2; // 9 Nara, 11 Teo, -1 luciérnaga, -2 narrador
    Color color;
};

static Line L(const std::string& who, const std::string& text) {
    if (who == "NARA") return {who, text, 9, Color(255, 205, 150)};
    if (who == "TEO") return {who, text, 11, Color(150, 205, 255)};
    if (who == "LUCIÉRNAGA") return {who, text, -1, Color(240, 235, 150)};
    return {who, text, -2, Color(165, 190, 210)};
}

struct Particle {
    float x, y, vx, vy, life, maxLife, size;
    Color color;
    int kind; // 0 polvo, 1 brillo, 2 agua, 3 hoja
};

struct Ambient {
    float x, y, vx, vy, phase;
};

class Game {
public:
    bool init(bool hidden, const std::string& assetDir);
    void shutdown();
    void run();
    void capture(const std::string& dir);
    void setAutoplay() { autoplay_ = true; }
    void setTouchMode() { touchMode_ = true; }
    void setFullscreen();

private:
    // estados y bucle
    void pollEvents();
    void update(float dt);
    void render();
    void runAutoplay();
    void autoplayStep(float dt);
    void autoplayInput();
    void present() { SDL_RenderPresent(ren_); }
    // Escala el lienzo 1280x720 a todo el tamaño real del renderer (sin bandas).
    void applyRenderScale();
    void startGame();
    void respawn(bool fromWater);
    void togglePause();

    // entrada táctil (Android)
    SDL_FPoint touchGamePos(float nx, float ny);
    static bool touchHit(float x, float y, float cx, float cy, float r);
    void handleTouchDown(const SDL_TouchFingerEvent& f);
    void handleTouchUp(const SDL_TouchFingerEvent& f);
    void handleTouchUi(float x, float y);
    void drawTouchControls();
    void drawTouchButton(float cx, float cy, float r, bool held, int icon);

    // jugador y cámara
    void updatePlayer(float dt);
    void updateCamera(float dt, bool snap = false);
    void killPlayer(bool water);
    void emitDust(int n, float x, float y, float power = 1.0f);
    void emitGlow(int n, float x, float y, float power = 1.0f);
    void emitSplash(float x, float y);

    // historia
    void startDialogue(std::vector<Line> lines, std::function<void()> done = nullptr);
    void advanceDialogue();
    void updateDialogue(float dt);
    void checkStory();
    void setZone(int z);
    void showToast(const std::string& text);

    // partículas / ambiente
    void updateParticles(float dt);
    void updateAmbient(float dt, int target);
    void initAmbient(int n);

    // dibujo
    void drawTitleBackdrop(float drift);
    void drawWorld();
    void drawNara(float cx, float feet, bool moving, int face, float alpha = 1.0f,
                  float scaleMul = 1.0f);
    void drawNaraSilhouette(float cx, float feet, float scale, Color c, int face);
    void drawFirefly(float x, float y, float t, float alpha = 1.0f);
    void drawAmbient();
    void drawParticles(bool additive);
    void drawLights();
    void drawHud();
    void drawDialogue();
    void drawPause();
    void drawTitle();
    void drawIntro();
    void drawEnding();
    void drawEpilogue();
    void drawCredits();
    void drawFades();

    // utilidades
    float playerCenterX() const { return player_.x + 5.0f; }
    float playerFeet() const { return player_.y + 18.0f; }
    SDL_Rect cameraClamp(float x, float y) const;

    SDL_Window* win_ = nullptr;
    SDL_Renderer* ren_ = nullptr;
    bool hidden_ = false;
    Gfx gfx_;
    Audio audio_;
    Level level_;

    GameState state_ = GameState::Title;
    bool running_ = true;
    bool paused_ = false;
    float time_ = 0.0f;
    float dt_ = 0.016f;
    float camX_ = 0.0f, camY_ = 324.0f;
    float bloom_ = 0.0f;
    float fade_ = 1.0f;
    float flash_ = 0.0f;
    float whiteFade_ = 0.0f;

    struct Player {
        float x = 58, y = 504, vx = 0, vy = 0;
        bool onGround = false;
        int face = 1;
        float coyote = 0, jumpBuf = 0;
        float animT = 0;
        float scale = 1;
        float landT = 0;
    } player_;
    bool jumpHeld_ = false;
    bool dying_ = false;
    float deathT_ = 0;
    float dustTimer_ = 0;

    bool inLeft_ = false, inRight_ = false;
    bool jumpQueued_ = false;
    bool interactQueued_ = false;
    bool screenshotQueued_ = false;
    bool fullscreen_ = false;
    bool vsync_ = false;

    // diálogo
    std::vector<Line> dlg_;
    int dlgIndex_ = 0;
    int dlgTotal_ = 0;
    float dlgVisible_ = 0;
    float dlgAnim_ = 0;
    bool dialogueActive_ = false;
    std::function<void()> dlgDone_;

    // progreso
    int fireflyCount_ = 0;
    int respawnCP_ = -1;
    bool flags_[8] = {false};
    bool awaitingRelease_ = false;
    bool nearTree_ = false;
    bool finalLineShown_ = false;
    int zone_ = 0;
    float zoneBannerT_ = 10;
    std::string zoneText_;
    std::string toast_;
    float toastT_ = 0;
    float playTime_ = 0;
    float startLineTimer_ = 0;
    bool startLineDone_ = false;

    // intro y epílogo
    int introPage_ = 0;
    float introT_ = 0;
    float endingT_ = 0;
    float epiT_ = 0;
    float creditsT_ = 0;

    // partículas
    std::vector<Particle> particles_;
    std::vector<Ambient> ambient_;
    std::mt19937 rng_{0x51C3A7u};
    float endingSpawn_ = 0;
    float releasePhase_ = 0;

    // captura / pruebas automáticas
    int shotN_ = 0;
    std::string assetDir_;
    std::string shotDir_ = "captures";
    bool autoplay_ = false;
    float botJumpHold_ = 0;
    float botMaxX_ = 0;
    float botIdleTime_ = 0;
    int botDeaths_ = 0;

    // controles táctiles
    bool touchMode_ = LUCIERNAGA_ANDROID != 0;
    struct TouchButton { SDL_FingerID id = -1; bool held = false; };
    TouchButton tLeft_, tRight_, tJump_, tInteract_;
};

// ---------------------------------------------------------------------------
// inicialización
// ---------------------------------------------------------------------------

bool Game::init(bool hidden, const std::string& assetDir) {
    hidden_ = hidden;
    assetDir_ = assetDir;
#if LUCIERNAGA_ANDROID
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "1");
    // El tacto ya se gestiona con SDL_FINGER*; evita que se duplique como ratón.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
#endif
#if defined(_WIN32)
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    Uint32 winFlags;
#if LUCIERNAGA_ANDROID
    winFlags = hidden ? SDL_WINDOW_HIDDEN : (SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN);
#else
    winFlags = hidden ? SDL_WINDOW_HIDDEN : (SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
#endif
    win_ = SDL_CreateWindow("La última luciérnaga", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, OUT_W, OUT_H, winFlags);
    if (!win_) {
        std::fprintf(stderr, "Window: %s\n", SDL_GetError());
        return false;
    }
    Uint32 renFlags = hidden ? SDL_RENDERER_SOFTWARE
                             : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    ren_ = SDL_CreateRenderer(win_, -1, renFlags);
    if (!ren_) ren_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_SOFTWARE);
    if (!ren_) {
        std::fprintf(stderr, "Renderer: %s\n", SDL_GetError());
        return false;
    }
    // Sin SDL_RenderSetLogicalSize: ese modo conserva el 16:9 y deja bandas
    // negras. Escalamos el lienzo para ocupar todo el renderer, de modo que
    // en 16:9 se ve nítido a resolución nativa y en pantallas más anchas
    // (móviles 19.5:9) la imagen se estira hasta los bordes.
    applyRenderScale();
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    SDL_RendererInfo ri{};
    SDL_GetRendererInfo(ren_, &ri);
    vsync_ = (ri.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
    if (!touchMode_) SDL_ShowCursor(SDL_DISABLE);

    // En Android las capturas van a un directorio escribible de la app.
#if LUCIERNAGA_ANDROID
    if (char* pref = SDL_GetPrefPath("ericson", "la-ultima-luciernaga")) {
        shotDir_ = pref;
        SDL_free(pref);
    }
#endif

    if (!gfx_.init(ren_, assetDir)) return false;
    audio_.init();

    fade_ = 1.0f;
    camX_ = 0;
    camY_ = 324;
    initAmbient(14);
    return true;
}

void Game::shutdown() {
    audio_.shutdown();
    gfx_.shutdown();
    if (ren_) SDL_DestroyRenderer(ren_);
    if (win_) SDL_DestroyWindow(win_);
    SDL_Quit();
}

void Game::applyRenderScale() {
    if (!ren_) return;
    int ow = OUT_W, oh = OUT_H;
    if (SDL_GetRendererOutputSize(ren_, &ow, &oh) != 0 || ow <= 0 || oh <= 0) {
        ow = OUT_W;
        oh = OUT_H;
    }
    SDL_RenderSetIntegerScale(ren_, SDL_FALSE);
    SDL_RenderSetViewport(ren_, nullptr);
    SDL_RenderSetScale(ren_, (float)ow / (float)OUT_W, (float)oh / (float)OUT_H);
}

void Game::setFullscreen() {
    fullscreen_ = true;
    SDL_SetWindowFullscreen(win_, SDL_WINDOW_FULLSCREEN_DESKTOP);
    applyRenderScale();
}

// ---------------------------------------------------------------------------
// entrada
// ---------------------------------------------------------------------------

void Game::togglePause() {
    if (state_ != GameState::Play) return;
    paused_ = !paused_;
    audio_.setPaused(paused_);
    audio_.play(Sfx::Ui);
}

SDL_FPoint Game::touchGamePos(float nx, float ny) {
    // Sin tamaño lógico, SDL entrega el dedo normalizado a la ventana. El
    // lienzo ocupa la ventana completa, así que la conversión es directa.
    return SDL_FPoint{nx * (float)(OUT_W / RENDER_SCALE),
                      ny * (float)(OUT_H / RENDER_SCALE)};
}

bool Game::touchHit(float x, float y, float cx, float cy, float r) {
    float dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= r * r;
}

void Game::handleTouchDown(const SDL_TouchFingerEvent& f) {
    SDL_FPoint p = touchGamePos(f.x, f.y);
    if (p.x < -8 || p.x > 648 || p.y < -8 || p.y > 368) return;

    if (paused_) {
        if (!touchMode_) return;
        // Botones de la pantalla de pausa (ver drawPause).
        if (p.x > 200 && p.x < 440) {
            if (p.y > 150 && p.y < 186) {
                paused_ = false; audio_.setPaused(false); audio_.play(Sfx::Ui);
            } else if (p.y >= 186 && p.y < 220) {
                paused_ = false; audio_.setPaused(false); respawn(false);
            } else if (p.y >= 220 && p.y < 254) {
                paused_ = false; audio_.setPaused(false);
                state_ = GameState::Title; fade_ = 1.0f; initAmbient(20);
                audio_.update(0.1f, 0, false);
            } else if (p.y >= 254 && p.y < 288) {
                running_ = false;
            }
        }
        return;
    }

    if (state_ == GameState::Play) {
        if (dialogueActive_) { advanceDialogue(); return; }
        if (touchHit(p.x, p.y, 612, 64, 18)) { togglePause(); return; }

        if (touchHit(p.x, p.y, 54, 300, 34) && tLeft_.id < 0) {
            tLeft_.id = f.fingerId; tLeft_.held = true;
        } else if (touchHit(p.x, p.y, 126, 300, 34) && tRight_.id < 0) {
            tRight_.id = f.fingerId; tRight_.held = true;
        } else if (touchHit(p.x, p.y, 586, 300, 38) && tJump_.id < 0) {
            tJump_.id = f.fingerId; tJump_.held = true;
            if (!dying_) player_.jumpBuf = 0.14f;
        } else if (touchHit(p.x, p.y, 512, 260, 30) && tInteract_.id < 0) {
            tInteract_.id = f.fingerId; tInteract_.held = true;
            interactQueued_ = true;
        }
        return;
    }

    handleTouchUi(p.x, p.y);
}

void Game::handleTouchUp(const SDL_TouchFingerEvent& f) {
    if (tLeft_.id == f.fingerId) { tLeft_ = TouchButton{}; }
    if (tRight_.id == f.fingerId) { tRight_ = TouchButton{}; }
    if (tJump_.id == f.fingerId) { tJump_ = TouchButton{}; }
    if (tInteract_.id == f.fingerId) { tInteract_ = TouchButton{}; }
}

void Game::handleTouchUi(float x, float y) {
    switch (state_) {
        case GameState::Title:
            state_ = GameState::Intro;
            introPage_ = 0;
            introT_ = 0;
            audio_.play(Sfx::Ui);
            break;
        case GameState::Intro:
            introPage_++;
            introT_ = 0;
            audio_.play(Sfx::Ui);
            if (introPage_ >= 3) startGame();
            break;
        case GameState::Ending:
            if (endingT_ > 2.0f) whiteFade_ = std::max(whiteFade_, 0.6f);
            break;
        case GameState::Epilogue:
            if (epiT_ < 7.0f) epiT_ = 7.0f;
            else { state_ = GameState::Credits; creditsT_ = 0; }
            break;
        case GameState::Credits:
            if (y > 276 && y < 306) startGame();
            else if (y >= 306 && y < 336) {
                state_ = GameState::Title;
                fade_ = 1.0f;
                initAmbient(20);
            }
            break;
        default:
            break;
    }
}

void Game::pollEvents() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) { running_ = false; continue; }
        if (ev.type == SDL_WINDOWEVENT &&
            (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
             ev.window.event == SDL_WINDOWEVENT_RESIZED)) {
            // Al cambiar de tamaño o entrar/salir de pantalla completa hay que
            // recalcular la escala para que el lienzo siga llenando la ventana.
            applyRenderScale();
            continue;
        }
        // En Android también puede haber ratón (DeX, Chromebooks) y en PC el
        // modo --touch se prueba con clics. Se traduce a un dedo sintético.
        if (touchMode_ && ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT &&
            ev.button.which != SDL_TOUCH_MOUSEID) {
            int ww = 1, wh = 1;
            SDL_GetWindowSize(win_, &ww, &wh);
            SDL_TouchFingerEvent f{};
            f.fingerId = -2;
            f.x = ev.button.x / (float)ww;
            f.y = ev.button.y / (float)wh;
            handleTouchDown(f);
            continue;
        }
        if (touchMode_ && ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT &&
            ev.button.which != SDL_TOUCH_MOUSEID) {
            SDL_TouchFingerEvent f{};
            f.fingerId = -2;
            handleTouchUp(f);
            continue;
        }
        if (ev.type == SDL_FINGERDOWN) {
            if (touchMode_) handleTouchDown(ev.tfinger);
            continue;
        }
        if (ev.type == SDL_FINGERUP) {
            if (touchMode_) handleTouchUp(ev.tfinger);
            continue;
        }
#if LUCIERNAGA_ANDROID
        if (ev.type == SDL_APP_WILLENTERBACKGROUND) {
            if (state_ == GameState::Play && !paused_) togglePause();
            continue;
        }
#endif
        if (ev.type == SDL_KEYDOWN) {
            bool repeat = ev.key.repeat != 0;
            SDL_Keycode k = ev.key.keysym.sym;
#if LUCIERNAGA_ANDROID
            if (k == SDLK_AC_BACK && !repeat) {
                if (paused_) {
                    paused_ = false;
                    audio_.setPaused(false);
                    audio_.play(Sfx::Ui);
                } else if (state_ == GameState::Play) {
                    togglePause();
                } else if (state_ == GameState::Intro || state_ == GameState::Credits) {
                    state_ = GameState::Title;
                    fade_ = 1.0f;
                    initAmbient(20);
                } else if (state_ == GameState::Epilogue) {
                    state_ = GameState::Credits;
                    creditsT_ = 0;
                } else if (state_ == GameState::Title) {
                    running_ = false;
                }
                continue;
            }
#endif
            if (k == SDLK_F11 && !repeat) {
                fullscreen_ = !fullscreen_;
                SDL_SetWindowFullscreen(win_, fullscreen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                applyRenderScale();
            }
            if (k == SDLK_F12 && !repeat) screenshotQueued_ = true;

            if (paused_) {
                if (k == SDLK_ESCAPE || k == SDLK_p) {
                    paused_ = false;
                    audio_.setPaused(false);
                    audio_.play(Sfx::Ui);
                } else if (k == SDLK_r) {
                    paused_ = false;
                    audio_.setPaused(false);
                    respawn(false);
                } else if (k == SDLK_t) {
                    paused_ = false;
                    audio_.setPaused(false);
                    state_ = GameState::Title;
                    fade_ = 1.0f;
                    initAmbient(20);
                    audio_.update(0.1f, 0, false);
                } else if (k == SDLK_q) {
                    running_ = false;
                }
                continue;
            }

            if (state_ == GameState::Title) {
                if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
                    state_ = GameState::Intro;
                    introPage_ = 0;
                    introT_ = 0;
                    audio_.play(Sfx::Ui);
                } else if (k == SDLK_ESCAPE) {
                    running_ = false;
                }
            } else if (state_ == GameState::Intro) {
                if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE || k == SDLK_e) {
                    introPage_++;
                    introT_ = 0;
                    audio_.play(Sfx::Ui);
                    if (introPage_ >= 3) startGame();
                } else if (k == SDLK_ESCAPE) {
                    state_ = GameState::Title;
                    fade_ = 0.6f;
                }
            } else if (state_ == GameState::Play) {
                if (dialogueActive_) {
                    if (!repeat && (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE || k == SDLK_e))
                        advanceDialogue();
                } else {
                    if (k == SDLK_ESCAPE || k == SDLK_p) {
                        paused_ = true;
                        audio_.setPaused(true);
                        audio_.play(Sfx::Ui);
                    } else if (k == SDLK_e && !repeat) {
                        interactQueued_ = true;
                    } else if (!repeat && (k == SDLK_SPACE || k == SDLK_w || k == SDLK_UP || k == SDLK_z)) {
                        // el buffer se procesa en updatePlayer
                        if (!dying_) player_.jumpBuf = 0.14f;
                    }
                }
            } else if (state_ == GameState::Ending) {
                if ((k == SDLK_RETURN || k == SDLK_SPACE) && endingT_ > 2.0f) {
                    whiteFade_ = std::max(whiteFade_, 0.6f);
                }
            } else if (state_ == GameState::Epilogue) {
                if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
                    if (epiT_ < 7.0f) epiT_ = 7.0f;
                    else { state_ = GameState::Credits; creditsT_ = 0; }
                }
            } else if (state_ == GameState::Credits) {
                if (k == SDLK_r) startGame();
                else if (k == SDLK_t) { state_ = GameState::Title; fade_ = 1.0f; initAmbient(20); }
                else if (k == SDLK_ESCAPE) running_ = false;
            }
            continue;
        }
        if (ev.type == SDL_KEYUP) {
            SDL_Keycode k = ev.key.keysym.sym;
            if (k == SDLK_SPACE || k == SDLK_w || k == SDLK_UP || k == SDLK_z)
                jumpHeld_ = false;
        }
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    inLeft_ = keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A] || tLeft_.held;
    inRight_ = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D] || tRight_.held;
    bool jumpDown = keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_W] ||
                    keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_Z] || tJump_.held;
    jumpHeld_ = jumpDown;

    // mando
    static int padJumpPrev = 0, padStartPrev = 0, padInteractPrev = 0;
    static SDL_GameController* pad = nullptr;
    if (!pad && SDL_NumJoysticks() > 0 && SDL_IsGameController(0))
        pad = SDL_GameControllerOpen(0);
    if (pad) {
        int j = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A);
        int st = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START);
        int it = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B);
        int ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        if (std::abs(ax) > 9000) {
            inLeft_ = inLeft_ || ax < -9000;
            inRight_ = inRight_ || ax > 9000;
        }
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) inLeft_ = true;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) inRight_ = true;
        if (j && !padJumpPrev && state_ == GameState::Play && !dialogueActive_ && !paused_ && !dying_)
            player_.jumpBuf = 0.14f;
        jumpHeld_ = jumpHeld_ || j;
        if (st && !padStartPrev && state_ == GameState::Play && !dialogueActive_) {
            paused_ = !paused_;
            audio_.setPaused(paused_);
        }
        if (it && !padInteractPrev && state_ == GameState::Play) {
            if (dialogueActive_) advanceDialogue();
            else interactQueued_ = true;
        }
        padJumpPrev = j; padStartPrev = st; padInteractPrev = it;
    }
}

// ---------------------------------------------------------------------------
// juego
// ---------------------------------------------------------------------------

void Game::startGame() {
    state_ = GameState::Play;
    paused_ = false;
    fade_ = 1.0f;
    flash_ = 0;
    whiteFade_ = 0;
    bloom_ = 0;
    fireflyCount_ = 0;
    respawnCP_ = -1;
    awaitingRelease_ = false;
    nearTree_ = false;
    finalLineShown_ = false;
    dialogueActive_ = false;
    dlg_.clear();
    dlgDone_ = nullptr;
    particles_.clear();
    for (auto& f : level_.fireflies) f.taken = false;
    for (auto& c : level_.checkpoints) c.active = false;
    for (bool& b : flags_) b = false;
    startLineDone_ = false;
    startLineTimer_ = 0;
    playTime_ = 0;
    zone_ = 0;
    zoneText_ = "I · EL SENDERO";
    zoneBannerT_ = 0;
    toast_.clear();
    toastT_ = 0;
    player_ = Player();
    player_.x = 3.4f * TILE - 5;
    player_.y = 29 * TILE - 18;
    player_.face = 1;
    camX_ = 0;
    camY_ = 324;
    dying_ = false;
    deathT_ = 0;
    initAmbient(8);
    updateCamera(0, true);
    audio_.update(0.1f, 0, false);
    audio_.play(Sfx::Ui);
}

void Game::respawn(bool fromWater) {
    int cp = respawnCP_;
    float feet;
    float cx;
    if (cp >= 0 && cp < (int)level_.checkpoints.size()) {
        auto& c = level_.checkpoints[cp];
        cx = c.tx * TILE + 9;
        feet = c.ty * TILE;
    } else {
        cx = 3.4f * TILE;
        feet = 29 * TILE;
    }
    player_.x = cx - 5;
    player_.y = feet - 18;
    player_.vx = player_.vy = 0;
    player_.onGround = false;
    player_.coyote = 0;
    player_.jumpBuf = 0;
    player_.scale = 1;
    dying_ = false;
    deathT_ = 0;
    fade_ = 1.0f;
    updateCamera(0, true);
    if (fromWater) emitSplash(cx, feet);
}

void Game::killPlayer(bool water) {
    if (dying_) return;
    dying_ = true;
    deathT_ = 0;
    if (autoplay_) botDeaths_++;
    player_.vx = 0;
    player_.vy = 0;
    if (water) {
        audio_.play(Sfx::Splash);
        emitSplash(playerCenterX(), playerFeet());
    } else {
        audio_.play(Sfx::Splash);
    }
}

void Game::updatePlayer(float dt) {
    if (dying_) {
        deathT_ += dt;
        player_.y += 24 * dt;
        if (deathT_ > 0.55f) respawn(level_.inWater(playerCenterX(), playerFeet() - 4));
        return;
    }

    float dir = (inRight_ ? 1.0f : 0.0f) - (inLeft_ ? 1.0f : 0.0f);
    if (dir != 0) player_.face = dir > 0 ? 1 : -1;

    float accel = player_.onGround ? 1500.0f : 950.0f;
    float maxSpeed = 158.0f;
    if (dir != 0) {
        if (player_.vx * dir < maxSpeed)
            player_.vx += dir * accel * dt;
        player_.vx = clampf(player_.vx, -maxSpeed, maxSpeed);
    } else {
        float fric = player_.onGround ? 1800.0f : 420.0f;
        if (player_.vx > 0) player_.vx = std::max(0.0f, player_.vx - fric * dt);
        else player_.vx = std::min(0.0f, player_.vx + fric * dt);
    }

    player_.coyote = player_.onGround ? 0.10f : std::max(0.0f, player_.coyote - dt);
    if (player_.jumpBuf > 0) player_.jumpBuf -= dt;
    if (player_.jumpBuf > 0 && player_.coyote > 0) {
        player_.vy = -374.0f;
        player_.onGround = false;
        player_.coyote = 0;
        player_.jumpBuf = 0;
        player_.scale = 0.94f;
        audio_.play(Sfx::Jump);
        emitDust(4, playerCenterX(), playerFeet(), 0.7f);
    }
    if (!jumpHeld_ && player_.vy < -110.0f) player_.vy += 1750.0f * dt;

    player_.vy += 1280.0f * dt;
    player_.vy = std::min(player_.vy, 580.0f);

    // movimiento horizontal con colisión
    float nx = player_.x + player_.vx * dt;
    if (level_.hits(nx, player_.y, 10, 18)) {
        if (player_.vx > 0) {
            int tx = (int)std::floor((nx + 10) / TILE);
            player_.x = tx * TILE - 10.01f;
        } else if (player_.vx < 0) {
            int tx = (int)std::floor(nx / TILE);
            player_.x = (tx + 1) * TILE + 0.01f;
        }
        player_.vx = 0;
    } else {
        player_.x = nx;
    }

    // movimiento vertical
    bool wasGround = player_.onGround;
    player_.onGround = false;
    float ny = player_.y + player_.vy * dt;
    if (level_.hits(player_.x, ny, 10, 18)) {
        if (player_.vy > 0) {
            int ty = (int)std::floor((ny + 18) / TILE);
            player_.y = ty * TILE - 18.01f;
            if (!wasGround) {
                if (player_.vy > 260) {
                    audio_.play(Sfx::Land);
                    emitDust(5, playerCenterX(), playerFeet(), 0.8f);
                }
                player_.scale = 1.06f;
                player_.landT = 0.12f;
            }
            player_.onGround = true;
            player_.vy = 0;
        } else if (player_.vy < 0) {
            int ty = (int)std::floor(ny / TILE);
            player_.y = (ty + 1) * TILE + 0.01f;
            player_.vy = 0;
        }
    } else {
        player_.y = ny;
    }

    if (player_.onGround) {
        player_.scale += (1.0f - player_.scale) * std::min(1.0f, dt * 12);
        dustTimer_ -= dt;
        if (std::abs(player_.vx) > 80 && dustTimer_ <= 0) {
            dustTimer_ = 0.17f;
            emitDust(1, playerCenterX() - player_.face * 6, playerFeet(), 0.4f);
        }
    } else {
        player_.scale += (1.0f - player_.scale) * std::min(1.0f, dt * 6);
    }
    player_.animT += dt * (0.7f + std::abs(player_.vx) / 60.0f);

    if (player_.y > level_.h * TILE + 40) killPlayer(false);
    if (level_.inWater(playerCenterX(), playerFeet() - 2)) killPlayer(true);
}

void Game::updateCamera(float dt, bool snap) {
    float targetX = playerCenterX() - 300.0f + player_.face * 26.0f;
    float targetY = player_.y + 9.0f - 188.0f;
    if (state_ == GameState::Ending) {
        targetX = 4400.0f;
        targetY = 24.0f;
    }
    targetX = clampf(targetX, 0.0f, level_.w * TILE - VIEW_W);
    targetY = clampf(targetY, 0.0f, level_.h * TILE - VIEW_H);
    if (snap) {
        camX_ = targetX; camY_ = targetY;
    } else {
        float k = 1.0f - std::exp(-dt * 6.5f);
        camX_ = lerpf(camX_, targetX, k);
        camY_ = lerpf(camY_, targetY, k);
    }
}

void Game::checkStory() {
    if (dialogueActive_ || dying_) return;
    float cx = playerCenterX();

    if (!flags_[4] && playTime_ > 1.1f) {
        flags_[4] = true;
        startDialogue({
            L("NARA", "Teo decía que las luciérnagas guían a los que se pierden."),
            L("NARA", "Entonces... ¿también pueden guiarme hasta ti?")
        });
        return;
    }

    if (!flags_[0] && cx > 8.4f * TILE) {
        flags_[0] = true;
        startDialogue({
            L("NARRADOR", "«No cruces el arroyo cuando la luna esté alta», decía la abuela."),
            L("NARRADOR", "Nara nunca supo si era una advertencia o una promesa.")
        });
        return;
    }

    if (!flags_[1] && cx > 92.0f * TILE) {
        flags_[1] = true;
        startDialogue({
            L("NARRADOR", "El arroyo brilla como si guardara estrellas."),
            L("NARRADOR", "Pero el agua está quieta. Demasiado quieta.")
        });
        return;
    }

    if (!flags_[2] && cx > 178.0f * TILE) {
        flags_[2] = true;
        startDialogue({
            L("NARRADOR", "La colina de los sauces. Desde aquí se veía amanecer."),
            L("NARRADOR", "Aquí lo esperó cada noche. Aquí aprendió a contar estrellas en vez de días.")
        });
        return;
    }

    if (!flags_[3] && fireflyCount_ >= 3 && cx > 259.0f * TILE) {
        flags_[3] = true;
        finalLineShown_ = true;
        startDialogue({
            L("NARRADOR", "Bajo el árbol, las tres luciérnagas dibujan un círculo."),
            L("TEO", "La noche que me fui, te saqué del agua. Me viste caer, pero no lo recuerdas."),
            L("NARA", "Teo..."),
            L("TEO", "Me quedé porque tú no me dejabas ir. Las luciérnagas no se apagan, Nara. Solo cambian de sitio."),
            L("NARA", "¿Y si te olvido?"),
            L("TEO", "Entonces me habré ido de verdad. Pero no estaré solo."),
            L("NARA", "Tengo miedo."),
            L("TEO", "Yo también. Suéltanos.")
        }, [this]() {
            awaitingRelease_ = true;
            showToast("El árbol de la luz despierta");
            emitGlow(30, 268 * TILE, 10 * TILE, 1.2f);
            audio_.play(Sfx::Checkpoint);
        });
        return;
    }
}

void Game::startDialogue(std::vector<Line> lines, std::function<void()> done) {
    if (lines.empty()) return;
    dlg_ = std::move(lines);
    dlgIndex_ = 0;
    dlgVisible_ = 0;
    dlgTotal_ = utf8Count(dlg_[0].text);
    dlgAnim_ = 0;
    dialogueActive_ = true;
    dlgDone_ = std::move(done);
    player_.vx = 0;
    player_.vy = 0;
    audio_.play(Sfx::Blip);
}

void Game::advanceDialogue() {
    if (!dialogueActive_) return;
    if (dlgVisible_ < dlgTotal_ - 0.01f) {
        dlgVisible_ = (float)dlgTotal_;
        return;
    }
    dlgIndex_++;
    if (dlgIndex_ >= (int)dlg_.size()) {
        dialogueActive_ = false;
        dlg_.clear();
        auto done = dlgDone_;
        dlgDone_ = nullptr;
        if (done) done();
    } else {
        dlgVisible_ = 0;
        dlgTotal_ = utf8Count(dlg_[dlgIndex_].text);
        audio_.play(Sfx::Blip);
    }
}

void Game::updateDialogue(float dt) {
    if (!dialogueActive_) { dlgAnim_ = std::max(0.0f, dlgAnim_ - dt * 5); return; }
    dlgAnim_ = std::min(1.0f, dlgAnim_ + dt * 8);
    if (dlgVisible_ < dlgTotal_) {
        int before = (int)dlgVisible_;
        dlgVisible_ = std::min((float)dlgTotal_, dlgVisible_ + dt * 56.0f);
        if (before / 4 != (int)dlgVisible_ / 4) audio_.play(Sfx::Blip);
    }
}

void Game::showToast(const std::string& text) {
    toast_ = text;
    toastT_ = 0;
}

void Game::setZone(int z) {
    if (z == zone_ && z != 0) return;
    zone_ = z;
    zoneBannerT_ = 0;
    switch (z) {
        case 0: zoneText_ = "I · EL SENDERO"; break;
        case 1: zoneText_ = "II · EL ARROYO"; break;
        case 2: zoneText_ = "III · LA COLINA"; break;
        case 3: zoneText_ = "EL ÁRBOL DE LA LUZ"; break;
        default: zoneText_ = ""; break;
    }
    audio_.play(Sfx::Ui);
}

void Game::update(float dt) {
    dt_ = dt;
    if (state_ == GameState::Title) {
        updateAmbient(dt, 26);
        updateParticles(dt);
        updateDialogue(dt);
        if (fade_ > 0) fade_ = std::max(0.0f, fade_ - dt * 1.4f);
        audio_.update(dt, 0, false);
        return;
    }
    if (state_ == GameState::Intro) {
        updateAmbient(dt, 22);
        updateParticles(dt);
        introT_ += dt;
        if (introT_ < 0.6f) fade_ = std::max(fade_, 0.0f);
        audio_.update(dt, 0, false);
        return;
    }
    if (state_ == GameState::Play) {
        playTime_ += dt;
        if (paused_) return;
        updateDialogue(dt);
        if (dialogueActive_) {
            // pequeña respiración del bosque mientras hablan
            updateAmbient(dt, 8 + fireflyCount_ * 9);
            updateParticles(dt);
            updateCamera(dt);
            if (fade_ > 0) fade_ = std::max(0.0f, fade_ - dt * 2.2f);
            if (flash_ > 0) flash_ = std::max(0.0f, flash_ - dt * 2.5f);
            audio_.update(dt, fireflyCount_, false);
            return;
        }

        // input de interacción
        if (interactQueued_) {
            interactQueued_ = false;
            if (awaitingRelease_ && playerCenterX() > 262.0f * TILE) {
                state_ = GameState::Ending;
                endingT_ = 0;
                releasePhase_ = 0;
                player_.vx = player_.vy = 0;
                audio_.play(Sfx::Release);
                audio_.update(0.1f, 3, true);
                return;
            }
        }

        updatePlayer(dt);
        updateCamera(dt);
        updateAmbient(dt, 7 + fireflyCount_ * 11);
        updateParticles(dt);

        // checkpoints
        for (int i = 0; i < (int)level_.checkpoints.size(); i++) {
            auto& cp = level_.checkpoints[i];
            if (!cp.active && playerCenterX() > cp.tx * TILE - 12 &&
                std::abs((player_.y + 18) - cp.ty * TILE) < 44) {
                cp.active = true;
                respawnCP_ = i;
                showToast("Faro encendido — el camino queda a salvo");
                audio_.play(Sfx::Checkpoint);
                emitGlow(16, cp.tx * TILE, cp.ty * TILE - 40, 0.8f);
            }
        }

        // luciérnagas
        for (auto& f : level_.fireflies) {
            if (f.taken) continue;
            float fx = f.x + std::sin(time_ * 1.3f + f.phase) * 7.0f;
            float fy = f.y + std::sin(time_ * 1.9f + f.phase) * 4.5f;
            float dx = fx - playerCenterX();
            float dy = fy - (playerFeet() - 10);
            if (dx * dx + dy * dy < 31 * 31) {
                f.taken = true;
                fireflyCount_++;
                audio_.play(Sfx::Collect);
                flash_ = 1.0f;
                emitGlow(26, fx, fy, 1.0f);
                if (f.memory == 0) {
                    startDialogue({
                        L("NARA", "Teo... aquí saltábamos los charcos contando hasta tres."),
                        L("TEO", "Uno... dos... ¡tres! No mires abajo, Nara."),
                        L("NARA", "Nunca miré. Hasta la noche en que te fuiste."),
                        L("NARRADOR", "La luciérnaga se posa en su hombro. El bosque respira.")
                    });
                } else if (f.memory == 1) {
                    startDialogue({
                        L("NARA", "Teo, ya no recuerdo tu cara. Solo tu voz contando."),
                        L("TEO", "Cuando tengas miedo, busca la luz más pequeña."),
                        L("TEO", "Es la que más se esfuerza."),
                        L("NARA", "¿Dónde estás?"),
                        L("NARRADOR", "El agua, debajo, parece contener una respuesta. Nara no se atreve a mirar.")
                    });
                } else {
                    startDialogue({
                        L("NARA", "Ya casi estoy, Teo. Solo un poco más."),
                        L("TEO", "Nara... no viniste a buscarme."),
                        L("NARA", "¿Qué?"),
                        L("TEO", "Viniste a despedirte."),
                        L("NARA", "No. No, no, no."),
                        L("NARRADOR", "La tercera luciérnaga tiembla. El árbol de la luz despierta.")
                    });
                    bloom_ = 0.35f;
                }
            }
        }

        // carteles y escenas
        checkStory();

        if (fireflyCount_ >= 1 && playerCenterX() > 84 * TILE && zone_ == 0) setZone(1);
        if (fireflyCount_ >= 1 && playerCenterX() > 170 * TILE && zone_ == 1) setZone(2);
        if (playerCenterX() > 250 * TILE && zone_ == 2) setZone(3);

        nearTree_ = awaitingRelease_ && playerCenterX() > 262.0f * TILE;
        if (zoneBannerT_ < 6) zoneBannerT_ += dt;
        if (toastT_ < 6) toastT_ += dt;
        if (fade_ > 0) fade_ = std::max(0.0f, fade_ - dt * 2.2f);
        if (flash_ > 0) flash_ = std::max(0.0f, flash_ - dt * 2.2f);
        audio_.update(dt, fireflyCount_, false);
        return;
    }
    if (state_ == GameState::Ending) {
        endingT_ += dt;
        releasePhase_ += dt;
        bloom_ = std::min(1.0f, endingT_ / 4.0f);
        updateAmbient(dt, 40);
        updateParticles(dt);
        updateCamera(dt);
        // ascuas que suben
        endingSpawn_ -= dt;
        if (endingSpawn_ <= 0) {
            endingSpawn_ = 0.055f;
            Particle p;
            p.x = 268 * TILE + (float)((int)(rng_() % 260) - 130);
            p.y = 12 * TILE + (float)((int)(rng_() % 40) - 30);
            p.vx = (float)((int)(rng_() % 20) - 10) * 0.4f;
            p.vy = -18.0f - (float)(rng_() % 40);
            p.life = 4.5f + (float)(rng_() % 25) * 0.1f;
            p.maxLife = p.life;
            p.size = 1.0f + (float)(rng_() % 20) * 0.1f;
            p.color = Color(230, 255, 140, 200);
            p.kind = 1;
            particles_.push_back(p);
        }
        flash_ = std::max(0.0f, flash_ - dt);
        if (endingT_ > 5.0f) whiteFade_ = std::min(1.0f, (endingT_ - 5.0f) / 1.6f);
        if (endingT_ > 6.9f) {
            state_ = GameState::Epilogue;
            epiT_ = 0;
            whiteFade_ = 0;
            fade_ = 0;
            particles_.clear();
        }
        audio_.update(dt, 3, true);
        return;
    }
    if (state_ == GameState::Epilogue) {
        epiT_ += dt;
        updateAmbient(dt, 10);
        updateParticles(dt);
        audio_.update(dt, 3, false);
        return;
    }
    if (state_ == GameState::Credits) {
        creditsT_ += dt;
        updateAmbient(dt, 8);
        updateParticles(dt);
        audio_.update(dt, 0, false);
        return;
    }
}

// ---------------------------------------------------------------------------
// partículas
// ---------------------------------------------------------------------------

void Game::emitDust(int n, float x, float y, float power) {
    for (int i = 0; i < n; i++) {
        Particle p;
        p.x = x + (float)((int)(rng_() % 11) - 5);
        p.y = y - 1;
        p.vx = ((float)(rng_() % 100) / 100.0f - 0.5f) * 24.0f * power;
        p.vy = -(8.0f + (float)(rng_() % 20)) * power;
        p.life = 0.35f + (float)(rng_() % 30) / 100.0f;
        p.maxLife = p.life;
        p.size = 1.0f + (float)(rng_() % 20) / 10.0f;
        p.color = Color(120, 140, 150, 90);
        p.kind = 0;
        particles_.push_back(p);
    }
}

void Game::emitGlow(int n, float x, float y, float power) {
    for (int i = 0; i < n; i++) {
        Particle p;
        p.x = x; p.y = y;
        float ang = (float)(rng_() % 628) / 100.0f;
        float sp = (10.0f + (float)(rng_() % 70)) * power;
        p.vx = std::cos(ang) * sp;
        p.vy = std::sin(ang) * sp - 12.0f;
        p.life = 0.8f + (float)(rng_() % 90) / 100.0f;
        p.maxLife = p.life;
        p.size = 1.0f + (float)(rng_() % 22) / 10.0f;
        p.color = Color(235, 255, 140, 210);
        p.kind = 1;
        particles_.push_back(p);
    }
}

void Game::emitSplash(float x, float y) {
    for (int i = 0; i < 18; i++) {
        Particle p;
        p.x = x + (float)((int)(rng_() % 17) - 8);
        p.y = y;
        p.vx = ((float)(rng_() % 100) / 50.0f - 1.0f) * 45.0f;
        p.vy = -40.0f - (float)(rng_() % 80);
        p.life = 0.5f + (float)(rng_() % 60) / 100.0f;
        p.maxLife = p.life;
        p.size = 1.0f + (float)(rng_() % 20) / 10.0f;
        p.color = Color(130, 190, 210, 190);
        p.kind = 2;
        particles_.push_back(p);
    }
}

void Game::updateParticles(float dt) {
    for (auto& p : particles_) {
        p.life -= dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        if (p.kind == 0) { p.vy += 12.0f * dt; p.vx *= 0.97f; }
        else if (p.kind == 2) { p.vy += 260.0f * dt; }
        else if (p.kind == 1) { p.vy -= 4.0f * dt; p.vx *= 0.995f; }
        else { p.vy += 20.0f * dt; p.vx += std::sin(time_ + p.y * 0.01f) * 8.0f * dt; }
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
        [](const Particle& p) { return p.life <= 0; }), particles_.end());
}

void Game::initAmbient(int n) {
    ambient_.clear();
    std::uniform_real_distribution<float> ux(0, 640), uy(60, 340), uv(-6, 6);
    for (int i = 0; i < n; i++) {
        Ambient a;
        a.x = camX_ + ux(rng_);
        a.y = camY_ + uy(rng_);
        a.vx = uv(rng_);
        a.vy = uv(rng_) * 0.4f;
        a.phase = (float)(rng_() % 628) / 100.0f;
        ambient_.push_back(a);
    }
}

void Game::updateAmbient(float dt, int target) {
    if ((int)ambient_.size() > target) ambient_.resize(target);
    std::uniform_real_distribution<float> ux(0, 760), uy(30, 360), uv(-5, 5);
    while ((int)ambient_.size() < target) {
        Ambient a;
        a.x = camX_ + ux(rng_) - 60;
        a.y = camY_ + uy(rng_);
        a.vx = uv(rng_);
        a.vy = uv(rng_) * 0.35f;
        a.phase = (float)(rng_() % 628) / 100.0f;
        ambient_.push_back(a);
    }
    for (auto& a : ambient_) {
        a.x += (a.vx + std::sin(time_ * 0.7f + a.phase) * 4.0f) * dt;
        a.y += (a.vy + std::cos(time_ * 0.9f + a.phase * 1.3f) * 3.0f) * dt;
        if (a.x < camX_ - 100 || a.x > camX_ + 740 || a.y < camY_ - 80 || a.y > camY_ + 430) {
            a.x = camX_ + ux(rng_) - 60;
            a.y = camY_ + uy(rng_);
            a.vx = uv(rng_);
            a.vy = uv(rng_) * 0.35f;
        }
    }
}

// ---------------------------------------------------------------------------
// dibujo: escenario
// ---------------------------------------------------------------------------

void Game::drawTitleBackdrop(float drift) {
    float oldX = camX_, oldY = camY_;
    camX_ = 360.0f + drift;
    camY_ = 240.0f;
    level_.drawSky(gfx_, camX_, camY_, time_, 0);
    level_.drawFar(gfx_, camX_, camY_, time_);
    level_.drawBackDecor(gfx_, camX_, camY_, time_);
    level_.drawWater(gfx_, camX_, camY_, time_);
    level_.drawTerrain(gfx_, camX_, camY_, time_);
    level_.drawFrontDecor(gfx_, camX_, camY_, time_);

    // Nara junto a la luna
    float nx = 520.0f;
    float feet = 29 * TILE - camY_;
    drawNara(nx, feet, false, -1, 1.0f, 1.35f);
    drawFirefly(474, 142 + std::sin(time_ * 1.7f) * 5, time_, 1.0f);

    gfx_.beginLights(0.42f);
    gfx_.addLight(nx - 8, feet - 30, 116, 0.9f);
    gfx_.addLight(474, 142, 55, 0.55f);
    gfx_.addLight(512, 64, 100, 0.35f);
    gfx_.endLights();
    gfx_.glow(nx - 8, feet - 28, 48, Color(255, 190, 90, 70));
    gfx_.glow(474, 142, 22, Color(220, 255, 130, 80));
    gfx_.glow(512, 56, 82, Color(175, 200, 235, 30));
    gfx_.glow(512, 56, 34, Color(230, 235, 215, 26));
    gfx_.drawVignette();

    camX_ = oldX;
    camY_ = oldY;
}

void Game::drawWorld() {
    level_.drawSky(gfx_, camX_, camY_, time_, 0);
    level_.drawFar(gfx_, camX_, camY_, time_);
    level_.drawBackDecor(gfx_, camX_, camY_, time_);
    level_.drawWater(gfx_, camX_, camY_, time_);
    level_.drawTerrain(gfx_, camX_, camY_, time_);
    level_.drawTree(gfx_, camX_, camY_, time_, bloom_);
    level_.drawFrontDecor(gfx_, camX_, camY_, time_);

    // luciérnagas sin recoger
    for (auto& f : level_.fireflies) {
        if (f.taken) continue;
        float fx = f.x + std::sin(time_ * 1.3f + f.phase) * 7.0f;
        float fy = f.y + std::sin(time_ * 1.9f + f.phase) * 4.5f;
        drawFirefly(fx - camX_, fy - camY_, time_ + f.phase, 1.0f);
    }

    // Nara (las coordenadas del jugador son de mundo: hay que llevarlas a pantalla)
    if (state_ != GameState::Ending) {
        drawNara(playerCenterX() - camX_, playerFeet() - camY_,
                 std::abs(player_.vx) > 12, player_.face, 1.0f);
    } else {
        drawNara(playerCenterX() - camX_, playerFeet() - camY_, false, 1, 1.0f);
    }

    // luciérnagas recogidas orbitando a Nara
    if (state_ != GameState::Ending) {
        for (int i = 0; i < fireflyCount_; i++) {
            float ang = time_ * 1.35f + i * 2.1f;
            float r = 15 + i * 4;
            float px = playerCenterX() + std::cos(ang) * r - camX_;
            float py = playerFeet() - 24 + std::sin(ang * 1.4f) * (6 + i * 2) - camY_;
            drawFirefly(px, py, time_ + i, 0.8f);
        }
    }

    // partículas no luminosas
    drawParticles(false);

    drawLights();

    // partículas luminosas
    drawParticles(true);
    drawAmbient();

    // destellos de la escena final
    if (bloom_ > 0.01f) {
        float tx = 271 * TILE - camX_;
        float ty = 12 * TILE - camY_ - 32;
        gfx_.glow(tx, ty, 260 + bloom_ * 220, Color(255, 190, 90, (Uint8)(20 + bloom_ * 45)));
    }
    if (state_ == GameState::Ending) {
        // hoguera de luciérnagas ascendiendo
        for (int i = 0; i < 3; i++) {
            float t = std::max(0.0f, releasePhase_ - i * 0.3f);
            float ang = t * 1.25f + i * 2.1f;
            float r = 26 + t * 15;
            float px = playerCenterX() + std::cos(ang) * r - camX_;
            float py = playerFeet() - 34 - t * 72 + std::sin(ang * 1.3f) * 10 - camY_;
            drawFirefly(px, py, time_ + i, 1.0f);
        }
    }
    gfx_.drawVignette();
}

void Game::drawLights() {
    float darkness = 0.60f;
    if (state_ == GameState::Ending) darkness = 0.52f;
    if (state_ == GameState::Title) darkness = 0.4f;
    gfx_.beginLights(darkness);
    float lx = playerCenterX() + player_.face * 8 - camX_;
    float ly = playerFeet() - 20 - camY_;
    gfx_.addLight(lx, ly, 132, 0.96f);
    float mx = 512.0f - std::fmod(camX_ * 0.02f, 80.0f);
    float my = 64.0f - camY_ * 0.03f;
    gfx_.addLight(mx, my, 90, 0.25f);
    for (auto& f : level_.fireflies) {
        if (f.taken) continue;
        float fx = f.x + std::sin(time_ * 1.3f + f.phase) * 7.0f - camX_;
        float fy = f.y + std::sin(time_ * 1.9f + f.phase) * 4.5f - camY_;
        gfx_.addLight(fx, fy, 54, 0.55f);
    }
    for (auto& c : level_.checkpoints) {
        if (c.active) gfx_.addLight(c.tx * TILE + 9 - camX_, c.ty * TILE - 42 - camY_, 54, 0.4f);
    }
    if (playerCenterX() > 240 * TILE) {
        gfx_.addLight(271 * TILE - camX_, 12 * TILE - 40 - camY_,
                      150 + bloom_ * 280, 0.5f + bloom_ * 0.45f);
    }
    gfx_.endLights();

    gfx_.glow(mx, my, 72, Color(175, 200, 235, 26));
    gfx_.glow(lx, ly, 46, Color(255, 190, 95, 60));
    gfx_.glow(lx, ly, 18, Color(255, 230, 160, 55));
    for (auto& f : level_.fireflies) {
        if (f.taken) continue;
        float fx = f.x + std::sin(time_ * 1.3f + f.phase) * 7.0f - camX_;
        float fy = f.y + std::sin(time_ * 1.9f + f.phase) * 4.5f - camY_;
        gfx_.glow(fx, fy, 22, Color(225, 255, 130, 65));
    }
    for (auto& c : level_.checkpoints) {
        if (c.active) {
            float gx = c.tx * TILE + 9 - camX_;
            float gy = c.ty * TILE - 41 - camY_;
            gfx_.glow(gx, gy, 34, Color(255, 195, 100, 55));
        }
    }
    if (bloom_ > 0.01f) {
        gfx_.glow(271 * TILE - camX_, 12 * TILE - camY_ - 28,
                  120 + bloom_ * 180, Color(255, 180, 80, (Uint8)(40 + bloom_ * 80)));
    }
}

void Game::drawNara(float cx, float feet, bool moving, int face, float alpha, float scaleMul) {
    float bob = 0.0f;
    int frame = 9;
    if (moving) {
        frame = 9 + ((int)(player_.animT * 9.0f) & 1);
        bob = std::sin(player_.animT * 9.0f) * 1.2f;
    } else {
        frame = 9 + ((int)(time_ * 1.7f) & 1);
        bob = std::sin(time_ * 2.0f) * 0.5f;
    }
    if (!player_.onGround && state_ == GameState::Play) frame = 10;
    float s = player_.scale * scaleMul;
    if (state_ != GameState::Play) s = scaleMul;
    float sx = cx - 12 * s;
    float sy = feet - 24 * s + bob;
    gfx_.rect(cx - 9, feet - 1, 18, 2, Color(0, 0, 0, 70));
    gfx_.sprite24(frame, sx, sy, face < 0, s, Color(218, 226, 245, 255), alpha);
    // farolillo
    float lx = cx + face * 8.0f;
    float ly = feet - 19.0f + std::sin(time_ * 3.1f) * 1.2f;
    gfx_.tile(152, lx - 5, ly - 5, 0.56f, false, Color(255, 225, 165, 255), alpha);
}

void Game::drawNaraSilhouette(float cx, float feet, float scale, Color c, int face) {
    float sx = cx - 12 * scale;
    float sy = feet - 24 * scale;
    gfx_.sprite24(9, sx, sy, face < 0, scale, c, (float)c.a / 255.0f);
}

void Game::drawFirefly(float x, float y, float t, float alpha) {
    float pulse = 0.75f + 0.25f * std::sin(t * 3.3f);
    gfx_.glow(x, y, 13 + 6 * pulse, Color(210, 255, 120, (Uint8)(70 * alpha)));
    gfx_.circle(x, y, 1.8f, Color(250, 255, 210, (Uint8)(255 * alpha)));
    gfx_.rect(x - 5, y, 10, 1, Color(220, 255, 150, (Uint8)(85 * alpha)));
    gfx_.rect(x, y - 5, 1, 10, Color(220, 255, 150, (Uint8)(85 * alpha)));
    gfx_.circle(x - 3, y - 2, 1.7f, Color(210, 245, 160, (Uint8)(45 * alpha)));
    gfx_.circle(x + 3, y - 2, 1.7f, Color(210, 245, 160, (Uint8)(45 * alpha)));
}

void Game::drawAmbient() {
    for (auto& a : ambient_) {
        float x = a.x - camX_;
        float y = a.y - camY_;
        if (x < -20 || x > VIEW_W + 20 || y < -20 || y > VIEW_H + 20) continue;
        float tw = 0.45f + 0.55f * std::sin(time_ * 1.9f + a.phase);
        gfx_.glow(x, y, 3.5f + tw * 3.5f, Color(190, 230, 130, (Uint8)(22 + 32 * tw)));
        gfx_.rect(x, y, 1, 1, Color(225, 245, 170, (Uint8)(70 + 80 * tw)));
    }
}

void Game::drawParticles(bool additive) {
    for (auto& p : particles_) {
        bool glowKind = p.kind == 1;
        if (glowKind != additive) continue;
        float a = clampf(p.life / p.maxLife, 0.0f, 1.0f);
        float sx = p.x - camX_;
        float sy = p.y - camY_;
        if (sx < -30 || sx > VIEW_W + 30 || sy < -30 || sy > VIEW_H + 30) continue;
        if (glowKind) {
            gfx_.glow(sx, sy, p.size * 4.0f + 2.0f, Color(p.color.r, p.color.g, p.color.b, (Uint8)(p.color.a * a)));
            gfx_.rect(sx, sy, 1, 1, Color(255, 255, 220, (Uint8)(220 * a)));
        } else if (p.kind == 2) {
            gfx_.rect(sx, sy, p.size, p.size, Color(p.color.r, p.color.g, p.color.b, (Uint8)(p.color.a * a)));
        } else {
            gfx_.rect(sx, sy, p.size, p.size, Color(p.color.r, p.color.g, p.color.b, (Uint8)(p.color.a * a)));
        }
    }
}

// ---------------------------------------------------------------------------
// dibujo: interfaz y escenas
// ---------------------------------------------------------------------------

void Game::drawHud() {
    int n = fireflyCount_;
    gfx_.text(624, 12, "LUCIÉRNAGAS", 9, Color(150, 180, 200, 210), 2, true);
    for (int i = 0; i < 3; i++) {
        float x = 522 + i * 17;
        float y = 30;
        if (i < n) {
            gfx_.glow(x, y, 10, Color(220, 255, 120, 70));
            gfx_.circle(x, y, 3.2f, Color(235, 255, 170, 255));
        } else {
            gfx_.circle(x, y, 3.2f, Color(50, 65, 78, 220));
            gfx_.circle(x, y, 2.4f, Color(20, 28, 38, 230));
        }
    }
    gfx_.text(588, 24, std::to_string(n) + " / 3", 9, Color(190, 210, 220, 190), 0, true);

    // pista de controles al principio
    if (playTime_ < 9.0f && !dialogueActive_ && !touchMode_) {
        float a = smoothstep(0.0f, 1.0f, playTime_) * (1.0f - smoothstep(6.0f, 9.0f, playTime_));
        gfx_.text(14, 338, "A / D  MOVER      ESPACIO  SALTAR      E  INTERACTUAR", 8,
                  Color(160, 180, 195, (Uint8)(190 * a)), 0, true);
    }

    // nombre de zona
    if (zoneBannerT_ < 4.2f) {
        float a = std::min(1.0f, zoneBannerT_ / 0.5f) *
                  (1.0f - smoothstep(3.2f, 4.2f, zoneBannerT_));
        gfx_.text(20, 20, zoneText_, 14, Color(235, 220, 190, (Uint8)(235 * a)), 0, true);
        gfx_.text(20, 38, "VALLE UMBRÍO", 8, Color(140, 170, 190, (Uint8)(190 * a)), 0, true);
        gfx_.rect(20, 33, 86 * a, 1, Color(180, 160, 110, (Uint8)(120 * a)));
    }

    // aviso
    if (toastT_ < 4.0f && !toast_.empty()) {
        float a = std::min(1.0f, toastT_ / 0.25f) * (1.0f - smoothstep(3.2f, 4.0f, toastT_));
        float w = gfx_.textWidth(toast_, 10) + 26;
        gfx_.rect(320 - w / 2, 322, w, 22, Color(8, 14, 24, (Uint8)(190 * a)));
        gfx_.rectOutline(320 - w / 2, 322, w, 22, Color(170, 150, 100, (Uint8)(80 * a)), 1);
        gfx_.text(320, 327, toast_, 10, Color(235, 220, 180, (Uint8)(240 * a)), 1, true);
    }

    // prompt de liberación
    if (nearTree_ && awaitingRelease_ && !dialogueActive_) {
        float pulse = 0.65f + 0.35f * std::sin(time_ * 3.0f);
        float w = touchMode_ ? 306 : 280;
        gfx_.rect(320 - w / 2, 300, w, 30, Color(8, 12, 22, 210));
        gfx_.rectOutline(320 - w / 2, 300, w, 30, Color(220, 180, 100, (Uint8)(120 * pulse)), 1);
        gfx_.text(320, 309,
                  touchMode_ ? "BOTÓN LUCIÉRNAGA  ·  SOLTAR" : "E  ·  SOLTAR LAS LUCIÉRNAGAS",
                  10, Color(245, 225, 170, (Uint8)(180 + 60 * pulse)), 1, true);
    }
}

void Game::drawTouchButton(float cx, float cy, float r, bool held, int icon) {
    float a = held ? 1.0f : 0.78f;
    gfx_.circle(cx, cy, r, Color(5, 10, 18, (Uint8)(120 * a)));
    gfx_.circle(cx, cy, r - 2.0f, Color(17, 28, 42, (Uint8)(120 * a)));
    Color ring = held ? Color(255, 226, 160, 210) : Color(155, 185, 205, 105);
    const int segs = 30;
    for (int i = 0; i < segs; i++) {
        float a0 = (float)i / segs * 2.0f * PI;
        float a1 = (float)(i + 1) / segs * 2.0f * PI;
        gfx_.line(cx + std::cos(a0) * (r - 0.5f), cy + std::sin(a0) * (r - 0.5f),
                  cx + std::cos(a1) * (r - 0.5f), cy + std::sin(a1) * (r - 0.5f), ring);
    }
    Color fg = held ? Color(255, 238, 190, 245) : Color(205, 222, 232, 195);
    auto seg = [&](float x1, float y1, float x2, float y2) {
        gfx_.line(x1, y1, x2, y2, fg);
        gfx_.line(x1 + 0.7f, y1, x2 + 0.7f, y2, fg);
    };
    if (icon == 0) {           // izquierda
        seg(cx + 8, cy - 11, cx - 8, cy);
        seg(cx - 8, cy, cx + 8, cy + 11);
    } else if (icon == 1) {    // derecha
        seg(cx - 8, cy - 11, cx + 8, cy);
        seg(cx + 8, cy, cx - 8, cy + 11);
    } else if (icon == 2) {    // salto
        seg(cx - 11, cy + 5, cx, cy - 7);
        seg(cx, cy - 7, cx + 11, cy + 5);
        seg(cx - 6, cy + 13, cx + 6, cy + 13);
    } else if (icon == 3) {    // interactuar / luciérnaga
        gfx_.glow(cx, cy, 12, Color(225, 255, 150, 45));
        gfx_.circle(cx, cy, 3.2f, fg);
        gfx_.line(cx - 9, cy, cx - 5, cy, fg);
        gfx_.line(cx + 5, cy, cx + 9, cy, fg);
        gfx_.line(cx, cy - 9, cx, cy - 5, fg);
        gfx_.line(cx, cy + 5, cx, cy + 9, fg);
    } else {                   // pausa
        gfx_.rect(cx - 5, cy - 6, 3, 12, fg);
        gfx_.rect(cx + 2, cy - 6, 3, 12, fg);
    }
}

void Game::drawTouchControls() {
    if (!touchMode_ || state_ != GameState::Play || paused_ || dialogueActive_) return;
    drawTouchButton(54, 300, 34, tLeft_.held, 0);
    drawTouchButton(126, 300, 34, tRight_.held, 1);
    drawTouchButton(586, 300, 38, tJump_.held, 2);
    drawTouchButton(512, 260, 30, tInteract_.held, 3);
    drawTouchButton(612, 64, 18, false, 4);
}

void Game::drawDialogue() {
    if (!dialogueActive_ && dlgAnim_ < 0.01f) return;
    float t = std::max(0.0f, dlgAnim_);
    float yoff = (1.0f - t) * 46.0f;
    float x = 36, y = 248 + yoff, w = 568, h = 98;
    gfx_.rect(x, y, w, h, Color(6, 10, 20, (Uint8)(225 * t)));
    gfx_.rect(x, y, w, 1, Color(190, 160, 100, (Uint8)(150 * t)));
    gfx_.rect(x, y + h - 1, w, 1, Color(60, 90, 110, (Uint8)(120 * t)));
    gfx_.rect(x, y, 1, h, Color(110, 140, 160, (Uint8)(90 * t)));
    gfx_.rect(x + w - 1, y, 1, h, Color(110, 140, 160, (Uint8)(90 * t)));

    if (!dlg_.empty()) {
        const Line& line = dlg_[std::min((size_t)dlgIndex_, dlg_.size() - 1)];

        // retrato
        float px = 84, py = y + 48;
        gfx_.circle(px, py, 29, Color(18, 28, 44, (Uint8)(245 * t)));
        gfx_.circle(px, py, 27, Color(32, 46, 62, (Uint8)(235 * t)));
        if (line.portrait == 9 || line.portrait == 11) {
            gfx_.sprite24(line.portrait, px - 18, py - 18, false, 1.5f,
                          Color(225, 230, 245, 255), t);
        } else if (line.portrait == -1) {
            gfx_.glow(px, py, 26, Color(225, 255, 120, (Uint8)(60 * t)));
            gfx_.circle(px, py, 5, Color(250, 255, 200, (Uint8)(240 * t)));
            gfx_.rect(px - 12, py, 24, 1, Color(220, 255, 160, (Uint8)(90 * t)));
        } else {
            gfx_.tile(126, px - 18, py - 24, 2.0f, false, Color(90, 150, 130, 255), t);
        }

        gfx_.text(124, y + 10, line.speaker, 10, Color(line.color.r, line.color.g, line.color.b,
                  (Uint8)(240 * t)), 0, true, 0, FontFace::Pixel);
        gfx_.textReveal(124, y + 29, 466, line.text, 15,
                        Color(228, 235, 245, (Uint8)(248 * t)), (int)dlgVisible_, true,
                        FontFace::Body);

        float blink = 0.45f + 0.55f * std::sin(time_ * 4.0f);
        gfx_.text(x + w - 16, y + h - 18, touchMode_ ? "TOCA" : "ENTER", 8,
                  Color(170, 190, 200, (Uint8)(150 * blink * t)), 2, false);
    }
}

void Game::drawTitle() {
    drawTitleBackdrop(std::sin(time_ * 0.08f) * 12.0f);
    gfx_.rect(0, 0, 640, 360, Color(4, 8, 18, 55));
    float a = std::min(1.0f, time_ * 0.6f);
    gfx_.text(320, 64, "UN CUENTO DE CINCO MINUTOS", 8,
              Color(150, 180, 200, (Uint8)(190 * a)), 1, true);
    gfx_.glow(320, 138, 150, Color(255, 180, 80, (Uint8)(16 * a)));
    gfx_.text(320, 84, "LA ÚLTIMA", 44, Color(246, 226, 178, (Uint8)(250 * a)), 1, true);
    gfx_.text(320, 132, "LUCIÉRNAGA", 44, Color(255, 214, 130, (Uint8)(250 * a)), 1, true);
    gfx_.rect(260, 194, 120, 1, Color(190, 160, 100, (Uint8)(120 * a)));
    gfx_.text(320, 204, "sobre dejar ir lo que más queremos", 14,
              Color(192, 205, 218, (Uint8)(215 * a)), 1, true, 0, FontFace::Body);
    float pulse = 0.55f + 0.45f * std::sin(time_ * 2.6f);
    gfx_.text(320, 288, touchMode_ ? "TOCA PARA EMPEZAR" : "PULSA  ENTER", 14,
              Color(240, 220, 170, (Uint8)(170 + 70 * pulse)), 1, true, 0, FontFace::Body);
    gfx_.text(320, 332, touchMode_ ? "BOTONES EN PANTALLA  ·  MÓVIL" :
                                    "A/D mover   ·   ESPACIO saltar   ·   E interactuar", 8,
              Color(140, 165, 185, (Uint8)(190 * a)), 1, true);
    gfx_.text(12, 346, "Kenney · Pixelify Sans + Lora · SDL2", 7,
              Color(90, 110, 128, (Uint8)(150 * a)), 0, false);
    drawFades();
}

void Game::drawIntro() {
    drawTitleBackdrop(std::sin(time_ * 0.08f) * 12.0f);
    gfx_.rect(0, 0, 640, 360, Color(3, 6, 14, 165));
    static const char* pages[3] = {
        "Hace un año, Teo siguió una luz\nhasta el bosque.\nNunca volvió.",
        "Desde esa noche, las luciérnagas\ndejaron de venir a Valle Umbrío.\nLa abuela dice que se llevaron su luz.",
        "Pero esta noche, Nara ha dejado\nla ventana abierta.\n\nY algo ha entrado."
    };
    float a = clampf(introT_ * 1.6f, 0.0f, 1.0f);
    gfx_.text(320, 70, "PRÓLOGO", 9, Color(160, 185, 205, (Uint8)(190 * a)), 1, true);
    gfx_.text(320, 112, pages[std::clamp(introPage_, 0, 2)], 19,
              Color(226, 232, 242, (Uint8)(245 * a)), 1, true, 520, FontFace::Body);
    float pulse = 0.55f + 0.45f * std::sin(time_ * 2.4f);
    gfx_.text(320, 318, touchMode_ ? "TOCA PARA CONTINUAR" : "ENTER", 10,
              Color(220, 200, 155, (Uint8)((150 + 70 * pulse) * a)), 1, true);
    gfx_.text(320, 338, std::to_string(introPage_ + 1) + " / 3", 8,
              Color(130, 150, 170, (Uint8)(170 * a)), 1, false);
    drawFades();
}

void Game::drawEnding() {
    drawWorld();
    float a = smoothstep(0.4f, 1.4f, endingT_) * (1.0f - smoothstep(4.2f, 5.0f, endingT_));
    gfx_.text(320, 92, "Adiós, Teo.", 20, Color(255, 230, 180, (Uint8)(240 * a)), 1, true);
    float warm = smoothstep(2.5f, 5.0f, endingT_) * 0.35f;
    if (warm > 0) gfx_.rect(0, 0, 640, 360, Color(255, 190, 110, (Uint8)(warm * 255)));
    if (whiteFade_ > 0.001f)
        gfx_.rect(0, 0, 640, 360, Color(255, 244, 220, (Uint8)(whiteFade_ * 255)));
    (void)bloom_;
}

void Game::drawEpilogue() {
    gfx_.vGradient(0, 0, 640, 260, Color(24, 28, 62, 255), Color(120, 105, 135, 255));
    gfx_.vGradient(0, 260, 640, 100, Color(120, 105, 135, 255), Color(235, 165, 125, 255));
    // sol
    gfx_.glow(458, 244, 150, Color(255, 190, 120, 55));
    gfx_.glow(458, 244, 80, Color(255, 220, 170, 90));
    gfx_.circle(458, 252, 30, Color(255, 230, 190, 235));
    // nubes cálidas
    for (int i = 0; i < 4; i++) {
        float x = 80 + i * 150 + std::sin(time_ * 0.05f + i) * 20;
        float y = 120 + i * 18;
        gfx_.circle(x, y, 22, Color(235, 170, 140, 14));
        gfx_.circle(x + 26, y + 4, 16, Color(235, 170, 140, 10));
    }
    // colinas
    for (int layer = 0; layer < 2; layer++) {
        Color c = layer == 0 ? Color(86, 72, 92, 255) : Color(45, 40, 58, 255);
        int base = layer == 0 ? 262 : 286;
        for (int x = 0; x < 640; x += 4) {
            float h = 20 + 12 * std::sin((x + layer * 120) * 0.011f) +
                      7 * std::sin((x + 40) * 0.027f + layer);
            gfx_.rect((float)x, base - h, 4, 100, c);
        }
    }
    // suelo
    gfx_.rect(0, 300, 640, 60, Color(28, 25, 36, 255));
    for (int i = 0; i < 12; i++) {
        float x = 20 + i * 57;
        float h = 22 + (i * 7 % 19);
        gfx_.rect(x, 300 - h, 3, h, Color(25, 22, 33, 255));
    }
    // Nara de espaldas al bosque
    drawNaraSilhouette(176, 303, 1.6f, Color(32, 28, 40, 255), 1);
    // luciérnagas que suben
    for (int i = 0; i < 22; i++) {
        float t = time_ * (0.25f + 0.03f * (i % 6)) + i * 0.9f;
        float x = 120 + (float)((i * 71) % 460) + std::sin(t) * 12;
        float y = 300 - std::fmod(time_ * (8 + i % 7) + i * 23, 220);
        float al = 0.35f + 0.35f * std::sin(time_ * 2 + i);
        gfx_.glow(x, y, 6, Color(220, 255, 140, (Uint8)(70 * al)));
        gfx_.rect(x, y, 1, 1, Color(245, 255, 190, (Uint8)(180 * al)));
    }

    struct EpiLine { float t; const char* text; int size; float y; };
    EpiLine lines[] = {
        {0.6f, "Nara volvió a casa antes del amanecer.", 16, 54},
        {2.0f, "Dejó la ventana abierta.", 16, 82},
        {3.4f, "Pero esa noche, por primera vez,", 16, 110},
        {4.1f, "durmió sin esperar.", 16, 138}
    };
    for (auto& l : lines) {
        float a = clampf((epiT_ - l.t) / 1.0f, 0.0f, 1.0f);
        if (a <= 0) continue;
        gfx_.text(320, l.y, l.text, l.size,
                  Color(246, 236, 218, (Uint8)(240 * a)), 1, true, 0, FontFace::Body);
    }
    if (epiT_ > 6.3f) {
        float a = clampf((epiT_ - 6.3f) / 1.2f, 0.0f, 1.0f);
        gfx_.text(320, 232, "FIN", 30, Color(255, 235, 190, (Uint8)(245 * a)), 1, true);
        float pulse = 0.5f + 0.5f * std::sin(time_ * 2.2f);
        gfx_.text(320, 330, touchMode_ ? "TOCA  ·  CRÉDITOS" : "ENTER  ·  CRÉDITOS", 9,
                  Color(230, 210, 170, (Uint8)((120 + 80 * pulse) * a)), 1, true);
    }
    gfx_.drawVignette();
    drawFades();
}

void Game::drawCredits() {
    gfx_.vGradient(0, 0, 640, 360, Color(8, 11, 24, 255), Color(18, 24, 40, 255));
    for (int i = 0; i < 40; i++) {
        float x = (float)((i * 173) % 640);
        float y = (float)((i * 97) % 320);
        float a = 0.3f + 0.5f * std::sin(time_ * 1.3f + i);
        gfx_.rect(x, y, 1, 1, Color(190, 210, 240, (Uint8)(80 * a)));
    }
    for (int i = 0; i < 8; i++) {
        float x = 60 + (float)((i * 149) % 520) + std::sin(time_ * 0.5f + i) * 14;
        float y = 60 + (float)((i * 83) % 230);
        gfx_.glow(x, y, 8, Color(220, 255, 140, 35));
        gfx_.rect(x, y, 1, 1, Color(240, 255, 190, 150));
    }
    gfx_.text(320, 34, "LA ÚLTIMA LUCIÉRNAGA", 20, Color(248, 228, 180, 245), 1, true);
    gfx_.text(320, 66, "un cuento sobre dejar ir", 12, Color(160, 185, 205, 215),
              1, true, 0, FontFace::Body);
    gfx_.rect(250, 90, 140, 1, Color(160, 140, 95, 130));

    gfx_.text(320, 106, "Recursos: Kenney — Pixel Platformer (CC0)", 10,
              Color(190, 205, 220, 225), 1, true, 0, FontFace::Body);
    gfx_.text(320, 126, "Fuentes: Pixelify Sans + Lora (SIL Open Font License)", 10,
              Color(190, 205, 220, 225), 1, true, 0, FontFace::Body);
    gfx_.text(320, 146, "Motor: C++17 + SDL2", 10,
              Color(190, 205, 220, 225), 1, true, 0, FontFace::Body);
    gfx_.text(320, 166, "Música y sonido: síntesis procedural en tiempo real", 10,
              Color(190, 205, 220, 225), 1, true, 0, FontFace::Body);
    gfx_.text(320, 194, "Gracias por jugar.", 17, Color(240, 220, 180, 240),
              1, true, 0, FontFace::Body);
    gfx_.text(320, 220, "Las luciérnagas no se apagan: solo cambian de sitio.", 11,
              Color(176, 199, 214, 210), 1, true, 0, FontFace::Body);

    float pulse = 0.5f + 0.5f * std::sin(time_ * 2.0f);
    if (touchMode_) {
        gfx_.rect(200, 278, 240, 28, Color(16, 26, 40, 210));
        gfx_.rectOutline(200, 278, 240, 28, Color(220, 190, 120, 150), 1);
        gfx_.text(320, 285, "VOLVER A JUGAR", 11,
                  Color(240, 220, 175, (Uint8)(180 + 55 * pulse)), 1, true);
        gfx_.rect(200, 312, 240, 26, Color(14, 22, 34, 200));
        gfx_.rectOutline(200, 312, 240, 26, Color(90, 115, 135, 110), 1);
        gfx_.text(320, 318, "TÍTULO", 10, Color(190, 208, 220, 220), 1, true);
    } else {
        gfx_.text(320, 292, "R  ·  VOLVER A JUGAR", 11,
                  Color(235, 215, 170, (Uint8)(150 + 80 * pulse)), 1, true);
        gfx_.text(320, 316, "T  ·  TÍTULO          ESC  ·  SALIR", 9,
                  Color(150, 170, 190, 200), 1, true);
    }
    gfx_.text(320, 342, "Valle Umbrío, una noche de hace un año.", 10,
              Color(112, 132, 152, 170), 1, false, 0, FontFace::Body);
    gfx_.drawVignette();
}

void Game::drawFades() {
    if (fade_ > 0.001f)
        gfx_.rect(0, 0, 640, 360, Color(2, 4, 10, (Uint8)(fade_ * 255)));
    if (flash_ > 0.001f)
        gfx_.rect(0, 0, 640, 360, Color(255, 205, 120, (Uint8)(flash_ * 60)));
}

void Game::drawPause() {
    gfx_.rect(0, 0, 640, 360, Color(2, 5, 12, 190));
    if (touchMode_) {
        float x = 154, y = 70, w = 332, h = 232;
        gfx_.rect(x, y, w, h, Color(8, 13, 24, 240));
        gfx_.rectOutline(x, y, w, h, Color(150, 160, 150, 90), 1);
        gfx_.text(320, y + 20, "PAUSA", 20, Color(240, 225, 190, 245), 1, true);
        gfx_.text(320, y + 54, "LUCIÉRNAGAS  " + std::to_string(fireflyCount_) + " / 3", 10,
                  Color(180, 200, 215, 220), 1, true);
        auto button = [&](float by, const char* label, bool accent) {
            gfx_.rect(204, by, 232, 28, Color(16, 26, 40, 220));
            gfx_.rectOutline(204, by, 232, 28,
                             accent ? Color(220, 190, 120, 150) : Color(90, 115, 135, 110), 1);
            gfx_.text(320, by + 7, label, 11,
                      accent ? Color(245, 225, 175, 240) : Color(195, 210, 222, 220), 1, true);
        };
        button(154, "CONTINUAR", true);
        button(188, "ÚLTIMO FAROL", false);
        button(222, "VOLVER AL TÍTULO", false);
        button(256, "SALIR", false);
        gfx_.text(320, 291, "«Las luciérnagas no se apagan: solo cambian de sitio.»", 8,
                  Color(150, 170, 190, 170), 1, false);
        return;
    }
    float x = 154, y = 82, w = 332, h = 196;
    gfx_.rect(x, y, w, h, Color(8, 13, 24, 240));
    gfx_.rectOutline(x, y, w, h, Color(150, 160, 150, 90), 1);
    gfx_.text(320, y + 24, "PAUSA", 22, Color(240, 225, 190, 245), 1, true);
    gfx_.text(320, y + 62, "LUCIÉRNAGAS  " + std::to_string(fireflyCount_) + " / 3", 10,
              Color(180, 200, 215, 220), 1, true);
    gfx_.text(320, y + 92, "Esc  ·  continuar", 10, Color(200, 215, 225, 220), 1, true);
    gfx_.text(320, y + 112, "R  ·  volver al último farol", 10, Color(200, 215, 225, 220), 1, true);
    gfx_.text(320, y + 132, "T  ·  volver al título", 10, Color(200, 215, 225, 220), 1, true);
    gfx_.text(320, y + 152, "Q  ·  salir", 10, Color(200, 215, 225, 220), 1, true);
    gfx_.text(320, y + 180, "«Las luciérnagas no se apagan: solo cambian de sitio.»", 8,
              Color(150, 170, 190, 170), 1, false);
}

void Game::render() {
    SDL_SetRenderDrawColor(ren_, 0, 0, 0, 255);
    SDL_RenderClear(ren_);
    switch (state_) {
        case GameState::Title:
            drawTitle();
            break;
        case GameState::Intro:
            drawIntro();
            break;
        case GameState::Play:
            drawWorld();
            drawHud();
            drawTouchControls();
            drawDialogue();
            drawFades();
            if (paused_) drawPause();
            break;
        case GameState::Ending:
            drawEnding();
            break;
        case GameState::Epilogue:
            drawEpilogue();
            break;
        case GameState::Credits:
            drawCredits();
            break;
    }
    if (screenshotQueued_) {
        screenshotQueued_ = false;
#if !LUCIERNAGA_ANDROID
        std::filesystem::create_directories("captures");
#endif
        if (!shotDir_.empty() && shotDir_.back() != '/') shotDir_ += '/';
        char name[512];
        std::snprintf(name, sizeof(name), "%sluciernaga_%02d.png", shotDir_.c_str(), ++shotN_);
        if (gfx_.saveScreenshot(name)) showToast("Captura guardada");
    }
}

// ---------------------------------------------------------------------------
// bucle
// ---------------------------------------------------------------------------

void Game::run() {
    if (autoplay_) { runAutoplay(); return; }
    Uint64 last = SDL_GetPerformanceCounter();
    while (running_) {
        Uint64 frameStart = SDL_GetTicks64();
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)(now - last) / (float)SDL_GetPerformanceFrequency();
        last = now;
        dt = clampf(dt, 0.0001f, 0.05f);
        time_ += dt;
        pollEvents();
        update(dt);
        render();
        present();
        if (!vsync_) {
            Uint64 elapsed = SDL_GetTicks64() - frameStart;
            if (elapsed < 16) SDL_Delay((Uint32)(16 - elapsed));
        }
    }
}

void Game::runAutoplay() {
    const float dt = 1.0f / 60.0f;
    const int maxFrames = 60 * 360;
    int frames = 0;
    bool finished = false;
    while (running_ && frames < maxFrames) {
        autoplayStep(dt);
        time_ += dt;
        update(dt);
        if (state_ == GameState::Epilogue && epiT_ > 7.0f) {
            state_ = GameState::Credits;
            creditsT_ = 0;
        }
        if (state_ == GameState::Credits) { finished = true; break; }
        frames++;
    }
    std::printf("AUTOPLAY: %s · tiempo %.1f s · luciérnagas %d/3 · x %.0f · muertes %d\n",
                finished ? "OK" : "BLOQUEADO", playTime_, fireflyCount_,
                playerCenterX(), botDeaths_);
    // En Android stdout no llega a logcat; duplicamos el resultado con SDL_Log.
    SDL_Log("AUTOPLAY: %s · tiempo %.1f s · luciérnagas %d/3 · x %.0f · muertes %d",
            finished ? "OK" : "BLOQUEADO", playTime_, fireflyCount_,
            playerCenterX(), botDeaths_);
    running_ = false;
}

void Game::autoplayStep(float dt) {
    if (state_ == GameState::Title) {
        state_ = GameState::Intro;
        introPage_ = 0;
        introT_ = 0;
        return;
    }
    if (state_ == GameState::Intro) {
        if (introT_ > 0.25f) {
            introPage_++;
            introT_ = 0;
            if (introPage_ >= 3) startGame();
        }
        return;
    }
    if (state_ != GameState::Play) return;

    // Avanza los diálogos automáticamente para poder recorrer el nivel.
    while (dialogueActive_) advanceDialogue();

    if (awaitingRelease_ && playerCenterX() > 262.0f * TILE) {
        interactQueued_ = true;
        return;
    }

    if (playerCenterX() > botMaxX_ + 1.0f) {
        botMaxX_ = playerCenterX();
        botIdleTime_ = 0;
    } else if (!dying_) {
        botIdleTime_ += dt;
    }

    // Si se queda atascada, reintenta saltando.
    if (botIdleTime_ > 2.5f) {
        player_.jumpBuf = 0.14f;
        botJumpHold_ = 0.30f;
        botIdleTime_ = 0;
    }

    autoplayInput();
}

void Game::autoplayInput() {
    inLeft_ = inRight_ = false;
    if (dying_ || dialogueActive_) { jumpHeld_ = false; return; }

    float feet = playerFeet();
    float front = player_.x + 10.0f;
    float cx = playerCenterX();

    // Mantén una velocidad estable: los saltos tienen un alcance predecible.
    const float desiredVx = 130.0f;
    if (player_.vx <= desiredVx) inRight_ = true;
    else inLeft_ = true;

    // Al final, detente junto al árbol.
    if (cx > 267.0f * TILE) {
        inRight_ = false;
        if (player_.vx > 4.0f) inLeft_ = true;
    }

    bool wantJump = false;
    if (player_.onGround) {
        int ty = (int)std::floor((feet + 2.0f) / TILE);
        int tx = (int)std::floor((front + 4.0f) / TILE);
        bool gap = true;
        for (int k = 0; k < 2; k++)
            if (level_.solidAt(tx + k, ty)) { gap = false; break; }
        bool wall = level_.hits(front + 2.0f, player_.y, 2.0f, 18.0f);
        wantJump = gap || wall;

        // Las luciérnagas están sobre el camino: salta para alcanzarlas.
        for (auto& f : level_.fireflies) {
            if (f.taken) continue;
            if (std::abs(f.x - cx) < 42.0f && f.y < feet - 12.0f && f.y > feet - 96.0f)
                wantJump = true;
        }
        if (wantJump) {
            player_.jumpBuf = 0.14f;
            botJumpHold_ = 0.32f;
        }
    }

    if (botJumpHold_ > 0) {
        botJumpHold_ -= 1.0f / 60.0f;
        jumpHeld_ = true;
    } else {
        jumpHeld_ = false;
    }
}

// ---------------------------------------------------------------------------
// captura de pantallas para revisión
// ---------------------------------------------------------------------------

void Game::capture(const std::string& dir) {
    std::filesystem::create_directories(dir);
    fade_ = 0;
    time_ = 2.4f;
    state_ = GameState::Title;
    camX_ = 360; camY_ = 324;
    initAmbient(28);
    updateAmbient(0.1f, 28);
    render();
    gfx_.saveScreenshot(dir + "/01_title.png");

    state_ = GameState::Intro;
    introPage_ = 0;
    introT_ = 2.0f;
    fade_ = 0;
    render();
    gfx_.saveScreenshot(dir + "/02_intro.png");

    startGame();
    fade_ = 0;
    playTime_ = 5.0f;
    zoneBannerT_ = 1.2f;
    time_ = 3.2f;
    updateAmbient(0.1f, 10);
    render();
    gfx_.saveScreenshot(dir + "/03_sendero.png");

    // plataforma de la primera luciérnaga, con diálogo de recuerdo
    player_.x = 37.5f * TILE;
    player_.y = 27 * TILE - 18;
    player_.onGround = true;
    camX_ = clampf(playerCenterX() - 320, 0.0f, level_.w * TILE - VIEW_W);
    camY_ = clampf(player_.y + 9 - 188, 0.0f, level_.h * TILE - VIEW_H);
    level_.fireflies[0].taken = true;
    fireflyCount_ = 1;
    startDialogue({
        L("NARA", "Teo... aquí saltábamos los charcos contando hasta tres."),
        L("TEO", "Uno... dos... ¡tres! No mires abajo, Nara."),
        L("NARA", "Nunca miré. Hasta la noche en que te fuiste.")
    });
    dlgVisible_ = 9999;
    dlgAnim_ = 1;
    time_ = 4.0f;
    render();
    gfx_.saveScreenshot(dir + "/04_dialogo.png");

    dialogueActive_ = false;
    dlg_.clear();
    dlgAnim_ = 0;
    player_.x = 117.5f * TILE;
    player_.y = 26 * TILE - 18;
    player_.onGround = true;
    camX_ = clampf(playerCenterX() - 320, 0.0f, level_.w * TILE - VIEW_W);
    camY_ = clampf(player_.y + 9 - 188, 0.0f, level_.h * TILE - VIEW_H);
    initAmbient(18);
    time_ = 6.0f;
    render();
    gfx_.saveScreenshot(dir + "/05_arroyo.png");

    // final junto al árbol
    for (auto& f : level_.fireflies) f.taken = true;
    for (auto& c : level_.checkpoints) c.active = true;
    fireflyCount_ = 3;
    awaitingRelease_ = true;
    nearTree_ = true;
    player_.x = 265.0f * TILE;
    player_.y = 12 * TILE - 18;
    player_.face = 1;
    player_.onGround = true;
    camX_ = 4400;
    camY_ = 24;
    bloom_ = 0.25f;
    initAmbient(32);
    time_ = 8.0f;
    render();
    gfx_.saveScreenshot(dir + "/06_arbol.png");

    state_ = GameState::Ending;
    endingT_ = 3.0f;
    releasePhase_ = 2.5f;
    bloom_ = 0.78f;
    initAmbient(40);
    render();
    gfx_.saveScreenshot(dir + "/07_final.png");

    state_ = GameState::Epilogue;
    epiT_ = 8.5f;
    time_ = 9.5f;
    render();
    gfx_.saveScreenshot(dir + "/08_epilogo.png");

    state_ = GameState::Credits;
    time_ = 10.0f;
    render();
    gfx_.saveScreenshot(dir + "/09_creditos.png");

    std::printf("Capturas en %s\n", dir.c_str());
    running_ = false;
}

// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    bool captureMode = false;
    bool autoplay = false;
    bool forceTouch = false;
    bool startFullscreen = false;
    std::string captureDir = "captures";
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--capture") {
            captureMode = true;
            if (i + 1 < argc) captureDir = argv[++i];
        } else if (a == "--autoplay") {
            autoplay = true;
        } else if (a == "--touch") {
            forceTouch = true;
        } else if (a == "--fullscreen") {
            startFullscreen = true;
        } else if (a == "--help" || a == "-h") {
            std::printf("La última luciérnaga\n"
                        "  ./luciernaga                 jugar\n"
                        "  ./luciernaga --fullscreen    jugar a pantalla completa\n"
                        "  ./luciernaga --capture DIR   generar capturas de las escenas\n"
                        "  ./luciernaga --autoplay      recorrido automático de prueba\n"
                        "  ./luciernaga --touch         mostrar controles táctiles (pruebas)\n"
                        "Controles: A/D o flechas · ESPACIO saltar · E interactuar · Esc pausa · F11 pantalla completa · F12 captura\n");
            return 0;
        }
    }

#if LUCIERNAGA_ANDROID
    std::string assets = "assets"; // dentro del APK, vía SDL_RWops
#else
    std::string base = SDL_GetBasePath() ? SDL_GetBasePath() : "";
    std::string assets = base + "assets";
    if (!std::filesystem::exists(assets + "/tiles.png")) assets = "assets";
#endif

    Game game;
    if (!game.init(captureMode || autoplay, assets)) {
        std::fprintf(stderr, "No se pudo iniciar el juego.\n");
        return 1;
    }
    if (autoplay) game.setAutoplay();
    if (forceTouch) game.setTouchMode();
    if (startFullscreen && !captureMode && !autoplay) game.setFullscreen();
    if (captureMode) game.capture(captureDir);
    else game.run();
    game.shutdown();
    return 0;
}
