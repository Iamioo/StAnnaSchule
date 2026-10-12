"""Sanity checks for the hand-written data in src/game_data.inc.

Every map row must be MAP_W wide, every pixel-art block must have rows of
the right width, table lengths must match their declared sizes, and doors
must actually lead somewhere.
"""
import re
import sys
from pathlib import Path

src = Path(sys.argv[1] if len(sys.argv) > 1 else "src/game_data.inc").read_text(encoding="utf-8")
errors = 0


def err(msg):
    global errors
    errors += 1
    print(msg)


def block(name):
    m = re.search(r"\b" + name + r"\[([A-Z_0-9]*)\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        sys.exit(f"{name} not found")
    body = re.sub(r"/\*.*?\*/", "", m.group(2), flags=re.S)
    return m.group(1), body


def strings(name):
    return re.findall(r'"([^"]*)"', block(name)[1])


def numbers(name):
    return re.findall(r"-?\b(?:0x[0-9A-Fa-f]+|\d+)\b", block(name)[1])


defines = {k: int(v) for k, v in re.findall(r"#define (\w+) (\d+)\n", src)}


def size_of(name):
    s = block(name)[0]
    return defines[s] if s in defines else int(s)


W, H, NF = defines["MAP_W"], defines["MAP_H"], defines["NFLOORS"]
rows = strings("sa_floor_src")
if len(rows) != H * NF:
    err(f"map: {len(rows)} rows, expected {H * NF}")
for i, r in enumerate(rows):
    if len(r) != W:
        err(f"floor {i // H + 1} row {i % H}: width {len(r)}")

# every door must connect two walkable sides
walk = set(".,cthfm_;-rDKPH^v")
for f in range(NF):
    fl = rows[f * H:(f + 1) * H]
    if any(len(r) != W for r in fl):
        continue
    for y in range(H):
        for x in range(W):
            if fl[y][x] == "D":
                ns = (fl[y - 1][x] in walk and fl[y + 1][x] in walk) if 0 < y < H - 1 else False
                ew = (fl[y][x - 1] in walk and fl[y][x + 1] in walk) if 0 < x < W - 1 else False
                if not (ns or ew):
                    err(f"floor {f + 1}: door at ({x},{y}) leads nowhere")

art = strings("sa_art")
if len(art) != defines["ART_COUNT"]:
    err(f"art: {len(art)} rows, expected {defines['ART_COUNT']}")
offsets = sorted((v, k) for k, v in defines.items() if k.startswith("ART_") and k != "ART_COUNT")
offsets.append((defines["ART_COUNT"], "END"))
for (start, name), (end, _) in zip(offsets, offsets[1:]):
    widths = {len(r) for r in art[start:end]}
    if len(widths) != 1:
        err(f"{name}: mixed widths {sorted(widths)}")
        for j, r in enumerate(art[start:end]):
            print(f"   {j:2} {len(r):2} {r}")
    for r in art[start:end]:
        bad = set(r) - set(".0123456789abcdefx")
        if bad:
            err(f"{name}: bad chars {bad}")

chars = re.search(r'sa_font_chars = "(.*?)";', src).group(1)
want = (len(chars) + 4) * 7
if len(numbers("sa_font")) != want or size_of("sa_font") != want:
    err(f"font: {len(numbers('sa_font'))} bytes, expected {want}")

for name in ("sa_rm_f", "sa_rm_x0", "sa_rm_y0", "sa_rm_x1", "sa_rm_y1"):
    if len(numbers(name)) != size_of(name):
        err(f"{name}: {len(numbers(name))} entries, expected {size_of(name)}")
if len(strings("sa_rm_name")) != size_of("sa_rm_name"):
    err(f"sa_rm_name: {len(strings('sa_rm_name'))} entries")
for name in ("sa_lb_f", "sa_lb_x", "sa_lb_y", "sa_lb_c", "sa_lb_bg"):
    if len(numbers(name)) != size_of(name):
        err(f"{name}: {len(numbers(name))} entries, expected {size_of(name)}")
if len(strings("sa_lb_text")) != size_of("sa_lb_text"):
    err(f"sa_lb_text: {len(strings('sa_lb_text'))} entries")
for name in ("sa_mus_lead", "sa_mus_bass", "sa_mus_drum", "sa_sfx_def", "sa_ring_mel", "sa_note_base",
             "sa_lamp_x", "sa_lamp_y", "sa_lamp_r", "sa_sign_c1", "sa_sign_c2", "sa_sign_dith",
             "sa_trk_off", "sa_trk_len", "sa_trk_tps"):
    if len(numbers(name)) != size_of(name):
        err(f"{name}: {len(numbers(name))} entries, expected {size_of(name)}")

print("OK" if errors == 0 else f"{errors} problem(s)")
sys.exit(1 if errors else 0)
