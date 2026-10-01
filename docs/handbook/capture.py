"""Record every face and screen of the pet as real frame-buffer captures.

Each sequence is saved as shots/<name>.npz: t (seconds) and frames (n,64,128).
The time and weather are example values, set through the normal protocol.
"""
import calendar
import math
import random
import sys
import time

import numpy as np

from pet import Pet

OFFSET = 19800  # IST
random.seed(7)
pet = Pet()
print(pet.hello(), flush=True)
time.sleep(1.8)                         # the hello face passes


def local_time(h, m, s=5):
    """TM line that makes the pet's clock read h:m on 29 Sep 2026."""
    utc = calendar.timegm((2026, 9, 29, h, m, s, 0, 0, 0)) - OFFSET
    return f"TM {utc} {OFFSET}"


def save(name, frames):
    t = np.array([f[0] for f in frames])
    a = np.array([f[1] for f in frames], dtype=np.uint8)
    np.savez_compressed(f"shots/{name}.npz", t=t, frames=a)
    print(f"{name}: {len(frames)} frames, {len(frames) / max(t[-1], 1e-3):.1f} fps",
          flush=True)


def still(name, settle=0.6):
    time.sleep(settle)
    save(name, [(0.0, pet.snap())])


def rec(name, seconds, feed=None):
    save(name, pet.record(seconds, feed))


def reset():
    for line in ("MD -", "AC -", "SF -", "ST idle", "AL -", "TI -", "GO face"):
        pet.send(line)
    time.sleep(0.3)


def voice(t):
    """A talking voice level: syllables on a slow phrase envelope."""
    phrase = 0.7 + 0.3 * math.sin(t * 1.3)
    syll = abs(math.sin(t * 6.0 + math.sin(t * 2.1) * 2)) ** 0.6
    return int(max(0, min(100, 110 * phrase * syll + random.uniform(-6, 6))))


class Feeder:
    """Sends lines on a schedule while frames are recorded."""
    def __init__(self, every, make):
        self.every, self.make, self.next = every, make, 0.0

    def __call__(self, t):
        if t >= self.next:
            self.next = t + self.every
            for line in self.make(t):
                pet.send(line)


def bars(t):
    """A plausible music spectrum: heavy bass, a moving mid, airy top."""
    beat = max(0.0, 1 - ((t % 0.5) / 0.18))
    out = []
    for i in range(16):
        base = 12.5 - i * 0.55 + 2.2 * math.sin(t * 3.1 + i * 0.7)
        if i < 4:
            base += 3.5 * beat
        base += random.uniform(-1.6, 1.6)
        out.append(max(0, min(15, int(round(base)))))
    return "VZ " + "".join(f"{v:x}" for v in out)


def which(names):
    return len(sys.argv) < 2 or any(n in sys.argv[1:] for n in names)


pet.send(local_time(10, 42))
pet.send("WX 29 2 31 25 Mumbai")
reset()

# ---------- The face on its own ----------
if which(["idle"]):
    for part in (1, 2):
        pet.send("GO face")                      # a wake, so it doesn't doze off
        time.sleep(0.4)
        rec(f"idle{part}", 26)

if which(["emotes"]):
    pet.send("GO face")
    for em in ("happy", "love", "surprised", "sad", "angry", "wink"):
        pet.send(f"EM {em} 4000")
        rec(f"em_{em}", 1.6)
        time.sleep(0.3)

if which(["sleepy"]):
    pet.send(local_time(23, 40))
    pet.send("EM sleepy 6000")
    time.sleep(1.2)
    rec("sleepy", 2.5)
    pet.send(local_time(10, 42))
    pet.send("GO face")

# ---------- With Aminal ----------
if which(["aminal"]):
    pet.send("ST listening")
    rec("listening", 4, Feeder(0.05, lambda t: [f"LV {voice(t)}"]))
    pet.send("ST thinking")
    rec("thinking", 3.5)
    pet.send("ST speaking")
    rec("speaking", 4, Feeder(0.05, lambda t: [f"LV {voice(t + 3)}"]))
    pet.send("ST idle")
    time.sleep(0.8)

# ---------- Music and films ----------
if which(["music"]):
    pet.send("MD music headphones")
    time.sleep(1.0)
    rec("headphones", 5, Feeder(0.5, lambda t: ["BE"]))
    pet.send("MD music dance")
    time.sleep(0.6)
    rec("dance", 7, Feeder(0.5, lambda t: ["BE"]))
    pet.send("MD music bars")
    feed = Feeder(0.08, lambda t: [bars(t)] + (["BE"] if (t % 0.5) < 0.08 else []))
    rec("bars", 5, feed)
    pet.send("MD -")
    time.sleep(0.5)

if which(["watch"]):
    pet.send("MD watch")
    time.sleep(0.5)
    rec("watch", 9)
    pet.send("JS")
    rec("jump", 1.6)
    pet.send("MD -")
    time.sleep(0.5)

# ---------- Coding and gaming ----------
if which(["code"]):
    pet.send("AC code")
    time.sleep(1.2)
    rec("code_read", 6)
    pet.send("AC code typing")
    time.sleep(0.8)
    rec("code_type", 4)
    pet.send("MD music")
    time.sleep(0.8)
    rec("code_music", 4, Feeder(0.5, lambda t: ["BE"]))
    pet.send("MD -")
    pet.send("AC -")
    time.sleep(0.5)

if which(["game"]):
    pet.send("AC game")
    time.sleep(1.2)
    fired = []
    rec("game", 5, lambda t: (pet.send("JS"), fired.append(t)) if t >= 2.5 and not fired else None)
    pet.send("AC -")
    time.sleep(0.5)

# ---------- A selfie ----------
if which(["selfie"]):
    plan = [(0.0, "SF ready"), (2.2, "SF count 2100"), (4.9, "SF shot"), (8.6, "SF -")]
    sent = []

    def selfie(t):
        for at, line in plan:
            if t >= at and line not in sent:
                sent.append(line)
                pet.send(line)
    rec("selfie", 8.5, selfie)
    pet.send("SF -")
    time.sleep(0.5)

# ---------- The card ----------
if which(["cardui"]):
    pet.send("GO face")
    time.sleep(0.4)
    pet.send("AL 3 Folder open|Tap again to close it")
    rec("card_open", 1.2)
    pet.send("AL -")
    time.sleep(0.3)

# ---------- The screens ----------
if which(["screens"]):
    pet.send(local_time(10, 42, 36))
    pet.send("GO clock 60")
    rec("clock", 1.0)

    pet.send(local_time(13, 5))
    for code, temp, words in ((2, 29, "partly"), (0, 33, "clear"), (3, 27, "overcast"),
                              (45, 22, "fog"), (61, 24, "rain"), (71, -2, "snow"),
                              (95, 25, "storm")):
        pet.send(f"WX {temp} {code} {temp + 3} {temp - 5} Mumbai")
        pet.send("GO weather 60")
        rec(f"wx_{words}", 2.4 if code in (0, 2) else 1.4)
    pet.send(local_time(21, 30))
    pet.send("WX 26 0 31 25 Mumbai")
    pet.send("GO weather 60")
    rec("wx_night", 2.4)
    pet.send(local_time(10, 42))
    pet.send("WX 29 2 31 25 Mumbai")

    pet.send("TI 754 1500 0 Pasta")
    pet.send("GO timer 60")
    rec("timer", 2.0)
    pet.send("TI 754 1500 1 Pasta")
    rec("timer_paused", 2.0)
    pet.send("TI -")

    pet.send("GO status 60")
    still("status", 0.3)
    pet.send("GO face")

if which(["cards"]):
    pet.send("GO face")
    time.sleep(0.4)
    pet.send("AL 12 Reminder|Water the plants before the sun goes down")
    rec("card", 1.2)
    pet.send("AL -")
    time.sleep(0.3)
    pet.send("AL 12 Mail|Your order has shipped and should arrive on Thursday before noon, no signature needed")
    time.sleep(0.2)
    rec("card_long", 6.5)
    pet.send("AL -")
    time.sleep(0.3)
    pet.send("TI 2 300 0 Tea")
    time.sleep(2.6)
    rec("timesup", 2.5)
    pet.send("AL -")

reset()
print("done", flush=True)
