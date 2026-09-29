# The enclosure

A head for the face: a soft cube, the screen behind a black visor, the Black Pill up on a stand inside, and a touch pad under the top so you pet it where you'd pet a pet.

![The pet in its head](renders/hero.png)

| ![From the back, with the USB-C cable in](renders/back.png) | ![Cut open](renders/cutaway.png) |
| --- | --- |
| The USB-C port in the back, lined up with the Black Pill's connector | Inside: the screen behind the visor, the Black Pill on its stand with its jumper plugs hanging underneath, the touch pad under the top |

| ![The base](renders/tray.png) | ![The base with the Black Pill in](renders/tray-board.png) |
| --- | --- |
| The base: a spine for the Black Pill to sit on and four posts that clip over its edges | The board on it, chip side up, its plugs hanging free between the posts |

![Exploded](renders/exploded.png)

**66 × 70 × 58 mm.** Everything is modelled in [`model.py`](model.py) from the WeAct board drawing and the OLED module's dimensions, and [`check.py`](check.py) fits it all together with stand-ins for the boards, a jumper plug on every one of the Black Pill's 40 pins, and a USB-C plug pushed fully home: nothing overlaps.

## The Black Pill's stand

The Black Pill goes in the way it's wired: chip, buttons and USB-C on top, headers pointing down, every jumper plug hanging underneath. So it sits 23.5 mm up:

- **A spine** under the middle of the board, between the two rows of plugs, carries it. It stops short of the USB-C's metal tabs that come through the board.
- **A shoulder** at the spine's far end takes the push when the USB cable goes in, and the back wall takes the pull when it comes out.
- **Four springy posts**, outside the rows of plugs, clip over the board's long edges so it can't lift or slide sideways.
- The plugs end 7 mm above the floor, which leaves room for the wires to bend out to the sides.

Nothing reaches over the board's top except those four clips, so buttons, the USB-C and an SWD header, if yours has one, all stay clear.

## What to print

The STL files are already turned the way they go on the bed. **No supports** on any of them.

| Part | File | On the bed | Colour | Time* |
| --- | --- | --- | --- | --- |
| Head | [`stl/shell.stl`](stl/shell.stl) | Upside down, top on the bed | Any (white in the renders) | ~3 h |
| Visor | [`stl/visor.stl`](stl/visor.stl) | Face down | **Black**, so only the lit pixels show | ~15 min |
| Base | [`stl/base.stl`](stl/base.stl) | Flat | Any | ~1.5 h |

\*Rough, at 0.2 mm layers.

PLA or PETG, 0.4 mm nozzle, 0.2 mm layers, 15% infill. The walls are 2.4 mm, six lines, so they print solid anyway. The head's top edge is rounded where it shows and turns into a 45° chamfer where it meets the bed, so the first layers don't droop.

## What else you need

- **A TTP223 touch sensor module** (the small 15 × 11 mm one). It sits under the thinned top and senses your finger through 1 mm of plastic. For the bigger 24 × 24 mm round one, set `TOUCH_L` and `TOUCH_W` to 24 in `model.py`.
- **3 more jumper wires** for it.
- A drop of superglue (or a soldering iron) for the visor's four pegs.
- The new firmware, which reads the touch pad on **B0** (upload it as usual; with the base out, KEY and NRST are right there on top of the board).

## Putting it together

1. **Print the visor first** and check the screen before printing anything else. Press the OLED onto its four pegs, glass into the recess, then power the pet and double tap to the **Status** screen. The whole top line (*Desktop pet … USB*) and bottom line (*Up …*) should show through the window. If a line is cut off, measure roughly how far and change `AA_UP` in `model.py` (positive moves the window up). Then take the OLED off again.
2. **Screen into the head.** Push the visor into the face from the outside. Reach in through the open bottom, put the OLED onto the pegs and glue or melt each peg tip. The screen is now clamped through the wall and can't move.
3. **Touch pad under the top.** Pad side up (the side without the chip), pins toward the back, into the pocket in the middle of the top. A bit of tape or glue holds it. Don't touch the head while the pet powers up: the TTP223 measures its "not touched" level then.
4. **Black Pill onto its stand,** with its jumpers already plugged in. USB end toward the back of the base (the end with the finger notch), chip side up. Lower it between the four posts, slide it forward against the shoulder, and press down until all four clips snap over its edges. Bend the wires out to the sides under the board.
5. **The loose modules** go on the floor: the HC-05 on the left, the ESP-01 and its regulator on the right, over the vents.
6. **Wire everything** as in the main README, and the touch pad as below.
7. **Base into the head,** USB end at the back, until both catches click into the side windows. To open it again, press both catches in through those windows and pull the base down by the notch at the back.

### The touch pad's wiring

| TTP223 | Black Pill |
| --- | --- |
| VCC | 3.3 |
| GND | G |
| I/O | B0 |

Touching it does everything KEY does: tap to pet, three taps for hearts, double tap for the next screen, hold to go back. KEY still works too. The pad isn't on KEY's own pin (A0), because the bootloader reads A0 at every reset, and a TTP223 holds its output low while nobody's touching it.

## Changing it

All the numbers are at the top of [`model.py`](model.py): the head's size and radii, the wall thickness, the clearance between parts (`FIT`), the OLED's hole spacing and the window, how high the Black Pill sits (`BP_LIFT`), the USB-C port and the touch pad.

```bash
pip install manifold3d trimesh numpy scipy
python model.py      # the STLs, into stl/
python check.py      # fits everything together and lists any overlap, writes assembly.glb
node render.mjs      # the pictures, into renders/ (uses headless Chrome)
```
