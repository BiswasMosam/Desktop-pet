# Desktop-pet

A tiny desk companion with a face. Two rounded eyes on a 0.96" OLED that glance around, blink, fidget, light up when you pet it and doze off when you ignore it. Plugged into the PC it becomes the physical face of [Aminal](https://github.com/BiswasMosam/Aminal-Public), my voice assistant: its eyes listen, think and talk along with it, and it shows the time, the weather, timers and reminders.

It follows the PC too: it dances to your music, eats popcorn through films, puts on glasses while you code, picks up a controller when you play, and counts down your selfies. And it doesn't need the cable: on a charger it reaches the PC over WiFi, and with the PC off it fetches its own time and weather.

<p>
  <img src="docs/shots/idle.gif" width="32%" alt="Idle: glancing around and blinking">
  <img src="docs/shots/dance.gif" width="32%" alt="Dancing to music, notes floating up">
  <img src="docs/shots/code_type.gif" width="32%" alt="Coding: glasses on, paws typing">
</p>

Real captures from the pet's own frame buffer: idle, dancing, and typing along while you code.

**[The handbook](https://www.mosambiswas.com/Desktop-pet/)** is the full user guide and technical notes: every face, trick and screen, the wiring, the firmware, the protocol and how it hears music. It's also a 25-page [PDF](docs/Desktop-pet-Handbook.pdf).

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

### Music and films

When Aminal hears Spotify playing, the pet joins in, in one of three moods picked at random and rerolled every 25 to 45 s:

| Mood | What you see |
| --- | --- |
| **Headphones** | Headphones on, eyes shut and happy, nodding on every beat, with a peek around now and then |
| **Dancing** | Swaying side to side and hopping on the beat, music notes floating up |
| **Visualizer** | The whole screen becomes 16 bars of what the speakers are actually playing, with falling peak marks |

Put on Netflix, Prime Video or Hotstar and it looks up at the screen with a bucket of **popcorn**, flicking a kernel into its mouth and chewing every few seconds. When a scene suddenly gets loud it jumps, and the popcorn flies out of the bucket.

Aminal decides music or film by the app and the tab title, never by guesswork, so a video call gets no reaction at all. The beats and bars come from Aminal listening to the speakers; without them the pet dances to a steady 115 bpm of its own.

### Coding and gaming

The pet also follows what's in front on the PC:

| In front | What you see |
| --- | --- |
| **Code** (VS Code, Cursor, a JetBrains IDE, a terminal, GitHub or Colab in a browser) | Glasses on, reading along each line and down the page. While you type, two paws tap on a little keyboard and code flies off its ends |
| **A game** (anything from a Steam, Epic, Riot, Xbox, EA, Ubisoft, GOG or Rockstar library, a few by name, or any exclusive full screen 3D app) | Eyes narrowed in focus and darting after the action, a controller in its hands with the buttons getting mashed. When the game gets loud, sparks burst out and the eyes pop wide |

With music playing as well it keeps the coding or gaming face and puts the headphones on over it, nodding on the beat. An editor nobody has touched for five minutes isn't coding, and a few seconds in front are needed before either counts, so alt-tabbing past VS Code changes nothing. Aminal goes by the program, its folder and the window title only, never by what's in the window or what's typed.

### The card

Tap the RFID card on the reader and a hidden folder opens on the PC (`C:\Users\<you>\Vault`, hidden and marked as a system folder so it stays out of sight even with hidden files shown). Tap again and it closes. The first card ever tapped becomes the key; any other card gets a sad face. It works with Aminal closed, since the bridge does it.

A tap is the card arriving after being away for 1.5 s, so a card left lying on the reader counts once. Gestures (double tap, hold to lock) and a "hotel key slot" came first and both glitched: a card resting on the reader drops out of reading for over a second now and then. Hidden is not locked: anyone who types the path can open the folder.

### Selfies

Ask Aminal for a selfie and the pet turns into a camera. While the webcam opens and meters, its lens focuses and searches for you ("smile please!"). As Aminal says "Hold still - three, two, one", the numbers count down inside the lens, then "cheese!". The moment the photo is actually taken, the screen flashes white and the shutter blinks closed and open ("click!"), and it says "developing" until Aminal has the picture ready.

The flash is the real shutter, not a timer: the photo burst starts the instant "three, two, one" ends, and the bridge flashes the pet on exactly that, because the voice takes anywhere from 1.9 to 2.4 s to say it.

### The button

KEY (PA0) is the only input, or a TTP223 touch pad on B0 under the top of the [enclosure](enclosure/), which does exactly the same:

| Press | On the face | Anywhere else |
| --- | --- | --- |
| **Tap** | Pet it | Next screen |
| **Double tap** | Next screen | Next screen |
| **Hold** | Back to the face | On the timer: pause or resume it. Elsewhere: back to the face |

A tap also dismisses a card. A screen you flipped to returns to the face after 20 s.

## Hardware

- **WeAct Black Pill**, STM32F401CC or STM32F411CE
- **0.96" SSD1306 OLED**, 128x64, I2C
- **ESP-01** WiFi module (optional): its own time and weather when the PC is off, and the link to the PC with no cable
- **RC522** RFID reader (optional): tap a card to open a hidden folder
- **TTP223** touch sensor (optional): pet it by touching the top of its head
- USB-C cable

### Wiring

| OLED | Black Pill |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SCL | B6 |
| SDA | B7 |

| ESP-01 | Black Pill |
| --- | --- |
| 3V3 (VCC) | 3V3, with **short direct wires**. **Never 5V**. Long breadboard jumpers made it brown out whenever its radio worked; short ones on the Black Pill's own 3V3 are fine |
| GND | G |
| EN (CH_PD) | A8 (the pet holds it high, which saves a 3V3 pin) |
| TX | A10 |
| RX | A9 |
| RST | B12 (lets the pet restart it) |
| GPIO0 | B13 (lets the pet start its bootloader) |

| RC522 | Black Pill |
| --- | --- |
| 3.3V | 3V3. **Never 5V** |
| GND | G |
| SDA | A4 |
| SCK | A5 |
| MISO | A6 |
| MOSI | A7 |
| RST | B1 (the pet drives it; left floating, the chip can sleep and stop answering) |
| IRQ | not connected |

| TTP223 | Black Pill |
| --- | --- |
| VCC | B14 (it draws microamps, so a pin powers it) |
| GND | B15 |
| I/O | B0 (not A0: the bootloader reads A0 at reset, and the pad's output sits low) |

The board has three 3V3 pins and three G pins (two on the long headers, one each on the small 4-pin header): one pair each for the OLED, the ESP-01 and the RC522. Both 5V pins stay empty.

The ESP-01's pins aren't labelled on top. With the chips facing you and the antenna up, one row holds GND, GPIO2, GPIO0 and RX, the other TX, EN, RST and 3V3; GND and 3V3 sit at opposite corners. GPIO2 stays empty.

The KEY button (PA0) and the blue LED (PC13) are already on the board.

### The enclosure

<img src="enclosure/renders/hero.png" width="48%" alt="The pet in its printed head"> <img src="enclosure/renders/cutaway.png" width="48%" alt="The head cut open">

A 3D-printable head, 66 × 89.5 × 58 mm: a soft cube, the screen clamped behind a black visor, the Black Pill held up by its edges so its jumper plugs and SWD header hang free underneath, its USB-C lined up with a port in the back, the touch pad under the top, and the RFID reader hidden inside the right side, with nothing on the outside to give it away. Three parts, no supports. The STLs, print settings and how to put it together are in [enclosure/](enclosure/).

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

A small bridge on the PC finds the pet on USB by its identity (`0483:5740`) and starts talking. It starts at login and runs whether Aminal is open or not, so the pet follows the music, films, code and games on the PC all day and gets the time, weather and timers; Aminal only adds its listening, thinking and speaking faces. If the bridge isn't running, Aminal starts it.

The cable isn't needed. With only a power cord on the pet, the bridge reaches it over WiFi: the ESP-01 listens on port 7676 and the bridge speaks the same protocol to it as over USB. It learns the pet's address whenever it's plugged in, and searches the home network for it if that address stops answering. USB always wins when it's plugged in. Anyone on the same network can reach that port too, so it's meant for a home network; joining WiFi (`WF`) and the ESP passthrough (`ES`) stay USB only. It can also save exactly what the OLED shows as a PNG (`SN`) or send the pet a single line. Aminal's code is private; its own README has the exact commands.

### WiFi

The ESP-01 keeps its factory **AT firmware**, and the pet drives it with plain text commands: join the network, then one plain-HTTP request to Open-Meteo every 20 minutes. That single reply carries the time (its `Date` header), the UTC offset and today's weather, so a pet running from a charger keeps its clock and forecast with the PC off. Where the weather is for (`LO`, sent by Aminal) and the UTC offset are kept in the Black Pill's flash across power cuts.

It also listens on port **7676** for the bridge, which is how the pet works with only a power cord (above). The module's multi-link mode makes that possible: the weather request goes out on link 4 while the bridge comes in on another. While the bridge is connected over WiFi it sends the time and weather itself, so the pet skips its own request. The pet resets the module at every boot, and again if it stops answering, so a bad power-up can't leave the WiFi stuck.

To connect it, run the bridge's WiFi setup with the pet on USB. It offers the network the PC is on, asks for the password without showing it, and sends both down the USB cable only. The module keeps them and rejoins by itself from then on. The ESP8266 only does **2.4 GHz** networks.

Reflashing the ESP-01 with firmware of its own, through the pet, was tried and failed: its ROM loader restarts on every esptool SYNC. The pet can still talk to it directly for debugging (`ES talk`), and every reset it does reports the ROM's own start-up line (`ES boot ets Jan 8 2013,rst cause:2, boot mode:(3,6)`), read at 74880 baud.

USB always wins. The bridge tries the cable first and WiFi every 15 s while the pet isn't plugged in, and plugging the cable back in moves the link to USB by itself. Running the pet from a phone charger or power bank is how it goes wireless: about 20 to 30 s after it powers up, the bridge reaches it over WiFi. Everything works over WiFi: a full `SN` snapshot takes about 0.4 s.

## The protocol

One short line per message, the same on USB and WiFi, simple enough to type into a serial monitor:

| Line | Meaning |
| --- | --- |
| `HI` | Who are you? The pet answers `PET desktop-pet 10` |
| `IP` | Its WiFi address: `IP 192.168.1.12`, or `IP -` when it isn't online |
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
| `RF`, `RF test`, `RF poll` | The RC522: its chip version; a wiring test (a register written and read back, and whether anything drives MISO); one try at reading a card |
| `MD music [headphones\|dance\|bars]`, `MD watch`, `MD -` | What's playing; a mood can be asked for by name |
| `VZ <16 hex digits>` | Spectrum bars, 0 to f each, low to high |
| `BE` | A beat |
| `JS` | A sudden loud moment in a film or a game |
| `AC code [typing]`, `AC game`, `AC -` | What's in front on the PC |
| `SF ready`, `SF count <ms>`, `SF shot`, `SF -` | A selfie: the camera opening, the spoken countdown, the shutter, done |
| `LO <lat> <lon> <place>` | Where the weather is for; kept in flash for the WiFi to use |
| `WF <ssid><tab><password>` | USB only: join a WiFi network (the ESP-01 keeps it) |
| `ES talk\|flash [baud]`, `ES reset` | USB only: the cable straight through to the ESP-01, in its own program or its bootloader, until 12-30 s of quiet |

The pet sends events back: `EV pet`, `EV next <screen>`, `EV hold`, `EV hold timer`, `EV dismiss`, `EV wifi ok <ip>`, `EV wifi fail`, `EV card <uid> tap`.

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
| ESP-01 answers `AT` but garbles its replies, restarts or goes silent while joining or scanning | Brownout. Its radio pulls ~300 mA bursts: serial turns to garbage (`AT+CWLAP` echoed as `AfWLAP`), then it crashes. Lowering its transmit power (`AT+RFPOWER`) didn't help. The cause here was the wiring, not the regulator: long breadboard jumpers drop too much voltage under those bursts, and short direct wires fixed it. If it still drops out on short wires, give it its own **AMS1117-3.3** on the 5V pin with a 470 µF capacitor. |

## How the code is laid out

| File | What's in it |
| --- | --- |
| `src/pet.h` | The shared `World`: time, weather, timer, Aminal's state, the current card |
| `src/main.cpp` | Setup, the frame loop, the button, switching screens |
| `src/face.cpp` | Eyes, moods, fidgets, and the listening, thinking and speaking faces |
| `src/screens.cpp` | Clock, weather icons, timer, status, and the alert card |
| `src/media.cpp` | Headphones, music notes, the visualizer, and the popcorn |
| `src/activity.cpp` | Glasses, the keyboard and paws, the controller, and the sparks |
| `src/camera.cpp` | The camera face for a selfie: countdown, flash and shutter |
| `src/rfid.cpp` | The RC522: polling for a card, and turning a card's arrival into a tap |
| `src/link.cpp` | The protocol, read from USB and WiFi alike, and the ESP-01 passthrough |
| `src/wifi.cpp` | The ESP-01 driven through its AT firmware: joining, the HTTP request, reading the reply |

All timers compare with `reached(now, t)` instead of `now > t`, so the pet keeps blinking after `millis()` wraps around at about 49.7 days.

The saved location lives in the chip's last flash sector (0x08020000 on the F401CC). The app starts at 0x08004000 and is about 98 KB, so it has about 14 KB of room before the two would meet.

## Stack

C++ on the Arduino framework (STM32duino), built with PlatformIO. Adafruit SSD1306 and Adafruit GFX for the display, FreeSans Bold for the big digits.

## Next

- The touch pad, once it arrives (the firmware already powers and reads it)
