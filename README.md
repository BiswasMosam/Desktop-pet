# Desktop-pet

A tiny desk companion with a face. Two rounded eyes on a 0.96" OLED that glance around, blink, fidget, light up when you pet it and doze off when you ignore it. Plugged into the PC it becomes the physical face of [Aminal](https://github.com/BiswasMosam/Aminal), my voice assistant: its eyes listen, think and talk along with it, and it shows the time, the weather, timers and reminders.

It runs on its own too. Unplug it from the PC and it carries on being a pet, keeping the clock it was last given.

## What it does

### The face

| Expression | When | What you see |
| --- | --- | --- |
| **Idle** | Default | Glances around every 1.5 to 4 s, blinks, and every 15 to 40 s fidgets: a look around, a little hop, a suspicious squint or a double blink |
| **Happy** | You tap KEY | Eyes lift and curve into `^ ^` arcs for 2.5 s |
| **In love** | Three taps in a row | Beating hearts |
| **Sleepy** | 30 s with no attention (10 s after 11 pm) | Eyes droop into slits, `z`s bob in the corner, and the time shows small in the other |
| **Listening** | Aminal is listening | Eyes wide open, swelling with your voice |
| **Thinking** | Aminal is working it out | Looking up and off to one side, three dots filling in |
| **Speaking** | Aminal is talking | Eyes lifted, and a mouth that opens with Aminal's voice |

Aminal can also make it pull a face on request: happy, love, surprised, sad, angry, wink or sleepy.

Everything moves by easing the current eye position, width and height toward a target every frame, so glances glide, blinks snap shut and falling asleep is a slow droop rather than a jump cut.

### The other screens

- **Clock:** big time, the date, and a hairline filling across the minute.
- **Weather:** an animated icon (turning sun, crescent moon at night, falling rain, drifting snow, flickering lightning, rolling fog), the temperature, today's high and low, and the place.
- **Timer:** the countdown with a bar draining under it. It blinks while paused, and when it runs out the whole screen flashes "Time's up" until you tap it.
- **Status:** which link it's on, what Aminal is doing, and how fresh the time and weather are.

Reminders, mail and what's next on the calendar slide down as a card over whatever is on screen. Ask Aminal for the weather or a timer and the pet flips to that screen while it answers.

### The button

KEY (PA0) is the only input:

| Press | On the face | Anywhere else |
| --- | --- | --- |
| **Tap** | Pet it | Next screen |
| **Double tap** | Next screen | Next screen |
| **Hold** | Back to the face | On the timer: pause or resume it. Elsewhere: back to the face |

A tap also dismisses a card. A screen you flipped to returns to the face after 20 s.

## Hardware

- **WeAct Black Pill**, STM32F401CC or STM32F411CE
- **0.96" SSD1306 OLED**, 128x64, I2C
- **HC-05** Bluetooth module (optional): a wireless link to Aminal
- **ESP-01** WiFi module (coming): time and weather straight from the internet
- USB-C cable

### Wiring

| OLED | Black Pill |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SCL | B6 |
| SDA | B7 |

| HC-05 | Black Pill |
| --- | --- |
| VCC | 5V |
| GND | GND |
| TXD | A3 |
| RXD | A2 |

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

### Connecting it to Aminal

Nothing to set up: Aminal starts `pet_bridge.py` beside itself, which finds the pet on USB by its identity (`0483:5740`) and starts talking. With Aminal closed the bridge can be run on its own and still gives the pet the time, weather and timers:

```bash
python3.12 pet_bridge.py                  # in the Aminal folder
python3.12 pet_bridge.py --snap pet.png   # save exactly what the OLED shows
python3.12 pet_bridge.py --say "EM love"  # send one line
```

### Bluetooth

1. **Wire the HC-05** (the board with 6 pins in a row, not the ESP-01 with 8 in two rows): VCC to **5V**, GND to G, TXD to A3, RXD to A2. Its red LED blinks fast when it's powered and ready to pair. No light at all is almost always a loose VCC or GND wire.
2. **Pair it with the PC**, not a phone: the HC-05 holds one connection at a time. Windows 11's own *Add device* list often shows it as *Unknown device*; press Win+R and run `DevicePairingWizard`, the old wizard, which shows it as **HC-05**. PIN `1234` (or `0000`).
3. **Find its outgoing COM port.** Pairing makes two. The outgoing one is listed under Bluetooth > More Bluetooth settings > COM Ports as *Outgoing 'HC-05'*; it's the one whose device ID carries the module's address.
4. **Set `PET_PORT=COMx`** in Aminal's `.env`.

USB always wins. The bridge tries the cable first and the Bluetooth port every 15 s while the pet isn't plugged in (opening the port of a pet that's off makes Windows try for seconds), and plugging the cable back in moves the link to USB by itself. Running the pet from a phone charger or power bank is how it goes wireless: within 15 s of losing the cable, Aminal reaches it over the air. Everything works over Bluetooth, just slower: a full `SN` snapshot takes about 2 s at 9600 baud.

## The protocol

One short line per message, the same on USB and Bluetooth, simple enough to type into a serial monitor:

| Line | Meaning |
| --- | --- |
| `HI` | Who are you? The pet answers `PET desktop-pet 1` |
| `PG` | Heartbeat, answered `PO`. Eight silent seconds and the pet is on its own again |
| `ST idle\|listening\|thinking\|speaking\|off` | What Aminal is doing |
| `LV 0-100` | Voice level, for the eyes and the mouth |
| `TM <unix> <utc offset s>` | The time |
| `WX <temp> <wmo code> <hi> <lo> <place>` | The weather |
| `TI <left s> <total s> <paused 0/1> <label>` or `TI -` | The timer, or none |
| `AL <secs> <title>\|<text>` or `AL -` | A card, or take it down |
| `EM <happy\|love\|surprised\|sad\|angry\|wink\|sleepy> [ms]` | Pull a face |
| `GO <face\|clock\|weather\|timer\|status> [secs]` | Show a screen |
| `SN` | Answered `SN <hex>`: the 1024-byte frame buffer, exactly what's on the OLED |

The pet sends events back: `EV pet`, `EV next <screen>`, `EV hold`, `EV hold timer`, `EV dismiss`.

## Troubleshooting

| Symptom | Likely cause |
| --- | --- |
| Blue LED blinks fast, screen blank | OLED not found on I2C. Check the four wires, then try `0x3D` for `OLED_ADDR` in `src/main.cpp`. |
| Upload can't find the board | Board isn't in the HID bootloader. Hold KEY, tap NRST, release KEY when the LED blinks. |
| Upload says the command isn't found | `tools/WeAct_HID_Flash-CLI.exe` is missing (step 2). |
| No HID device at all, even holding KEY | The HID bootloader was erased (a DFU or ST-Link upload to `0x08000000` does that). Reflash it from WeAct's repo, or drop `board_build.flash_offset` and upload over DFU or ST-Link instead. |
| Don't install WinUSB over *WeAct Studio HID Bootloader* in Zadig | The uploader talks to it as HID. If you already did, uninstall that device in Device Manager and replug. |
| Garbage or noise on screen | Module may be an SH1106 (common on 1.3" boards), which needs a different library. |
| Eyes never react to Aminal | Status screen says `Link none`: the bridge isn't running, or something else (a serial monitor) has the port open. |

## How the code is laid out

| File | What's in it |
| --- | --- |
| `src/pet.h` | The shared `World`: time, weather, timer, Aminal's state, the current card |
| `src/main.cpp` | Setup, the frame loop, the button, switching screens |
| `src/face.cpp` | Eyes, moods, fidgets, and the listening, thinking and speaking faces |
| `src/screens.cpp` | Clock, weather icons, timer, status, and the alert card |
| `src/link.cpp` | The protocol, read from USB and the HC-05 alike |

All timers compare with `reached(now, t)` instead of `now > t`, so the pet keeps blinking after `millis()` wraps around at about 49.7 days.

## Stack

C++ on the Arduino framework (STM32duino), built with PlatformIO. Adafruit SSD1306 and Adafruit GFX for the display, FreeSans Bold for the big digits.

## Next


- The ESP-01 for time and weather when the PC is off, and a link to Aminal over WiFi
- A real touch sensor (TTP223) in place of the KEY button
