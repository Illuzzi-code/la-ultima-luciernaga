// level.hpp - nivel, colisiones y decoración del bosque
#pragma once
#include "gfx.hpp"
#include <vector>
#include <cstdint>

constexpr int TILE = 18;
constexpr int VIEW_W = 640;
constexpr int VIEW_H = 360;

struct Water { float x, y, w, h; }; // en tiles

struct FireflySpot {
    float x = 0, y = 0; // píxeles, centro
    bool taken = false;
    float phase = 0;
    int memory = 0;
};

struct Checkpoint {
    float tx = 0, ty = 0; // tile superior del suelo
    float phase = 0;
    bool active = false;
};

struct Decor {
    int tile;
    float x, y;      // esquina superior izquierda en píxeles
    float scale;
    int layer;       // 0 detrás del terreno, 1 delante
    Color tint;
    bool flip;
};

class Level {
public:
    Level();
    void build();

    int w = 280, h = 34;
    std::vector<uint8_t> solid;

    std::vector<Water> waters;
    std::vector<FireflySpot> fireflies;
    std::vector<Checkpoint> checkpoints;
    std::vector<Decor> decors;

    bool solidAt(int tx, int ty) const;
    bool hits(float x, float y, float w, float h) const;
    bool inWater(float x, float y) const;
    float groundTop(int tx) const;

    void drawSky(Gfx& g, float camX, float camY, float time, float dawn);
    void drawFar(Gfx& g, float camX, float camY, float time);
    void drawBackDecor(Gfx& g, float camX, float camY, float time);
    void drawWater(Gfx& g, float camX, float camY, float time);
    void drawTerrain(Gfx& g, float camX, float camY, float time);
    void drawFrontDecor(Gfx& g, float camX, float camY, float time);
    void drawTree(Gfx& g, float camX, float camY, float time, float bloom);

    void addSolid(int tx, int ty, int tw, int th);
    void addPine(float tx, float groundTy, float scale, Color tint, float alpha);
    void addDecorAt(int tile, float tx, float groundTy, float scale, int layer, Color tint, bool flip = false);

private:
    struct Star { float x, y, size, phase; };
    std::vector<Star> stars_;
    void set(int x, int y);
};
