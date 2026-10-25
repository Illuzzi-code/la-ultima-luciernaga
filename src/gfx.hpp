// gfx.hpp - pequeña capa de dibujo sobre SDL2
#pragma once

#include <SDL.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct Color {
    Uint8 r = 255, g = 255, b = 255, a = 255;
    Color() = default;
    Color(Uint8 r_, Uint8 g_, Uint8 b_, Uint8 a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
};

// El juego se compone en un lienzo lógico de 640x360, pero se rasteriza a
// 1280x720. Así los sprites conservan su aspecto pixel-art y el texto se ve
// nítido en HD en lugar de ampliarse desde una textura diminuta.
constexpr int RENDER_SCALE = 2;
constexpr int OUT_W = 640 * RENDER_SCALE;
constexpr int OUT_H = 360 * RENDER_SCALE;

enum class FontFace { Pixel, Body };

class Gfx {
public:
    bool init(SDL_Renderer* ren, const std::string& assetDir);
    void shutdown();

    SDL_Renderer* renderer() const { return ren_; }

    // Hojas de sprites de Kenney (CC0)
    SDL_Texture* tiles() const { return tiles_; }
    SDL_Texture* chars() const { return chars_; }
    SDL_Texture* bgSheet() const { return bg_; }

    static SDL_Rect tileSrc(int index);   // 18x18, 20 columnas
    static SDL_Rect charSrc(int index);   // 24x24, 9 columnas
    static SDL_Rect bgSrc(int index);     // 18x18, 11 columnas

    void tile(int index, float x, float y, float scale = 1.0f,
              bool flipX = false, Color tint = Color(), float alpha = 1.0f);
    void sprite24(int index, float x, float y, bool flipX = false,
                  float scale = 1.0f, Color tint = Color(), float alpha = 1.0f,
                  double angle = 0.0);
    void bgTile(int index, float x, float y, float scale = 1.0f,
                bool flipX = false, Color tint = Color(), float alpha = 1.0f);

    void rect(float x, float y, float w, float h, Color c);
    void rectOutline(float x, float y, float w, float h, Color c, float t = 1.0f);
    void line(float x1, float y1, float x2, float y2, Color c);
    void circle(float cx, float cy, float radius, Color c);
    void glow(float cx, float cy, float radius, Color c);          // aditivo
    void vGradient(float x, float y, float w, float h, Color top, Color bottom);

    // Texto (TTF -> textura, con acentos UTF-8)
    int lineHeight(int size) const { return (int)(size * 1.45f); }
    float textWidth(const std::string& text, int size, FontFace face = FontFace::Pixel);
    void text(float x, float y, const std::string& text, int size, Color c,
              int align = 0, bool shadow = true, int wrapWidth = 0,
              FontFace face = FontFace::Pixel);
    void textReveal(float x, float y, int wrapWidth, const std::string& text,
                    int size, Color c, int visibleChars, bool shadow = true,
                    FontFace face = FontFace::Pixel);
    std::vector<std::string> wrapLines(const std::string& text, int size, int maxWidth);

    // Máscara de oscuridad con luces (media resolución)
    void beginLights(float darkness);
    void addLight(float cx, float cy, float radius, float strength = 1.0f);
    void endLights();
    void drawVignette();

    bool saveScreenshot(const std::string& path);

private:
    struct CachedText { SDL_Texture* tex = nullptr; int w = 0, h = 0; };
    struct GlyphRun { int codepoint, glyph; float x; };

    bool loadPng(const std::string& path, SDL_Texture** out, int* w = nullptr, int* h = nullptr);
    void* loadFont(const std::string& path, std::vector<unsigned char>& data);
    void* fontInfo(FontFace face) const;
    SDL_Texture* makeTextTexture(const std::string& text, int size, int wrapWidth,
                                 int* outW, int* outH, FontFace face);
    std::vector<std::string> splitLines(const std::string& text) const;
    std::vector<int> utf8Codepoints(const std::string& s) const;
    void drawTextTexture(SDL_Texture* tex, float x, float y, int w, int h, Color c, bool shadow);
    const CachedText* getText(const std::string& text, int size, int wrapWidth, FontFace face);
    void clearTextCache();

    SDL_Renderer* ren_ = nullptr;
    SDL_Texture* tiles_ = nullptr;
    SDL_Texture* chars_ = nullptr;
    SDL_Texture* bg_ = nullptr;
    SDL_Texture* circleTex_ = nullptr;
    SDL_Texture* glowTex_ = nullptr;
    SDL_Texture* vignette_ = nullptr;

    std::vector<unsigned char> fontData_;
    std::vector<unsigned char> bodyFontData_;
    void* font_ = nullptr;      // stbtt_fontinfo* (Pixelify Sans)
    void* bodyFont_ = nullptr;  // stbtt_fontinfo* (Lora, texto narrativo)
    std::string assetDir_;
    std::unordered_map<std::string, CachedText> textCache_;

    // luz
    SDL_Texture* lightTex_ = nullptr;
    std::vector<uint8_t> lightPixels_; // RGBA, 320x180
    int lightW_ = 320, lightH_ = 180;
    float lightDarkness_ = 0.7f;
};
