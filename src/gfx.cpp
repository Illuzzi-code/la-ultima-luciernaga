// gfx.cpp - implementación de dibujo, texto y luces
#include "gfx.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

static const float PI = 3.14159265358979f;
static constexpr float S = (float)RENDER_SCALE;

static bool readFile(const std::string& path, std::vector<unsigned char>& out) {
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) return false;
    Sint64 n = SDL_RWsize(rw);
    if (n <= 0) { SDL_RWclose(rw); return false; }
    out.resize((size_t)n);
    size_t got = SDL_RWread(rw, out.data(), 1, (size_t)n);
    SDL_RWclose(rw);
    if (got != (size_t)n) { out.clear(); return false; }
    return true;
}

bool Gfx::init(SDL_Renderer* ren, const std::string& assetDir) {
    ren_ = ren;
    assetDir_ = assetDir;
    if (!loadPng(assetDir + "/tiles.png", &tiles_)) return false;
    if (!loadPng(assetDir + "/chars.png", &chars_)) return false;
    if (!loadPng(assetDir + "/bg.png", &bg_)) return false;

    // fuentes: Pixelify Sans para títulos/HUD y Lora para el relato
    font_ = loadFont(assetDir + "/PixelifySans.ttf", fontData_);
    if (!font_) return false;
    bodyFont_ = loadFont(assetDir + "/Lora.ttf", bodyFontData_);
    if (!bodyFont_) return false;

    // textura circular suave
    {
        const int S = 128;
        std::vector<uint8_t> px(S * S * 4, 0);
        for (int y = 0; y < S; y++) {
            for (int x = 0; x < S; x++) {
                float dx = (x + 0.5f) / S * 2.0f - 1.0f;
                float dy = (y + 0.5f) / S * 2.0f - 1.0f;
                float d = std::sqrt(dx * dx + dy * dy);
                float a = std::clamp((1.0f - d) * S * 0.5f, 0.0f, 1.0f);
                size_t i = ((size_t)y * S + x) * 4;
                px[i + 0] = 255; px[i + 1] = 255; px[i + 2] = 255;
                px[i + 3] = (uint8_t)(a * 255);
            }
        }
        circleTex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STATIC, S, S);
        SDL_UpdateTexture(circleTex_, nullptr, px.data(), S * 4);
        SDL_SetTextureBlendMode(circleTex_, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(circleTex_, SDL_ScaleModeNearest);
    }
    // halo radial aditivo
    {
        const int S = 160;
        std::vector<uint8_t> px(S * S * 4, 0);
        for (int y = 0; y < S; y++) {
            for (int x = 0; x < S; x++) {
                float dx = (x + 0.5f) / S * 2.0f - 1.0f;
                float dy = (y + 0.5f) / S * 2.0f - 1.0f;
                float d = std::clamp(std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
                float a = std::pow(1.0f - d, 2.4f);
                size_t i = ((size_t)y * S + x) * 4;
                px[i + 0] = 255; px[i + 1] = 255; px[i + 2] = 255;
                px[i + 3] = (uint8_t)(a * 255);
            }
        }
        glowTex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA32,
                                     SDL_TEXTUREACCESS_STATIC, S, S);
        SDL_UpdateTexture(glowTex_, nullptr, px.data(), S * 4);
        SDL_SetTextureBlendMode(glowTex_, SDL_BLENDMODE_ADD);
        SDL_SetTextureScaleMode(glowTex_, SDL_ScaleModeLinear);
    }
    // viñeta
    {
        const int W = 320, H = 180;
        std::vector<uint8_t> px(W * H * 4, 0);
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                float nx = (x + 0.5f) / W * 2.0f - 1.0f;
                float ny = (y + 0.5f) / H * 2.0f - 1.0f;
                float d = std::sqrt(nx * nx * 0.9f + ny * ny);
                float a = std::clamp((d - 0.62f) * 1.6f, 0.0f, 1.0f);
                a = a * a * 150.0f;
                size_t i = ((size_t)y * W + x) * 4;
                px[i + 0] = 2; px[i + 1] = 4; px[i + 2] = 12;
                px[i + 3] = (uint8_t)a;
            }
        }
        vignette_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA32,
                                      SDL_TEXTUREACCESS_STATIC, W, H);
        SDL_UpdateTexture(vignette_, nullptr, px.data(), W * 4);
        SDL_SetTextureBlendMode(vignette_, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(vignette_, SDL_ScaleModeLinear);
    }

    lightPixels_.assign(lightW_ * lightH_ * 4, 0);
    lightTex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA32,
                                  SDL_TEXTUREACCESS_STREAMING, lightW_, lightH_);
    SDL_SetTextureBlendMode(lightTex_, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(lightTex_, SDL_ScaleModeLinear);

    return true;
}

void Gfx::shutdown() {
    clearTextCache();
    if (tiles_) SDL_DestroyTexture(tiles_);
    if (chars_) SDL_DestroyTexture(chars_);
    if (bg_) SDL_DestroyTexture(bg_);
    if (circleTex_) SDL_DestroyTexture(circleTex_);
    if (glowTex_) SDL_DestroyTexture(glowTex_);
    if (vignette_) SDL_DestroyTexture(vignette_);
    if (lightTex_) SDL_DestroyTexture(lightTex_);
    delete (stbtt_fontinfo*)font_;
    delete (stbtt_fontinfo*)bodyFont_;
    font_ = bodyFont_ = nullptr;
    tiles_ = chars_ = bg_ = circleTex_ = glowTex_ = vignette_ = lightTex_ = nullptr;
}

void* Gfx::loadFont(const std::string& path, std::vector<unsigned char>& data) {
    if (!readFile(path, data)) {
        SDL_Log("No se pudo leer la fuente %s", path.c_str());
        return nullptr;
    }
    stbtt_fontinfo* info = new stbtt_fontinfo();
    int offset = stbtt_GetFontOffsetForIndex(data.data(), 0);
    if (!stbtt_InitFont(info, data.data(), offset)) {
        SDL_Log("Fuente inválida: %s", path.c_str());
        delete info;
        return nullptr;
    }
    return info;
}

void* Gfx::fontInfo(FontFace face) const {
    return face == FontFace::Body ? bodyFont_ : font_;
}

bool Gfx::loadPng(const std::string& path, SDL_Texture** out, int* w, int* h) {
    int iw = 0, ih = 0, n = 0;
    // SDL_RWops permite leer tanto archivos normales como assets dentro del APK.
    std::vector<unsigned char> file;
    if (!readFile(path, file)) {
        SDL_Log("No se pudo leer %s", path.c_str());
        return false;
    }
    unsigned char* data = stbi_load_from_memory(file.data(), (int)file.size(),
                                                &iw, &ih, &n, 4);
    if (!data) {
        SDL_Log("No se pudo cargar %s: %s", path.c_str(), stbi_failure_reason());
        return false;
    }
    SDL_Texture* tex = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STATIC, iw, ih);
    SDL_UpdateTexture(tex, nullptr, data, iw * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
    stbi_image_free(data);
    *out = tex;
    if (w) *w = iw;
    if (h) *h = ih;
    return true;
}

SDL_Rect Gfx::tileSrc(int index) {
    index = std::clamp(index, 0, 179);
    return SDL_Rect{(index % 20) * 19, (index / 20) * 19, 18, 18};
}
SDL_Rect Gfx::charSrc(int index) {
    index = std::clamp(index, 0, 26);
    return SDL_Rect{(index % 9) * 25, (index / 9) * 25, 24, 24};
}
SDL_Rect Gfx::bgSrc(int index) {
    index = std::clamp(index, 0, 43);
    return SDL_Rect{(index % 11) * 19, (index / 11) * 19, 18, 18};
}

void Gfx::tile(int index, float x, float y, float scale, bool flipX, Color tint, float alpha) {
    SDL_Rect src = tileSrc(index);
    float w = 18.0f * scale;
    SDL_FRect dst{x * S, y * S, w * S, w * S};
    SDL_SetTextureColorMod(tiles_, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(tiles_, (Uint8)(std::clamp(alpha, 0.0f, 1.0f) * tint.a));
    SDL_RenderCopyExF(ren_, tiles_, &src, &dst, 0.0, nullptr,
                      flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(tiles_, 255, 255, 255);
    SDL_SetTextureAlphaMod(tiles_, 255);
}

void Gfx::sprite24(int index, float x, float y, bool flipX, float scale,
                   Color tint, float alpha, double angle) {
    SDL_Rect src = charSrc(index);
    float w = 24.0f * scale;
    SDL_FRect dst{x * S, y * S, w * S, w * S};
    SDL_SetTextureColorMod(chars_, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(chars_, (Uint8)(std::clamp(alpha, 0.0f, 1.0f) * tint.a));
    SDL_RenderCopyExF(ren_, chars_, &src, &dst, angle, nullptr,
                      flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(chars_, 255, 255, 255);
    SDL_SetTextureAlphaMod(chars_, 255);
}

void Gfx::bgTile(int index, float x, float y, float scale, bool flipX, Color tint, float alpha) {
    SDL_Rect src = bgSrc(index);
    float w = 18.0f * scale;
    SDL_FRect dst{x * S, y * S, w * S, w * S};
    SDL_SetTextureColorMod(bg_, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(bg_, (Uint8)(std::clamp(alpha, 0.0f, 1.0f) * tint.a));
    SDL_RenderCopyExF(ren_, bg_, &src, &dst, 0.0, nullptr,
                      flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(bg_, 255, 255, 255);
    SDL_SetTextureAlphaMod(bg_, 255);
}

void Gfx::rect(float x, float y, float w, float h, Color c) {
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, c.a);
    SDL_FRect r{x * S, y * S, w * S, h * S};
    SDL_RenderFillRectF(ren_, &r);
}

void Gfx::rectOutline(float x, float y, float w, float h, Color c, float t) {
    rect(x, y, w, t, c);
    rect(x, y + h - t, w, t, c);
    rect(x, y, t, h, c);
    rect(x + w - t, y, t, h, c);
}

void Gfx::line(float x1, float y1, float x2, float y2, Color c) {
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLineF(ren_, x1 * S, y1 * S, x2 * S, y2 * S);
}

void Gfx::circle(float cx, float cy, float radius, Color c) {
    float r = radius * S;
    SDL_FRect dst{cx * S - r, cy * S - r, r * 2, r * 2};
    SDL_SetTextureColorMod(circleTex_, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(circleTex_, c.a);
    SDL_RenderCopyF(ren_, circleTex_, nullptr, &dst);
    SDL_SetTextureColorMod(circleTex_, 255, 255, 255);
    SDL_SetTextureAlphaMod(circleTex_, 255);
}

void Gfx::glow(float cx, float cy, float radius, Color c) {
    float r = radius * S;
    SDL_FRect dst{cx * S - r, cy * S - r, r * 2, r * 2};
    SDL_SetTextureColorMod(glowTex_, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(glowTex_, c.a);
    SDL_RenderCopyF(ren_, glowTex_, nullptr, &dst);
    SDL_SetTextureColorMod(glowTex_, 255, 255, 255);
    SDL_SetTextureAlphaMod(glowTex_, 255);
}

void Gfx::vGradient(float x, float y, float w, float h, Color top, Color bottom) {
    SDL_Vertex v[4];
    v[0].position = {x * S, y * S};       v[0].color = {top.r, top.g, top.b, top.a};
    v[1].position = {(x + w) * S, y * S}; v[1].color = {top.r, top.g, top.b, top.a};
    v[2].position = {x * S, (y + h) * S}; v[2].color = {bottom.r, bottom.g, bottom.b, bottom.a};
    v[3].position = {(x + w) * S, (y + h) * S}; v[3].color = {bottom.r, bottom.g, bottom.b, bottom.a};
    v[0].tex_coord = {0, 0}; v[1].tex_coord = {0, 0};
    v[2].tex_coord = {0, 0}; v[3].tex_coord = {0, 0};
    int idx[6] = {0, 1, 2, 2, 1, 3};
    SDL_RenderGeometry(ren_, nullptr, v, 4, idx, 6);
}

// ---------------- texto ----------------

std::vector<int> Gfx::utf8Codepoints(const std::string& s) const {
    std::vector<int> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        int cp = 0, len = 1;
        if (c < 0x80) { cp = c; }
        else if ((c >> 5) == 0x6) { cp = c & 0x1F; len = 2; }
        else if ((c >> 4) == 0xE) { cp = c & 0x0F; len = 3; }
        else if ((c >> 3) == 0x1E) { cp = c & 0x07; len = 4; }
        else { cp = '?'; }
        for (int k = 1; k < len && i + k < s.size(); k++)
            cp = (cp << 6) | (s[i + k] & 0x3F);
        out.push_back(cp);
        i += len;
    }
    return out;
}

std::vector<std::string> Gfx::splitLines(const std::string& text) const {
    std::vector<std::string> lines;
    std::string cur;
    for (char c : text) {
        if (c == '\n') { lines.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    lines.push_back(cur);
    return lines;
}

float Gfx::textWidth(const std::string& text, int size, FontFace face) {
    stbtt_fontinfo* info = (stbtt_fontinfo*)fontInfo(face);
    if (!info || text.empty()) return 0;
    float scale = stbtt_ScaleForPixelHeight(info, (float)(size * RENDER_SCALE));
    float best = 0;
    for (const std::string& line : splitLines(text)) {
        auto cps = utf8Codepoints(line);
        float w = 0;
        int prev = 0;
        for (int cp : cps) {
            int adv = 0, lsb = 0;
            stbtt_GetCodepointHMetrics(info, cp, &adv, &lsb);
            if (prev) w += stbtt_GetCodepointKernAdvance(info, prev, cp) * scale;
            w += adv * scale;
            prev = cp;
        }
        best = std::max(best, w);
    }
    return best / (float)RENDER_SCALE;
}

std::vector<std::string> Gfx::wrapLines(const std::string& text, int size, int maxWidth) {
    std::vector<std::string> result;
    for (const std::string& para : splitLines(text)) {
        if (para.empty()) { result.push_back(""); continue; }
        auto cps = utf8Codepoints(para);
        stbtt_fontinfo* info = (stbtt_fontinfo*)font_;
        float scale = stbtt_ScaleForPixelHeight(info, (float)size);
        std::string line, word;
        auto wordWidth = [&](const std::string& w) {
            auto wc = utf8Codepoints(w);
            float ww = 0; int prev = 0;
            for (int cp : wc) {
                int adv = 0, lsb = 0;
                stbtt_GetCodepointHMetrics(info, cp, &adv, &lsb);
                if (prev) ww += stbtt_GetCodepointKernAdvance(info, prev, cp) * scale;
                ww += adv * scale; prev = cp;
            }
            return ww;
        };
        auto flushWord = [&]() {
            if (word.empty()) return;
            float ww = wordWidth(word);
            float lw = line.empty() ? 0 : wordWidth(line) + wordWidth(" ");
            if (!line.empty() && lw + ww > maxWidth) {
                result.push_back(line);
                line = word;
            } else {
                if (!line.empty()) line += " ";
                line += word;
            }
            word.clear();
        };
        for (size_t i = 0; i < cps.size();) {
            int cp = cps[i];
            if (cp == ' ') {
                flushWord();
                if (line.empty()) { /* ignora espacios iniciales */ }
                ++i;
                continue;
            }
            // reconstruir el carácter utf8
            std::string ch;
            if (cp < 0x80) ch += (char)cp;
            else if (cp < 0x800) { ch += (char)(0xC0 | (cp >> 6)); ch += (char)(0x80 | (cp & 0x3F)); }
            else if (cp < 0x10000) { ch += (char)(0xE0 | (cp >> 12)); ch += (char)(0x80 | ((cp >> 6) & 0x3F)); ch += (char)(0x80 | (cp & 0x3F)); }
            else { ch += (char)(0xF0 | (cp >> 18)); ch += (char)(0x80 | ((cp >> 12) & 0x3F)); ch += (char)(0x80 | ((cp >> 6) & 0x3F)); ch += (char)(0x80 | (cp & 0x3F)); }
            word += ch;
            ++i;
        }
        flushWord();
        if (!line.empty()) result.push_back(line);
        if (result.empty()) result.push_back("");
    }
    return result;
}

SDL_Texture* Gfx::makeTextTexture(const std::string& text, int size, int wrapWidth,
                                  int* outW, int* outH, FontFace face) {
    stbtt_fontinfo* info = (stbtt_fontinfo*)fontInfo(face);
    std::vector<std::string> lines;
    if (wrapWidth > 0) lines = wrapLines(text, size, wrapWidth);
    else lines = splitLines(text);

    const int sizePx = size * RENDER_SCALE;
    float scale = stbtt_ScaleForPixelHeight(info, (float)sizePx);
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(info, &ascent, &descent, &lineGap);
    float ascentPx = ascent * scale;
    float descentPx = -descent * scale;
    int lineSkip = (int)std::round(sizePx * 1.32f);
    if (lineSkip < (int)(ascentPx + descentPx + 2)) lineSkip = (int)(ascentPx + descentPx + 2);

    float maxW = 0;
    for (auto& l : lines) maxW = std::max(maxW, textWidth(l, size, face) * S);
    int W = (int)std::ceil(maxW) + 6 * RENDER_SCALE;
    int H = (int)(lines.size() * lineSkip + ascentPx + descentPx + 6 * RENDER_SCALE);
    W = std::max(W, 1); H = std::max(H, 1);

    std::vector<uint8_t> px((size_t)W * H * 4, 0);
    int baseline0 = (int)std::ceil(ascentPx) + 3 * RENDER_SCALE;

    for (size_t li = 0; li < lines.size(); ++li) {
        auto cps = utf8Codepoints(lines[li]);
        float x = 3.0f * RENDER_SCALE;
        int prev = 0;
        for (int cp : cps) {
            int adv = 0, lsb = 0;
            stbtt_GetCodepointHMetrics(info, cp, &adv, &lsb);
            if (prev) x += stbtt_GetCodepointKernAdvance(info, prev, cp) * scale;
            int xoff = 0, yoff = 0, gw = 0, gh = 0;
            unsigned char* bmp = stbtt_GetCodepointBitmap(info, scale, scale, cp, &gw, &gh, &xoff, &yoff);
            if (bmp) {
                int gx = (int)std::round(x) + xoff;
                int gy = baseline0 + (int)(li * lineSkip) + yoff;
                for (int yy = 0; yy < gh; yy++) {
                    for (int xx = 0; xx < gw; xx++) {
                        int dx = gx + xx, dy = gy + yy;
                        if (dx < 0 || dy < 0 || dx >= W || dy >= H) continue;
                        int a = bmp[yy * gw + xx];
                        if (!a) continue;
                        size_t idx = ((size_t)dy * W + dx) * 4;
                        int da = px[idx + 3];
                        int na = std::min(255, da + a);
                        px[idx + 0] = 255; px[idx + 1] = 255; px[idx + 2] = 255;
                        px[idx + 3] = (uint8_t)na;
                    }
                }
                stbtt_FreeBitmap(bmp, nullptr);
            }
            x += adv * scale;
            prev = cp;
        }
    }

    SDL_Texture* tex = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STATIC, W, H);
    SDL_UpdateTexture(tex, nullptr, px.data(), W * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
    *outW = W; *outH = H;
    return tex;
}

const Gfx::CachedText* Gfx::getText(const std::string& text, int size, int wrapWidth, FontFace face) {
    std::string key = std::to_string((int)face) + "|" + std::to_string(size) + "|" +
                      std::to_string(wrapWidth) + "|" + text;
    auto it = textCache_.find(key);
    if (it != textCache_.end()) return &it->second;
    if (textCache_.size() > 110) clearTextCache();
    CachedText ct;
    ct.tex = makeTextTexture(text, size, wrapWidth, &ct.w, &ct.h, face);
    auto res = textCache_.emplace(key, ct);
    return &res.first->second;
}

void Gfx::clearTextCache() {
    for (auto& kv : textCache_) if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
    textCache_.clear();
}

void Gfx::drawTextTexture(SDL_Texture* tex, float x, float y, int w, int h, Color c, bool shadow) {
    if (!tex) return;
    if (shadow) {
        SDL_FRect dst{x + 1.5f * S, y + 1.5f * S, (float)w, (float)h};
        SDL_SetTextureColorMod(tex, 4, 6, 14);
        SDL_SetTextureAlphaMod(tex, (Uint8)(c.a * 0.55f));
        SDL_RenderCopyF(ren_, tex, nullptr, &dst);
    }
    SDL_FRect dst{x, y, (float)w, (float)h};
    SDL_SetTextureColorMod(tex, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(tex, c.a);
    SDL_RenderCopyF(ren_, tex, nullptr, &dst);
    SDL_SetTextureColorMod(tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(tex, 255);
}

void Gfx::text(float x, float y, const std::string& text, int size, Color c,
               int align, bool shadow, int wrapWidth, FontFace face) {
    if (text.empty()) return;
    const CachedText* ct = getText(text, size, wrapWidth, face);
    if (!ct || !ct->tex) return;
    float w = (float)ct->w / S;
    float dx = x;
    if (align == 1) dx = x - w * 0.5f;
    else if (align == 2) dx = x - w;
    drawTextTexture(ct->tex, dx * S, y * S, ct->w, ct->h, c, shadow);
}

void Gfx::textReveal(float x, float y, int wrapWidth, const std::string& text,
                     int size, Color c, int visibleChars, bool shadow, FontFace face) {
    if (visibleChars <= 0 || text.empty()) return;
    std::vector<std::string> lines = wrapLines(text, size, wrapWidth);
    int lh = (int)std::round(size * 1.32f);
    int remaining = visibleChars;
    for (size_t i = 0; i < lines.size() && remaining > 0; ++i) {
        std::string line = lines[i];
        // contar codepoints
        auto cps = utf8Codepoints(line);
        int take = std::min((int)cps.size(), remaining);
        // reconstruir prefijo
        std::string shown;
        for (int k = 0; k < take; k++) {
            int cp = cps[k];
            if (cp < 0x80) shown += (char)cp;
            else if (cp < 0x800) { shown += (char)(0xC0 | (cp >> 6)); shown += (char)(0x80 | (cp & 0x3F)); }
            else if (cp < 0x10000) { shown += (char)(0xE0 | (cp >> 12)); shown += (char)(0x80 | ((cp >> 6) & 0x3F)); shown += (char)(0x80 | (cp & 0x3F)); }
            else { shown += (char)(0xF0 | (cp >> 18)); shown += (char)(0x80 | ((cp >> 12) & 0x3F)); shown += (char)(0x80 | ((cp >> 6) & 0x3F)); shown += (char)(0x80 | (cp & 0x3F)); }
        }
        this->text(x, y + i * lh, shown, size, c, 0, shadow, 0, face);
        remaining -= take + 1; // el salto de línea consume uno
    }
}

// ---------------- luces ----------------

void Gfx::beginLights(float darkness) {
    lightDarkness_ = std::clamp(darkness, 0.0f, 1.0f);
    uint8_t a = (uint8_t)(lightDarkness_ * 255);
    for (int i = 0; i < lightW_ * lightH_; i++) {
        lightPixels_[i * 4 + 0] = 5;
        lightPixels_[i * 4 + 1] = 7;
        lightPixels_[i * 4 + 2] = 18;
        lightPixels_[i * 4 + 3] = a;
    }
}

void Gfx::addLight(float cx, float cy, float radius, float strength) {
    float sx = (float)lightW_ / 640.0f;
    float sy = (float)lightH_ / 360.0f;
    float mx = cx * sx, my = cy * sy;
    float rx = radius * sx, ry = radius * sy;
    int x0 = std::max(0, (int)(mx - rx));
    int x1 = std::min(lightW_ - 1, (int)(mx + rx));
    int y0 = std::max(0, (int)(my - ry));
    int y1 = std::min(lightH_ - 1, (int)(my + ry));
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float dx = (x + 0.5f - mx) / std::max(1.0f, rx);
            float dy = (y + 0.5f - my) / std::max(1.0f, ry);
            float d = std::sqrt(dx * dx + dy * dy);
            if (d >= 1.0f) continue;
            float t = std::clamp((d - 0.20f) / 0.80f, 0.0f, 1.0f);
            float infl = 1.0f - t * t * (3.0f - 2.0f * t);
            float a = lightDarkness_ * 255.0f * (1.0f - 0.97f * strength * infl);
            uint8_t na = (uint8_t)std::clamp(a, 0.0f, 255.0f);
            uint8_t* p = &lightPixels_[((size_t)y * lightW_ + x) * 4];
            if (na < p[3]) p[3] = na;
        }
    }
}

void Gfx::endLights() {
    SDL_UpdateTexture(lightTex_, nullptr, lightPixels_.data(), lightW_ * 4);
    SDL_FRect dst{0, 0, (float)OUT_W, (float)OUT_H};
    SDL_RenderCopyF(ren_, lightTex_, nullptr, &dst);
}

void Gfx::drawVignette() {
    SDL_FRect dst{0, 0, (float)OUT_W, (float)OUT_H};
    SDL_RenderCopyF(ren_, vignette_, nullptr, &dst);
}

bool Gfx::saveScreenshot(const std::string& path) {
    int w = 0, h = 0;
    if (SDL_GetRendererOutputSize(ren_, &w, &h) != 0 || w <= 0 || h <= 0) return false;
    std::vector<uint8_t> pixels((size_t)w * h * 4);
    if (SDL_RenderReadPixels(ren_, nullptr, SDL_PIXELFORMAT_RGBA32,
                             pixels.data(), w * 4) != 0) {
        SDL_Log("ReadPixels: %s", SDL_GetError());
        return false;
    }
    // SDL_RenderReadPixels ya entrega las filas de arriba a abajo en todos
    // los renderers (GL, GLES, D3D y software), así que no hay que voltear.
    return stbi_write_png(path.c_str(), w, h, 4, pixels.data(), w * 4) != 0;
}
