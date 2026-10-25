// level.cpp - construcción y dibujo del bosque nocturno
#include "level.hpp"
#include <algorithm>
#include <cmath>
#include <random>

static const float PI = 3.14159265358979f;

static uint32_t hash2(int x, int y) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

Level::Level() { build(); }

void Level::set(int x, int y) {
    if (x >= 0 && x < w && y >= 0 && y < h) solid[(size_t)y * w + x] = 1;
}

void Level::addSolid(int tx, int ty, int tw, int th) {
    for (int y = ty; y < ty + th; y++)
        for (int x = tx; x < tx + tw; x++) set(x, y);
}

bool Level::solidAt(int tx, int ty) const {
    if (tx < 0 || tx >= w) return true;   // límites laterales
    if (ty < 0 || ty >= h) return false;
    return solid[(size_t)ty * w + tx] != 0;
}

bool Level::hits(float x, float y, float ww, float hh) const {
    int x0 = (int)std::floor(x / TILE);
    int x1 = (int)std::floor((x + ww - 0.001f) / TILE);
    int y0 = (int)std::floor(y / TILE);
    int y1 = (int)std::floor((y + hh - 0.001f) / TILE);
    for (int ty = y0; ty <= y1; ty++)
        for (int tx = x0; tx <= x1; tx++)
            if (solidAt(tx, ty)) return true;
    return false;
}

bool Level::inWater(float x, float y) const {
    for (auto& wa : waters) {
        if (x > wa.x * TILE && x < (wa.x + wa.w) * TILE &&
            y > wa.y * TILE && y < (wa.y + wa.h) * TILE + 200)
            return true;
    }
    return false;
}

float Level::groundTop(int tx) const {
    for (int y = 0; y < h; y++) if (solidAt(tx, y)) return (float)y;
    return (float)h;
}

void Level::addPine(float tx, float groundTy, float scale, Color tint, float alpha) {
    float px = tx * TILE;
    float py = groundTy * TILE - 18.0f * scale;
    decors.push_back(Decor{126, px, py, scale, 0, tint, false});
    (void)alpha;
}

void Level::addDecorAt(int tile, float tx, float groundTy, float scale, int layer, Color tint, bool flip) {
    decors.push_back(Decor{tile, tx * TILE, groundTy * TILE - 18.0f * scale,
                           scale, layer, tint, flip});
}

void Level::build() {
    solid.assign((size_t)w * h, 0);

    // ---------- Zona 1: el sendero ----------
    addSolid(0, 29, 30, h - 29);
    addSolid(12, 27, 5, 2);
    addSolid(33, 29, 20, h - 29);
    addSolid(36, 27, 5, 2);      // plataforma de la primera luciérnaga
    addSolid(44, 27, 4, 2);
    addSolid(56, 29, 18, h - 29);
    addSolid(77, 28, 11, h - 28);

    // ---------- Zona 2: el arroyo ----------
    addSolid(90, 27, 4, 2);
    addSolid(97, 25, 4, 2);
    addSolid(103, 27, 5, 2);
    addSolid(110, 25, 4, 2);
    addSolid(116, 26, 5, 2);
    addSolid(123, 24, 4, 2);
    addSolid(129, 25, 5, 2);
    addSolid(136, 23, 4, 2);
    addSolid(142, 24, 5, 2);
    addSolid(149, 23, 8, 3);     // isla del farol
    addSolid(158, 21, 4, 2);
    addSolid(164, 19, 5, 2);
    addSolid(168, 24, 21, h - 24); // orilla

    // ---------- Zona 3: la colina ----------
    addSolid(188, 22, 9, h - 22);
    addSolid(197, 20, 9, h - 20);
    addSolid(206, 18, 9, h - 18);
    addSolid(215, 16, 10, h - 16);
    addSolid(226, 14, 5, 2);
    addSolid(233, 12, 6, 2);
    addSolid(240, 14, 5, 2);
    addSolid(246, 12, 4, 2);
    addSolid(250, 12, 30, h - 12); // meseta final

    // agua / abismo
    waters.push_back({29, 32, 5, 6});
    waters.push_back({52, 32, 6, 6});
    waters.push_back({74, 32, 4, 6});
    waters.push_back({88, 32, 81, 6});
    waters.push_back({225, 32, 55, 6});

    // luciérnagas (posiciones en píxeles)
    fireflies.push_back({38.5f * TILE, 25.15f * TILE, false, 0.0f, 0});
    fireflies.push_back({166.0f * TILE, 16.45f * TILE, false, 1.7f, 1});
    fireflies.push_back({258.0f * TILE, 9.25f * TILE, false, 3.1f, 2});

    // faroles (checkpoints)
    checkpoints.push_back({70.0f, 29.0f, 0.0f, false});
    checkpoints.push_back({152.0f, 23.0f, 1.4f, false});
    checkpoints.push_back({218.0f, 16.0f, 2.8f, false});

    // ---------- decoración ----------
    Color pineBack(54, 86, 118, 255);
    Color pineFar(38, 62, 88, 255);
    addPine(0.8f, 29, 3.2f, pineBack, 1);
    addPine(15.5f, 27, 3.0f, pineBack, 1);
    addPine(9.0f, 29, 2.5f, pineFar, 1);
    addPine(19.0f, 29, 4.0f, pineBack, 1);
    addPine(26.0f, 29, 2.8f, pineBack, 1);
    addPine(47.0f, 29, 3.6f, pineBack, 1);
    addPine(52.0f, 29, 2.2f, pineFar, 1);
    addPine(61.0f, 29, 3.1f, pineBack, 1);
    addPine(68.0f, 29, 2.3f, pineFar, 1);
    addPine(81.0f, 28, 3.3f, pineBack, 1);
    addPine(86.0f, 28, 2.2f, pineFar, 1);
    addPine(105.0f, 27, 2.4f, pineFar, 1);
    addPine(124.5f, 24, 2.6f, pineFar, 1);
    addPine(143.0f, 24, 2.4f, pineBack, 1);
    addPine(155.0f, 23, 3.4f, pineBack, 1);
    addPine(172.0f, 24, 3.8f, pineBack, 1);
    addPine(181.0f, 24, 2.4f, pineFar, 1);
    addPine(192.0f, 22, 3.2f, pineBack, 1);
    addPine(201.0f, 20, 2.6f, pineFar, 1);
    addPine(210.0f, 18, 3.5f, pineBack, 1);
    addPine(221.0f, 16, 2.7f, pineFar, 1);
    addPine(234.5f, 12, 3.1f, pineFar, 1);

    // hierba, setas y arbustos (delante)
    Color front(140, 166, 196, 255);
    Color mush(185, 178, 178, 255);
    Color bush(70, 108, 112, 255);
    addDecorAt(124, 3.0f, 29, 1.0f, 1, front);
    addDecorAt(125, 7.0f, 29, 1.0f, 1, front);
    addDecorAt(124, 14.0f, 27, 1.1f, 1, front);
    addDecorAt(128, 15.6f, 27, 1.0f, 1, mush);
    addDecorAt(125, 24.0f, 29, 1.0f, 1, front);
    addDecorAt(124, 33.6f, 29, 1.0f, 1, front);
    addDecorAt(128, 37.4f, 27, 1.0f, 1, mush);
    addDecorAt(125, 40.0f, 27, 1.0f, 1, front);
    addDecorAt(124, 47.0f, 27, 1.0f, 1, front);
    addDecorAt(125, 57.2f, 29, 1.0f, 1, front);
    addDecorAt(128, 60.2f, 29, 1.0f, 1, mush);
    addDecorAt(124, 71.0f, 29, 1.0f, 1, front);
    addDecorAt(125, 78.0f, 28, 1.0f, 1, front);
    addDecorAt(124, 92.5f, 27, 1.0f, 1, front);
    addDecorAt(128, 104.4f, 27, 1.0f, 1, mush);
    addDecorAt(125, 111.2f, 25, 1.0f, 1, front);
    addDecorAt(128, 117.4f, 26, 1.0f, 1, mush);
    addDecorAt(124, 130.0f, 25, 1.0f, 1, front);
    addDecorAt(128, 143.5f, 24, 1.0f, 1, mush);
    addDecorAt(125, 150.4f, 23, 1.0f, 1, front);
    addDecorAt(124, 159.0f, 21, 1.0f, 1, front);
    addDecorAt(128, 165.0f, 19, 1.0f, 1, mush);
    addDecorAt(125, 173.5f, 24, 1.0f, 1, front);
    addDecorAt(128, 179.0f, 24, 1.0f, 1, mush);
    addDecorAt(124, 190.0f, 22, 1.0f, 1, front);
    addDecorAt(125, 199.0f, 20, 1.0f, 1, front);
    addDecorAt(128, 208.2f, 18, 1.0f, 1, mush);
    addDecorAt(124, 217.0f, 16, 1.0f, 1, front);
    addDecorAt(128, 227.6f, 14, 1.0f, 1, mush);
    addDecorAt(125, 234.5f, 12, 1.0f, 1, front);
    addDecorAt(124, 247.0f, 12, 1.0f, 1, front);
    addDecorAt(128, 252.8f, 12, 1.0f, 1, mush);
    addDecorAt(125, 262.0f, 12, 1.0f, 1, front);

    addDecorAt(17, 5.5f, 29, 1.4f, 1, bush);
    addDecorAt(18, 49.0f, 29, 1.6f, 1, bush);
    addDecorAt(19, 65.0f, 29, 1.3f, 1, bush, true);
    addDecorAt(17, 82.6f, 28, 1.4f, 1, bush);
    addDecorAt(18, 170.3f, 24, 1.5f, 1, bush);
    addDecorAt(19, 195.5f, 22, 1.2f, 1, bush, true);
    addDecorAt(17, 214.1f, 16, 1.3f, 1, bush);
    addDecorAt(18, 250.5f, 12, 1.5f, 1, bush);
    addDecorAt(19, 267.0f, 12, 1.7f, 1, bush, true);

    // carteles del camino
    decors.push_back(Decor{86, 8.0f * TILE, 29 * TILE - 23.0f, 1.3f, 1, front, false});
    decors.push_back(Decor{85, 92.0f * TILE, 27 * TILE - 23.0f, 1.25f, 1, front, false});
    decors.push_back(Decor{87, 178.0f * TILE, 24 * TILE - 23.0f, 1.3f, 1, front, true});
    decors.push_back(Decor{86, 234.0f * TILE, 12 * TILE - 23.0f, 1.2f, 1, front, false});

    // estrellas
    std::mt19937 rng(20261025);
    std::uniform_real_distribution<float> ux(0, 640), uy(0, 235), us(0.6f, 1.8f), up(0, 6.283f);
    for (int i = 0; i < 190; i++) stars_.push_back({ux(rng), uy(rng), us(rng), up(rng)});
}

void Level::drawSky(Gfx& g, float camX, float camY, float time, float dawn) {
    Color top((Uint8)(7 + dawn * 40), (Uint8)(11 + dawn * 45), (Uint8)(30 + dawn * 55), 255);
    Color mid((Uint8)(15 + dawn * 65), (Uint8)(26 + dawn * 55), (Uint8)(54 + dawn * 40), 255);
    Color low((Uint8)(30 + dawn * 90), (Uint8)(48 + dawn * 40), (Uint8)(72 + dawn * 10), 255);
    // banda media
    g.vGradient(0, 0, VIEW_W, VIEW_H * 0.55f, top, mid);
    g.vGradient(0, VIEW_H * 0.55f, VIEW_W, VIEW_H * 0.45f, mid, low);

    float horizon = 236.0f - camY * 0.045f;
    g.vGradient(0, horizon - 70, VIEW_W, 120, Color(65, 70, 100, 0), Color(80, 85, 115, 24));

    // estrellas
    for (auto& s : stars_) {
        float sx = std::fmod(s.x - camX * 0.012f, 640.0f);
        if (sx < 0) sx += 640.0f;
        float sy = s.y - camY * 0.025f + 8;
        if (sy < 0 || sy > 250) continue;
        float tw = 0.55f + 0.45f * std::sin(time * 0.9f + s.phase);
        Uint8 a = (Uint8)(70 + 150 * tw);
        g.rect(sx, sy, s.size, s.size, Color(205, 220, 255, a));
        if (s.size > 1.5f) {
            g.rect(sx - 2, sy + 0.5f, 6, 1, Color(150, 190, 255, (Uint8)(a / 2)));
            g.rect(sx + 0.5f, sy - 2, 1, 6, Color(150, 190, 255, (Uint8)(a / 2)));
        }
    }

    // luna
    float mx = 512.0f - std::fmod(camX * 0.02f, 80.0f);
    float my = 64.0f - camY * 0.03f;
    g.glow(mx, my, 92, Color(120, 160, 220, 28));
    g.glow(mx, my, 44, Color(180, 205, 240, 42));
    g.circle(mx, my, 20, Color(232, 226, 198, 245));
    g.circle(mx - 4, my - 3, 4, Color(206, 199, 172, 160));
    g.circle(mx + 6, my + 4, 3, Color(206, 199, 172, 130));
    g.circle(mx + 2, my - 7, 2, Color(206, 199, 172, 120));

    // nubes altas
    float drift = time * 3.0f;
    for (int c = 0; c < 3; c++) {
        float bx = std::fmod(80 + c * 270 + drift - camX * 0.04f, 900.0f) - 120;
        float by = 86 + c * 34 - camY * 0.04f;
        for (int k = 0; k < 4; k++) {
            g.circle(bx + k * 26, by + std::sin(k * 1.7f) * 5, 20 - k * 2,
                     Color(90, 110, 150, 12));
        }
    }
}

void Level::drawFar(Gfx& g, float camX, float camY, float time) {
    float horizon = 236.0f - camY * 0.045f;

    // montañas lejanas (scanlines)
    for (int layer = 0; layer < 2; layer++) {
        float par = layer == 0 ? 0.045f : 0.085f;
        Color col = layer == 0 ? Color(9, 15, 28, 235) : Color(7, 12, 23, 245);
        int baseH = layer == 0 ? 36 : 22;
        for (int sx = 0; sx < VIEW_W; sx += 3) {
            float wx = sx + camX * par;
            float hh = baseH + 15 * std::sin(wx * 0.009f + layer * 2.0f)
                       + 8 * std::sin(wx * 0.023f + 1.0f)
                       + 4 * std::sin(wx * 0.061f + 2.0f);
            float top = horizon - hh;
            g.rect((float)sx, top, 3, horizon - top + 30, col);
        }
    }

    // niebla del horizonte
    g.vGradient(0, horizon - 22, VIEW_W, 70, Color(70, 95, 120, 0), Color(60, 85, 110, 22));

    // bosque lejano: pinos de Kenney tintados
    for (int layer = 0; layer < 2; layer++) {
        float par = layer == 0 ? 0.14f : 0.27f;
        float scale = layer == 0 ? 2.4f : 3.6f;
        Color tint = layer == 0 ? Color(22, 40, 58, 255) : Color(12, 23, 37, 255);
        int spacing = layer == 0 ? 64 : 103;
        float width = 1100.0f;
        for (int i = 0; i < 22; i++) {
            float wx = i * (float)spacing + (layer ? 31.0f : 10.0f);
            float sx = std::fmod(wx - camX * par, width);
            if (sx < 0) sx += width;
            sx -= 220;
            float base = horizon + (layer == 0 ? 16 : 26);
            float sc = scale + (float)((i * 7 + layer * 3) % 5) * 0.12f;
            g.tile(126, sx, base - 18 * sc, sc, (i & 1), tint);
        }
    }

    // suelo de niebla bajo el bosque lejano
    g.rect(0, horizon + 18, VIEW_W, 90, Color(7, 13, 22, 90));
}

void Level::drawBackDecor(Gfx& g, float camX, float camY, float time) {
    for (auto& d : decors) {
        if (d.layer != 0) continue;
        float sx = d.x - camX;
        float sy = d.y - camY;
        if (sx < -120 || sx > VIEW_W + 80 || sy > VIEW_H + 80) continue;
        g.tile(d.tile, sx, sy, d.scale, d.flip, d.tint, 0.95f);
    }
}

void Level::drawWater(Gfx& g, float camX, float camY, float time) {
    for (auto& wa : waters) {
        float x0 = wa.x * TILE - camX;
        float y0 = wa.y * TILE - camY;
        float ww = wa.w * TILE;
        if (x0 > VIEW_W || x0 + ww < 0) continue;
        if (y0 > VIEW_H) continue;
        float hh = (h - wa.y) * TILE;
        g.vGradient(x0, y0, ww, hh, Color(15, 35, 58, 235), Color(3, 7, 16, 255));

        // niebla sobre el agua
        g.vGradient(x0, y0 - 28, ww, 46, Color(80, 120, 150, 0), Color(80, 120, 150, 18));

        // superficie ondulada
        for (float x = 0; x < ww; x += 5.0f) {
            float yy = y0 + std::sin(time * 1.6f + (x0 + x) * 0.06f) * 1.8f
                       + std::sin(time * 0.7f + (x0 + x) * 0.021f) * 1.2f;
            float a = 0.35f + 0.25f * std::sin(time * 1.2f + (x0 + x) * 0.05f);
            g.rect(x0 + x, yy, 5, 2, Color(95, 160, 180, (Uint8)(80 * a + 45)));
        }
        // destellos
        for (int i = 0; i < 22; i++) {
            float gx = x0 + (float)((i * 83 + (int)wa.x * 17) % (int)std::max(1.0f, ww));
            float gy = y0 + 8 + (float)((i * 47) % 70);
            float a = 0.5f + 0.5f * std::sin(time * 1.4f + i * 1.7f);
            g.rect(gx, gy, 2, 1, Color(150, 195, 210, (Uint8)(a * 60)));
        }
    }
}

void Level::drawTerrain(Gfx& g, float camX, float camY, float time) {
    int x0 = std::max(0, (int)(camX / TILE) - 1);
    int x1 = std::min(w - 1, (int)((camX + VIEW_W) / TILE) + 1);
    int y0 = std::max(0, (int)(camY / TILE) - 1);
    int y1 = std::min(h - 1, (int)((camY + VIEW_H) / TILE) + 1);

    static const int dirt[4] = {120, 121, 122, 123};

    for (int ty = y0; ty <= y1; ty++) {
        for (int tx = x0; tx <= x1; tx++) {
            if (!solidAt(tx, ty)) continue;
            bool up = solidAt(tx, ty - 1);
            bool left = solidAt(tx - 1, ty);
            bool right = solidAt(tx + 1, ty);
            bool down = solidAt(tx, ty + 1);
            uint32_t hs = hash2(tx, ty);
            int idx;
            Color tint;
            float depth = (float)ty / h;
            if (!up) {
                idx = (int)(hs % 4);
                tint = Color(122, 158, 188, 255);
            } else {
                idx = dirt[(hs >> 3) & 3];
                Uint8 base = (Uint8)(82 + 18 * (1.0f - depth));
                tint = Color(base, (Uint8)(base + 22), (Uint8)(base + 56), 255);
            }
            float sx = tx * TILE - camX;
            float sy = ty * TILE - camY;
            g.tile(idx, sx, sy, 1.0f, false, tint, 1.0f);
            if (!up) {
                g.rect(sx, sy, TILE, 1, Color(135, 195, 215, 70));
                // borde de hierba
                if (!left || !right) {
                    g.rect(sx + (left ? TILE - 2 : 0), sy + 2, 2, TILE - 2,
                           Color(20, 40, 48, 60));
                }
            }
            // raíces y grietas en el talud
            if (up && !left && (hs % 7 == 0)) {
                float vx = sx + 4 + (hs % 9);
                g.rect(vx, sy + 4, 1, 10, Color(20, 46, 42, 120));
                g.rect(vx, sy + 14, 1, 8, Color(18, 40, 38, 90));
                g.rect(vx + 1, sy + 9, 3, 1, Color(28, 58, 48, 100));
            }
            if (!up && !left && (hs % 5 == 0)) {
                g.rect(sx + 2, sy + 6, 1, 8, Color(16, 22, 30, 110));
                g.rect(sx + 3, sy + 13, 1, 4, Color(16, 22, 30, 90));
            }
            // oscurecer profundidades
            if (!down && ty > 2) g.rect(sx, sy + TILE - 2, TILE, 2, Color(8, 14, 24, 90));
        }
    }
}

void Level::drawFrontDecor(Gfx& g, float camX, float camY, float time) {
    for (auto& d : decors) {
        if (d.layer != 1) continue;
        float sx = d.x - camX;
        float sy = d.y - camY;
        if (sx < -60 || sx > VIEW_W + 60 || sy > VIEW_H + 40) continue;
        float sway = 0;
        if (d.tile == 124 || d.tile == 125)
            sway = std::sin(time * 1.7f + d.x * 0.09f) * 0.8f;
        g.tile(d.tile, sx, sy, d.scale, d.flip, d.tint, 0.96f);
        (void)sway;
    }

    // faroles (checkpoints)
    for (auto& cp : checkpoints) {
        float sx = cp.tx * TILE - camX + 9;
        float sy = cp.ty * TILE - camY;
        if (sx < -40 || sx > VIEW_W + 40) continue;
        // poste
        g.rect(sx - 2, sy - 34, 4, 34, Color(32, 26, 30, 255));
        g.rect(sx - 5, sy - 6, 10, 6, Color(24, 20, 24, 255));
        // farol de Kenney
        g.tile(152, sx - 11, sy - 52, 1.25f, false, cp.active ? Color(255, 220, 160, 255)
                                                                 : Color(120, 130, 150, 255));
        float a = cp.active ? 1.0f : 0.35f;
        g.glow(sx, sy - 41, cp.active ? 34 : 16,
               Color(255, 190, 100, (Uint8)(90 * a + 25 * std::sin(time * 4 + cp.phase))));
    }
}

void Level::drawTree(Gfx& g, float camX, float camY, float time, float bloom) {
    float cx = 271.0f * TILE - camX;
    float base = 12.0f * TILE - camY + 2;
    if (cx < -400 || cx > VIEW_W + 500) return;

    Color trunk(20, 15, 24, 255);
    Color trunk2(28, 21, 28, 255);

    // raíces
    g.rect(cx - 18, base - 4, 36, 6, trunk);
    g.rect(cx - 42, base - 2, 26, 4, trunk);
    g.rect(cx + 16, base - 2, 30, 4, trunk);

    // tronco estrechado
    const float topY = base - 210;
    for (float y = topY; y < base; y += 4) {
        float t = (y - topY) / (base - topY);
        float w = 10 + 30 * t * t;
        g.rect(cx - w * 0.5f, y, w, 4.5f, trunk);
        if (t > 0.25f) g.rect(cx - w * 0.5f + 2, y, 2, 4, trunk2);
    }
    // ramas gruesas
    auto branch = [&](float x0, float y0, float x1, float y1, float thick) {
        for (int i = 0; i <= 40; i++) {
            float t = i / 40.0f;
            float x = x0 + (x1 - x0) * t;
            float y = y0 + (y1 - y0) * t;
            float th = thick * (1.0f - 0.65f * t);
            g.rect(x - th * 0.5f, y - th * 0.5f, th, th, trunk);
        }
    };
    branch(cx - 6, base - 138, cx - 86, base - 178, 13);
    branch(cx + 4, base - 158, cx + 92, base - 198, 12);
    branch(cx - 2, base - 108, cx - 60, base - 142, 9);
    branch(cx + 2, base - 84, cx + 54, base - 116, 8);
    branch(cx - 4, base - 186, cx - 44, base - 214, 7);

    // copa: círculos oscuros agrupados
    struct Blob { float x, y, r; };
    Blob blobs[] = {
        {-66, -170, 42}, {22, -188, 50}, {88, -170, 38}, {-18, -204, 44},
        {56, -210, 34}, {-104, -164, 28}, {-46, -198, 32}, {112, -186, 26},
        {-130, -180, 20}, {10, -226, 28}, {-80, -216, 22}, {72, -230, 20},
        {138, -202, 18}, {-26, -244, 18}
    };
    for (auto& b : blobs) {
        Color c((Uint8)(10 + bloom * 18), (Uint8)(24 + bloom * 26), (Uint8)(26 + bloom * 18), 255);
        g.circle(cx + b.x, base + b.y, b.r, c);
    }
    for (auto& b : blobs) {
        Color c((Uint8)(16 + bloom * 30), (Uint8)(38 + bloom * 30), (Uint8)(38 + bloom * 24), 120);
        g.circle(cx + b.x - b.r * 0.18f, base + b.y - b.r * 0.2f, b.r * 0.62f, c);
    }

    // hueco cálido
    g.circle(cx, base - 26, 26, Color(24, 18, 22, 255));
    g.glow(cx, base - 26, 60 + bloom * 90, Color(255, 170, 80, (Uint8)(35 + bloom * 80)));
    g.circle(cx, base - 26, 8 + bloom * 3, Color(255, 200, 120, (Uint8)(90 + bloom * 100)));

    // luciérnagas en la copa
    for (int i = 0; i < 16; i++) {
        float a = time * (0.15f + 0.03f * (i % 5)) + i * 1.7f;
        float rr = 60 + 62 * std::sin(i * 3.1f);
        float px = cx + std::cos(a) * rr;
        float py = base - 172 + std::sin(a * 1.3f) * 42 + std::sin(i * 2.0f) * 14;
        float tw = 0.5f + 0.5f * std::sin(time * 2.0f + i);
        g.glow(px, py, 4 + tw * 4, Color(220, 255, 130, (Uint8)(50 + 70 * tw)));
    }
    (void)bloom;
}
