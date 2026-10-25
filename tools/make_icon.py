#!/usr/bin/env python3
"""Genera el icono de Android a partir del estilo del juego."""
from PIL import Image, ImageDraw, ImageFilter
import os

S = 512
OUT = os.path.join(os.path.dirname(__file__), "..", "android", "app", "src", "main", "res")

img = Image.new("RGBA", (S, S), (6, 10, 20, 255))
px = img.load()
for y in range(S):
    t = y / (S - 1)
    r = int(6 + 12 * t)
    g = int(10 + 20 * t)
    b = int(20 + 28 * t)
    for x in range(S):
        px[x, y] = (r, g, b, 255)

d = ImageDraw.Draw(img, "RGBA")

# luna tenue
d.ellipse((372, 62, 452, 142), fill=(226, 224, 198, 30))

# resplandor de la luciérnaga
glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
gd = ImageDraw.Draw(glow, "RGBA")
gd.ellipse((96, 96, 416, 416), fill=(190, 255, 120, 26))
gd.ellipse((146, 146, 366, 366), fill=(215, 255, 145, 46))
gd.ellipse((188, 188, 324, 324), fill=(238, 255, 178, 80))
glow = glow.filter(ImageFilter.GaussianBlur(38))
img = Image.alpha_composite(img, glow)
d = ImageDraw.Draw(img, "RGBA")

# suelo y pinos en silueta
d.rectangle((0, 424, S, S), fill=(7, 15, 22, 255))
for cx, h, w in ((58, 150, 82), (128, 96, 58), (430, 128, 70), (480, 84, 52), (370, 70, 46)):
    d.polygon([(cx, 424 - h), (cx - w, 430), (cx + w, 430)], fill=(8, 18, 26, 255))
    d.polygon([(cx, 424 - h * 0.55), (cx - w * 0.78, 430), (cx + w * 0.78, 430)],
              fill=(8, 18, 26, 255))

# alas
wings = Image.new("RGBA", (S, S), (0, 0, 0, 0))
wd = ImageDraw.Draw(wings, "RGBA")
wd.ellipse((172, 168, 268, 286), fill=(190, 235, 255, 58), outline=(225, 245, 255, 110), width=3)
wd.ellipse((244, 168, 340, 286), fill=(190, 235, 255, 58), outline=(225, 245, 255, 110), width=3)
wings = wings.rotate(0)
img = Image.alpha_composite(img, wings)
d = ImageDraw.Draw(img, "RGBA")

# cuerpo
d.ellipse((232, 218, 280, 330), fill=(250, 240, 168, 255), outline=(120, 92, 40, 255), width=4)
d.arc((232, 244, 280, 290), 0, 180, fill=(150, 120, 54, 220), width=4)
d.arc((232, 280, 280, 322), 0, 180, fill=(150, 120, 54, 220), width=4)
# cabeza
d.ellipse((234, 196, 278, 240), fill=(92, 72, 42, 255), outline=(40, 30, 18, 255), width=3)
d.ellipse((248, 205, 256, 213), fill=(255, 248, 210, 255))
d.ellipse((262, 205, 270, 213), fill=(255, 248, 210, 255))
# antenas
d.arc((238, 176, 256, 206), 200, 340, fill=(110, 88, 50, 255), width=3)
d.arc((256, 176, 274, 206), 200, 340, fill=(110, 88, 50, 255), width=3)

# chispas
for x, y, r in ((118, 190, 5), (402, 214, 4), (150, 330, 3), (368, 340, 4), (206, 138, 3)):
    d.line((x - r, y, x + r, y), fill=(236, 255, 186, 200), width=2)
    d.line((x, y - r, x, y + r), fill=(236, 255, 186, 200), width=2)

# viñeta
vig = Image.new("RGBA", (S, S), (0, 0, 0, 0))
vd = ImageDraw.Draw(vig, "RGBA")
vd.ellipse((-140, -140, S + 140, S + 140), fill=(0, 0, 0, 0))
mask = Image.new("L", (S, S), 0)
md = ImageDraw.Draw(mask)
md.ellipse((-60, -60, S + 60, S + 60), fill=255)
mask = mask.filter(ImageFilter.GaussianBlur(60))
black = Image.new("RGBA", (S, S), (0, 0, 0, 150))
black.putalpha(Image.eval(mask, lambda v: 150 - int(v * 150 / 255)))
img = Image.alpha_composite(img, black)

img = img.filter(ImageFilter.SMOOTH)

sizes = {"mipmap-mdpi": 48, "mipmap-hdpi": 72, "mipmap-xhdpi": 96,
         "mipmap-xxhdpi": 144, "mipmap-xxxhdpi": 192}
for folder, size in sizes.items():
    path = os.path.join(OUT, folder, "ic_launcher.png")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.resize((size, size), Image.LANCZOS).save(path)
    print("icono:", path, size)

img.save(os.path.join(os.path.dirname(__file__), "..", "dist", "icono-512.png"))
print("icono grande: dist/icono-512.png")
