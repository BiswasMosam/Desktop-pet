"""Build the handbook twice from handbook.src.html:

  site/index.html   the web page: animated GIFs, Google Fonts
  paper.html        the PDF source: frame strips, local fonts, A4 pages

Every screen image is a real frame-buffer capture from shots/*.npz, drawn
as vector pixels (or, on the web, the GIF made from the same capture).
"""
import html
import re

import numpy as np

FONT_C = "D:/Garage/Desktop-pet/.pio/libdeps/blackpill_f401cc/Adafruit GFX Library/glcdfont.c"

WEB_FONTS = ('<link rel="preconnect" href="https://fonts.googleapis.com">'
             '<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>'
             '<link rel="stylesheet" href="https://fonts.googleapis.com/css2?'
             'family=Manrope:wght@400;500;600;700&family=Space+Mono&family=Syne:wght@600;700;800&display=swap">')

LOCAL = "file:///D:/Garage/BiswasMosam.github.io/fonts/"
MONO_RANGE = ("U+0000-003F, U+0041-00FF, U+0131, U+0152-0153, U+02BB-02BC, U+02C6, "
              "U+02DA, U+02DC, U+2000-206F, U+20AC, U+2122, U+2191, U+2193, U+2212, "
              "U+2215, U+FEFF, U+FFFD")
PAPER_FONTS = "<style>" + "".join(
    f"@font-face{{font-family:'{fam}';font-weight:{w};font-style:normal;"
    f"src:url('{LOCAL}{file}') format('woff2'){rng}}}"
    for fam, w, file, rng in [
        ("Syne", 600, "syne-600-latin.woff2", ""),
        ("Syne", 700, "syne-700-latin.woff2", ""),
        ("Syne", 800, "syne-800-latin.woff2", ""),
        ("Manrope", 400, "manrope-400-latin.woff2", ""),
        ("Manrope", 500, "manrope-500-latin.woff2", ""),
        ("Manrope", 600, "manrope-600-latin.woff2", ""),
        ("Manrope", 700, "manrope-600-latin.woff2", ""),
        ("Space Mono", 400, "space-mono-400-latin.woff2", f";unicode-range:{MONO_RANGE}"),
    ]) + "</style>"


# ---------------------------------------------------------------- captures

_cache = {}


def load(name):
    if name not in _cache:
        d = np.load(f"shots/{name}.npz")
        _cache[name] = (d["t"], d["frames"])
    return _cache[name]


def pick(name, spec):
    """Frame indices from '12,69' or times '2.4s,3s'; -1 is the last frame."""
    t, F = load(name)
    out = []
    for s in spec.split(","):
        s = s.strip()
        if s.endswith("s"):
            out.append(int(np.argmin(np.abs(t - float(s[:-1])))))
        else:
            i = int(s)
            out.append(i if i >= 0 else len(F) + i)
    return out


def oled_svg(frame, label):
    """One frame as vector pixels: a run per row, overlapped a hair so no
    seams show between rows in a PDF viewer."""
    runs = []
    for y in range(64):
        row = frame[y]
        x = 0
        while x < 128:
            if row[x]:
                x0 = x
                while x < 128 and row[x]:
                    x += 1
                runs.append(f"M{x0} {y}h{x - x0 + .03:g}v1.04h-{x - x0 + .03:g}z")
            else:
                x += 1
    return (f'<svg viewBox="0 0 128 64" role="img" aria-label="{html.escape(label)}" '
            f'preserveAspectRatio="xMidYMid meet"><rect width="128" height="64" class="px-off"/>'
            f'<path class="px-on" d="{"".join(runs)}"/></svg>')


# ---------------------------------------------------------------- 5x7 font

def glcd_font():
    src = open(FONT_C, encoding="utf-8").read()
    body = src[src.index("font[] PROGMEM = {") :]
    body = body[: body.index("};")]
    return [int(b, 16) for b in re.findall(r"0x[0-9A-Fa-f]{2}", body)]


FONT = glcd_font()


def pixel_text(text, cls="wordmark", dot=0.84, label=None):
    """Text in the pet's own 5x7 font, each pixel a rounded dot."""
    cells = []
    for i, ch in enumerate(text):
        base = ord(ch) * 5
        for col in range(5):
            bits = FONT[base + col]
            for row in range(8):
                if bits >> row & 1:
                    x, y = i * 6 + col, row
                    cells.append(f'<rect x="{x + (1 - dot) / 2:g}" y="{y + (1 - dot) / 2:g}" '
                                 f'width="{dot}" height="{dot}" rx="{dot * .22:.3f}"/>')
    w = len(text) * 6 - 1
    return (f'<svg class="{cls}" viewBox="0 0 {w} 8" role="img" '
            f'aria-label="{html.escape(label or text)}">{"".join(cells)}</svg>')


# ---------------------------------------------------------------- figures

def figure(target, spec):
    """<!--FIG name | frames | title | caption | options-->"""
    parts = [p.strip() for p in spec.split("|")]
    name, frames, title, caption = parts[:4]
    opts = dict(o.split("=", 1) if "=" in o else (o, "1")
                for o in (parts[4].split() if len(parts) > 4 else []))
    t, F = load(name)
    idx = pick(name, frames)
    cls = "fig" + (" " + opts["class"] if "class" in opts else "")
    cap = f'<figcaption><b>{title}</b> {caption}</figcaption>'
    invert = float(opts.get("invert", 0))

    def frame(i):
        f = F[i]
        if "rows" in opts:
            lo, hi = map(int, opts["rows"].split("-"))
            f = f.copy()
            f[:lo] = 0
            f[hi + 1:] = 0
        if invert and int(t[i] / invert) % 2 == 1:
            f = f ^ 1
        return f

    if target == "web":
        gif = opts.get("gif", name if len(idx) > 1 else "")
        if gif:
            lazy = "" if "bare" in opts else ' loading="lazy"'
            art = (f'<img src="shots/{gif}.gif" alt="{html.escape(title)}: real capture" '
                   f'width="512" height="256"{lazy}>')
        else:
            art = oled_svg(frame(idx[-1] if len(idx) > 1 else idx[0]), title)
        if "bare" in opts:
            return art
        return f'<figure class="{cls}"><div class="oled">{art}</div>{cap}</figure>'

    if "bare" in opts:
        return oled_svg(frame(idx[0]), title)

    if len(idx) == 1:
        return f'<figure class="{cls}"><div class="oled">{oled_svg(frame(idx[0]), title)}</div>{cap}</figure>'
    t0 = t[idx[0]]
    cells = "".join(
        f'<div class="cell"><div class="oled">{oled_svg(frame(i), title)}</div>'
        f'<span class="tc">{t[i] - t0:.2f} s</span></div>' for i in idx)
    return (f'<figure class="{cls} film" style="--n:{len(idx)}">'
            f'<div class="frames">{cells}</div>{cap}</figure>')


def listen_frames():
    """Listening, quiet to loud: the smallest, a middling and the biggest open eyes."""
    t, F = load("listening")
    heights = [int(np.ptp(np.nonzero(f)[0]) + 1) if f.any() else 0 for f in F]
    cand = sorted((h, i) for i, h in enumerate(heights) if t[i] > 0.3 and h >= 30)
    lo, hi = cand[0], cand[-1]
    mid = min(cand, key=lambda c: abs(c[0] - (lo[0] + hi[0]) / 2))
    return ",".join(str(i) for i in sorted({lo[1], mid[1], hi[1]}))    # in time order


def sn_hex():
    """The start of a real SN reply (the love face), and a piece of page 3."""
    t, F = load("em_love")
    f = F[-1]
    data = bytearray(1024)
    for y in range(64):
        for x in range(128):
            if f[y, x]:
                data[(y // 8) * 128 + x] |= 1 << (y % 8)
    h = data.hex()
    page3 = h[2 * (384 + 24): 2 * (384 + 64)]
    lines = [
        f'SN {h[:64]}…  <span class="c">// page 0: the top 8 rows, dark</span>',
        '   <span class="c">… page 3, from column 24:</span>',
        f"   {page3}",
        '   <span class="c">… 2,048 digits in all</span>',
    ]
    return "\n".join(lines)


def build(target):
    import calendar
    src = open("handbook.src.html", encoding="utf-8").read()
    utc = calendar.timegm((2026, 9, 29, 10, 42, 5, 0, 0, 0)) - 19800
    src = src.replace("{{TM}}", f"TM {utc} 19800").replace("{{SNHEX}}", sn_hex()).replace("{{LISTEN}}", listen_frames())
    src = src.replace("{{FONTS}}", WEB_FONTS if target == "web" else PAPER_FONTS)
    src = src.replace("{{TARGET}}", target)
    src = src.replace("{{WORDMARK}}", pixel_text("DESKTOP-PET", label="Desktop-pet"))
    src = re.sub(r"\{\{PIXEL:([^}]*)\}\}", lambda m: pixel_text(m.group(1), "pixlabel"), src)
    src = re.sub(r"<!--FIG(.*?)-->", lambda m: figure(target, m.group(1)), src, flags=re.S)
    src = re.sub(r"<!--WEB-->(.*?)<!--/WEB-->", lambda m: m.group(1) if target == "web" else "",
                 src, flags=re.S)
    src = re.sub(r"<!--PAPER-->(.*?)<!--/PAPER-->", lambda m: m.group(1) if target != "web" else "",
                 src, flags=re.S)
    return src


if __name__ == "__main__":
    import os
    os.makedirs("site", exist_ok=True)
    web, paper = build("web"), build("paper")
    open("site/index.html", "w", encoding="utf-8").write(web)
    open("paper.html", "w", encoding="utf-8").write(paper)
    print(f"web {len(web) // 1024} KB, paper {len(paper) // 1024} KB")
