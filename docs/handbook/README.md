# The handbook's source

The handbook lives at **https://www.mosambiswas.com/Desktop-pet/** (GitHub Pages, serving this repo's `docs/` folder) and as [`../Desktop-pet-Handbook.pdf`](../Desktop-pet-Handbook.pdf). Both are built from `handbook.src.html`, and every screen picture in them is a real capture from the pet.

| File | What it does |
| --- | --- |
| `pet.py` | Talks to the pet over USB and grabs frames with `SN` |
| `capture.py` | Drives the pet through every face and screen and records each as `shots/<name>.npz` (frame times plus 64x128 frames) |
| `render.py` | Turns the captures into the cyan GIFs (`site/shots/`) |
| `build.py` | Builds the site (`../index.html`, `../shots/`, `../og.png`), the A4 source `paper.html` (frame strips as vector pixels) and `site/index.html`, the same page without its document wrapper, for hosts that add their own |
| `print.mjs` | Prints `paper.html` to PDF with headless Chrome, through the puppeteer-core in `D:/Garage/Fill.ai/node_modules` |

## Rebuilding

1. **Only if the pet's look changed:** close Aminal (its bridge holds the pet's port), then `python3.12 capture.py`, or name groups: `python3.12 capture.py aminal music`. It takes about 2.5 minutes for everything.
2. `python3.12 render.py`, then `python3.12 build.py`. Pushing `docs/` updates the site.
3. `node print.mjs paper.html handbook.pdf`, then set the metadata and save it as `../Desktop-pet-Handbook.pdf`.

The build reads the pet's own 5x7 font from PlatformIO's copy of Adafruit GFX (`.pio/libdeps`), and the PDF uses the fonts in `D:/Garage/BiswasMosam.github.io/fonts/`, because headless Chrome here can't reach Google Fonts. The site loads the same families from Google Fonts.

When the document changes, bump the revision on the cover and in the footer, and add a row to its *Document history* table.
