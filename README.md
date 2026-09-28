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

WeAct Black Pills ship with the **WeAct HID bootloader** in the first 16 KB of flash. It shows up in Windows as a plain HID device (`0483:572A`), so it needs no driver and no Zadig, and it sidesteps the chip's built-in USB bootloader, which often fails with *Device Descriptor Request Failed*. The project is linked to start at `0x08004000`, right after it.

1. **Install PlatformIO.** In VS Code, open Extensions, search for *PlatformIO IDE*, install it and let it finish its first time setup.
2. **Get the uploader.** Download [`WeAct_HID_Flash-CLI.exe`](https://github.com/WeActStudio/WeActStudio.MiniSTM32F4x1/tree/master/Soft/WeAct_HID_FW_Bootloader) from WeAct's repo and save it as `tools/WeAct_HID_Flash-CLI.exe` in this folder. It's WeAct's own tool, closed source, so it isn't committed here.
3. **Open the project.** File > Open Folder > `Desktop-pet`. PlatformIO fetches the STM32 toolchain and the Adafruit display libraries on its own.
4. **Enter the bootloader.** A fresh board is already in it. Once your code is on it, hold **KEY**, tap NRST, wait for the blue LED to blink, then release KEY.
5. **Upload.** Click the → arrow in the blue status bar.
6. The eyes appear. If they don't, tap NRST.

The default build targets the F401CC and also runs on an F411CE. For the F411's full 100 MHz, read the square chip and, if it says `STM32F411CE`, change `default_envs` in `platformio.ini` to `blackpill_f411ce`.

Serial output goes over the same USB cable (the plug icon in the status bar opens the monitor at 115200 baud) and prints `Desktop pet is awake` on boot.

## Troubleshooting

| Symptom | Likely cause |
| --- | --- |
| Blue LED blinks fast, screen blank | OLED not found on I2C. Check the four wires, then try `0x3D` for `OLED_ADDR` in `src/main.cpp`. |
| Upload can't find the board | Board isn't in the HID bootloader. Hold KEY, tap NRST, release KEY when the LED blinks. |
| Upload says the command isn't found | `tools/WeAct_HID_Flash-CLI.exe` is missing (step 2). |
| No HID device at all, even holding KEY | The HID bootloader was erased (a DFU or ST-Link upload to `0x08000000` does that). Reflash it from WeAct's repo, or drop `board_build.flash_offset` and upload over DFU or ST-Link instead. |
| Don't install WinUSB over *WeAct Studio HID Bootloader* in Zadig | The uploader talks to it as HID. If you already did, uninstall that device in Device Manager and replug. |
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
