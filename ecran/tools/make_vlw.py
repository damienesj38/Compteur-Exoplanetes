"""Génère des polices lissées .vlw (format Processing, lu par LovyanGFX) depuis un TTF,
puis les convertit en tableaux C. Usage : python3 make_vlw.py <ttf> <taille_px> <nom> <dossier_sortie>"""
import struct, sys, freetype
from pathlib import Path

CHARS = [c for c in range(0x20, 0x7F)] + [c for c in range(0xA0, 0x100)] + \
        [0x0152, 0x0153, 0x2013, 0x2019, 0x201C, 0x201D, 0x2026, 0x2032, 0x2033, 0x2192, 0x2212, 0x20AC]

def build(ttf, px):
    face = freetype.Face(ttf)
    face.set_pixel_sizes(0, px)
    glyphs = []
    for cp in sorted(set(CHARS)):
        if face.get_char_index(cp) == 0:
            continue
        face.load_char(cp, freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL)
        g = face.glyph
        bm = g.bitmap
        w, h = bm.width, bm.rows
        data = bytes(bm.buffer) if bm.pitch == w else b"".join(bytes(bm.buffer[r*bm.pitch:r*bm.pitch+w]) for r in range(h))
        adv = (g.advance.x + 32) >> 6
        glyphs.append((cp, h, w, adv, g.bitmap_top, g.bitmap_left, data))
    asc = (face.size.ascender + 32) >> 6
    desc = abs((face.size.descender - 32) >> 6)
    out = struct.pack(">6i", len(glyphs), 11, px, 0, asc, desc)
    for cp, h, w, adv, top, left, _ in glyphs:
        out += struct.pack(">7i", cp, h, w, adv, top, left, 0)
    for *_, data in glyphs:
        out += data
    return out

ttf, px, name, dossier = sys.argv[1], int(sys.argv[2]), sys.argv[3], Path(sys.argv[4])
vlw = build(ttf, px)
lignes = ",".join(str(b) for b in vlw)
(dossier / f"{name}.h").write_text(
    f"// Police Barlow Condensed (SIL Open Font License 1.1), {px} px, générée par tools/make_vlw.py\n"
    f"#pragma once\n#include <stdint.h>\nstatic const uint8_t {name}[] = {{{lignes}}};\n", encoding="utf-8")
print(name, len(vlw), "octets")
