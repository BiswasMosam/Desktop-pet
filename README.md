# Desktop-pet

A tiny desk companion with a face. Two rounded eyes on a 0.96" OLED that glance around, blink, light up when you pet it, and doze off when you ignore it.

Step 1 of the build: **the face**. The Black Pill's onboard KEY button stands in for a touch sensor for now.

## What it does

| Mood | When | What you see |
| --- | --- | --- |
| **Normal** | Default | Eyes glance somewhere random every 1.5 to 4 s and blink every 2.5 to 6 s |
| **Happy** | You press KEY | Eyes centre, lift slightly and curve into happy `^ ^` arcs for 2.5 s |
| **Sleepy** | 30 s with no pets | Eyes droop into slits and a pair of `z`s bobs in the corner |

Pressing KEY while it sleeps wakes it straight into Happy.

Everything moves by easing the current eye position and height toward a target every frame (about 50 fps), so glances glide, blinks snap shut, and falling asleep feels like a slow droop rather than a jump cut.

## Hardware

- **WeAct Black Pill**, STM32F401CC or STM32F411CE
- **0.96" SSD1306 OLED**, 128x64, I2C
- USB-C cable

### Wiring

| OLED | Black Pill |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SCL | B6 |
| SDA | B7 |

The KEY button (PA0) and the blue LED (PC13) are already on the board.

## Getting it running

1. **Install PlatformIO.** In VS Code, open Extensions, search for *PlatformIO IDE*, install it and let it finish its first time setup.
2. **Open the project.** File > Open Folder > `Desktop-pet`. PlatformIO fetches the STM32 toolchain and the Adafruit display libraries on its own.
3. **Check your chip.** Read the square chip in the middle of the board. `STM32F401CC` works as is. For `STM32F411CE`, change `default_envs` at the top of `platformio.ini` to `blackpill_f411ce`.
4. **Install the USB driver (Windows, once).** Put the board in bootloader mode (next step), open [Zadig](https://zadig.akeo.ie/), pick **STM32 BOOTLOADER**, choose **WinUSB** and click Install Driver.
5. **Enter bootloader mode.** Hold BOOT0, tap NRST, release BOOT0.
6. **Upload.** Click the → arrow in the blue status bar.
7. **Tap NRST** once the upload finishes. The eyes appear.

You repeat steps 5 to 7 for every upload. The board has no way to jump into the bootloader on its own.

Serial output goes over the same USB cable (the plug icon in the status bar opens the monitor at 115200 baud) and prints `Desktop pet is awake` on boot.

## Troubleshooting

| Symptom | Likely cause |
| --- | --- |
| Blue LED blinks fast, screen blank | OLED not found on I2C. Check the four wires, then try `0x3D` for `OLED_ADDR` in `src/main.cpp`. |
| *STM32 BOOTLOADER* never shows up | Bootloader detection on the F4x1 Black Pill is known to be unreliable. Hold BOOT0 and **replug the USB cable** instead of tapping NRST, and try a couple of times. |
| Upload says `No DFU capable USB device available` | Board isn't in bootloader mode, or the WinUSB driver from step 4 isn't installed. |
| Garbage or noise on screen | Module may be an SH1106 (common on 1.3" boards), which needs a different library. |

## How the code is laid out

`src/main.cpp`, top to bottom:

- **Eye shape:** size, corner radius and gap. Change these to restyle the face.
- **Mood and timing:** the three moods, the sleep timeout and how long a pet lasts.
- **Drawing:** `drawEye` draws one rounded rectangle and, when happy, cuts a circle out of its bottom to make the arc. `drawFace` places both eyes and the sleeping `z`s.
- **Behaviour:** `handleButton` catches the press edge, `updateMood` moves between moods, `updateTargets` picks where the eyes should be and eases toward it.

All timers compare with `reached(now, t)` instead of `now > t`, so the pet keeps blinking after `millis()` wraps around at about 49.7 days.

## Stack

C++ on the Arduino framework (STM32duino), built with PlatformIO. Adafruit SSD1306 and Adafruit GFX for the display.

## Next

- A real touch sensor (TTP223) in place of the KEY button
- More moods and reactions
