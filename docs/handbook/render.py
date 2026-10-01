"""Turn the captured frame buffers into cyan OLED GIFs and PNGs."""
import numpy as np
from PIL import Image

SCALE = 4
LIT = (61, 232, 255)        # the cyan of this OLED
OFF = (0, 0, 0)
OUT = "site/shots"


def image(frame, invert=False):
    f = frame ^ 1 if invert else frame
    img = Image.fromarray(f.astype(np.uint8), "P")      # index 0 off, 1 lit
    img.putpalette(list(OFF) + list(LIT))
    return img.resize((128 * SCALE, 64 * SCALE), Image.NEAREST)


def load(name):
    d = np.load(f"shots/{name}.npz")
    return d["t"], d["frames"]


def gif(name, start=0.0, end=None, out=None, hold_last=0.0, invert_every=None):
    t, F = load(name)
    end = t[-1] + 0.04 if end is None else end
    keep = [(ti, f) for ti, f in zip(t, F) if start <= ti <= end]
    frames, durs = [], []
    for i, (ti, f) in enumerate(keep):
        nxt = keep[i + 1][0] if i + 1 < len(keep) else ti + 0.036
        inv = bool(invert_every) and int(ti / invert_every) % 2 == 1
        key = (f.tobytes(), inv)
        dur = nxt - ti
        if frames and frames[-1][0] == key:
            durs[-1] += dur
        else:
            frames.append((key, f, inv))
            durs.append(dur)
    durs[-1] += hold_last
    imgs = [image(f, inv) for _, f, inv in frames]
    ms = [max(20, int(round(d * 100)) * 10) for d in durs]
    path = f"{OUT}/{out or name}.gif"
    imgs[0].save(path, save_all=True, append_images=imgs[1:], duration=ms,
                 loop=0, optimize=False, disposal=1)
    return path, len(imgs), sum(ms) / 1000


def png(name, at=-1, out=None):
    t, F = load(name)
    path = f"{OUT}/{out or name}.png"
    image(F[at]).save(path, optimize=True)
    return path


if __name__ == "__main__":
    import os
    os.makedirs(OUT, exist_ok=True)
    made = []
    made.append(gif("idle1", 2.4, 14.6, out="idle"))
    for em in ("happy", "surprised", "sad", "angry", "wink"):
        made.append((png(f"em_{em}"),))
    made.append(gif("em_love", 0.55, 1.6))
    made.append(gif("listening", 0.3))
    made.append(gif("thinking", 0.1))
    made.append(gif("speaking", 0.2))
    for name in ("sleepy", "headphones",
                 "dance", "bars", "watch", "jump", "timer", "timer_paused",
                 "card_long"):
        made.append(gif(name))
    for name in ("partly", "clear", "overcast", "fog", "rain", "snow", "storm", "night"):
        made.append(gif(f"wx_{name}"))
    made.append(gif("card", hold_last=2.5))
    for name in ("code_read", "code_type", "code_music", "game", "selfie"):
        made.append(gif(name))
    made.append(gif("card_open", hold_last=2.5))

    # The printed head, from the enclosure's own renders
    for name in ("hero", "side", "reader"):
        src = f"../../enclosure/renders/{name}.png"
        if os.path.exists(src):
            path = f"{OUT}/head_{name}.png"
            Image.open(src).convert("RGB").resize((1200, 900), Image.LANCZOS).save(path, optimize=True)
            made.append((path,))
    made.append(gif("timesup", invert_every=0.5))
    made.append((png("clock"),))
    made.append((png("status"),))
    for m in made:
        print(*m, f"{os.path.getsize(m[0]) // 1024} KB")

    # A contact sheet: the last frame of every capture, for a quick look
    names = sorted(n[:-4] for n in os.listdir("shots") if n.endswith(".npz"))
    cols = 6
    sheet = Image.new("RGB", (cols * 260, ((len(names) + cols - 1) // cols) * 140), (40, 40, 40))
    from PIL import ImageDraw
    draw = ImageDraw.Draw(sheet)
    for i, n in enumerate(names):
        t, F = load(n)
        im = image(F[len(F) * 2 // 3]).convert("RGB").resize((256, 128), Image.NEAREST)
        x, y = (i % cols) * 260 + 2, (i // cols) * 140 + 2
        sheet.paste(im, (x, y))
        draw.text((x + 2, y + 128), n, fill=(255, 255, 255))
    sheet.save("contact.png")
