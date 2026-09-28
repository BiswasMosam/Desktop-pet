# The handbook's source

[`../Desktop-pet-Handbook.pdf`](../Desktop-pet-Handbook.pdf) and its web edition are both built from `handbook.src.html`. Every screen picture in them is a real capture from the pet.

| File | What it does |
| --- | --- |
| `pet.py` | Talks to the pet over USB and grabs frames with `SN` |
| `capture.py` | Drives the pet through every face and screen and records each as `shots/<name>.npz` (frame times plus 64x128 frames) |
| `render.py` | Turns the captures into the cyan GIFs for the web edition (`site/shots/`) |
| `build.py` | Builds `site/index.html` (web, animated GIFs) and `paper.html` (A4, frame strips as vector pixels) from the one source |
| `print.mjs` | Prints `paper.html` to PDF with headless Chrome, through the puppeteer-core in `D:/Garage/Fill.ai/node_modules` |

## Rebuilding

1. **Only if the pet's look changed:** close Aminal (its bridge holds the pet's port), then `python3.12 capture.py`, or name groups: `python3.12 capture.py aminal music`. It takes about 2.5 minutes for everything.
2. `python3.12 render.py`, then `python3.12 build.py`.
3. `node print.mjs paper.html handbook.pdf`, then set the metadata and save it as `../Desktop-pet-Handbook.pdf`.

The build reads the pet's own 5x7 font from PlatformIO's copy of Adafruit GFX (`.pio/libdeps`), and the PDF uses the fonts in `D:/Garage/BiswasMosam.github.io/fonts/`, because headless Chrome here can't reach Google Fonts.

When the document changes, bump the revision on the cover and in the footer, and add a row to its *Document history* table.
