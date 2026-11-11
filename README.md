# CDIO Prototype — Field Alert System for Farmers

An ESP32 firmware proof-of-concept that reads a field's temperature and ground
humidity **from a file** (no sensors attached), computes how fast the field is
drying using **FAO Irrigation & Drainage Paper 56**, and sends the farmer a
push notification (via [ntfy.sh](https://ntfy.sh)) with:

- temperature measurements (min / mean / max),
- ground humidity measurements and root-zone depletion,
- **how long until the grass reaches water-stress** ("dries out"),
- **how much water to apply** to refill the field (mm, L/m², total liters),
- an overall verdict: OK / plan irrigation / irrigate now.

---

## Table of contents

1. [How the system works, step by step](#1-how-the-system-works-step-by-step)
2. [The math, and where it comes from](#2-the-math-and-where-it-comes-from)
3. [Project layout — what each file is for](#3-project-layout--what-each-file-is-for)
4. [Why these technical choices?](#4-why-these-technical-choices)
5. [Build & run it](#5-build--run-it)
6. [Setting up ntfy on your phone](#6-setting-up-ntfy-on-your-phone)
7. [Simulating new field data](#7-simulating-new-field-data)
8. [Verifying the formulas on your computer](#8-verifying-the-formulas-on-your-computer)
9. [From prototype to product — adding real sensors](#9-from-prototype-to-product--adding-real-sensors)
10. [Limitations to be honest about](#10-limitations-to-be-honest-about)

---

## 1. How the system works, step by step

```
 data/field_data.json          include/config.h
 (the "sensor readings")       (your field: soil, grass, size, Wi-Fi, ntfy topic)
        │                               │
        └────────────┬──────────────────┘
                     ▼
   ┌─────────────────────────────────────────────────┐
   │ 1. field_data.cpp   read + validate the JSON    │
   │ 2. fao_et.cpp       how much water evaporates   │  ETo (FAO eq. 21 + 52)
   │ 3. soil_water.cpp   how empty is the soil tank  │  TAW/RAW/Dr/Ks (Ch. 8)
   │ 4. alert.cpp        what does it all mean       │  verdict + wording
   │ 5. ntfy.cpp         deliver it                  │  ntfy.sh HTTPS POST
   └─────────────────────────────────────────────────┘
                     │
                     ▼
        Serial monitor  +  phone push notification
```

The program runs this pipeline once at boot. From then on the serial monitor
is also the **live input channel**, so no filesystem re-upload is needed during
a demo:

| You type | Effect |
|---|---|
| `Enter` | re-runs the report (and resends the notification) |
| `h=15` / `tmax=31` / `fecha=2026-11-05` | temporary overrides, applied on top of the file until reboot (`limpiar` clears them) |
| a full JSON object `{ ... }` | validated first, then **saved into the ESP32 flash** — it becomes the permanent data file |
| `ayuda` | prints the command list |

Handy for demonstrations: change a value, press Enter, watch the numbers and
the phone notification change.

---

## 2. The math, and where it comes from

All formulas come from **FAO Irrigation & Drainage Paper 56** —
[Crop evapotranspiration: Guidelines for computing crop water requirements](https://www.fao.org/4/X0490E/x0490e00.htm).
The chapter you linked (Chapter 8, [ETc under soil water stress](https://www.fao.org/4/X0490E/x0490e0e.htm))
provides the soil-water logic; two supporting formulas come from Chapter 3.

### Step A — How fast does water leave the field? (ETo)

We need a reference number: how much water a well-watered grass field loses
per day, called **ETo** [mm/day]. FAO's flagship equation
(Penman-Monteith, eq. 6) needs radiation, humidity and wind — we only have
temperature. FAO itself provides the sanctioned fallback:

> **Eq. 52 (Hargreaves):** `ETo = 0.0023 · (Tmean + 17.8) · √(Tmax − Tmin) · Ra`

where **Ra** is the sun's energy at the top of the atmosphere, computed from
the latitude and day of the year with **eq. 21** (and eq. 22–27). Ra is
converted from MJ m⁻² day⁻¹ to mm/day with **eq. 20** (`× 0.408`).

The daily temperature swing `Tmax − Tmin` is a proxy for cloudiness: clear
days are hot by day and cold by night, cloudy days are flat.

### Step B — The soil as a tank (Chapter 8)

| Quantity | Equation | Meaning |
|---|---|---|
| **TAW** | eq. 82: `1000 · (θFC − θWP) · Zr` | Total water the roots can drink, from field capacity down to wilting point [mm] |
| **RAW** | eq. 83: `p · TAW` | The part usable **without stress**. When it runs out, the grass starts to suffer [mm] |
| **Dr** | eq. 87: `1000 · (θFC − θmeasured) · Zr`, clamped to `[0, TAW]` (eq. 86) | How empty the tank is **right now**, from your humidity reading [mm] |
| **Ks** | eq. 84: `1 if Dr ≤ RAW, else (TAW − Dr)/((1−p)·TAW)` | Stress coefficient 0–1: how much the grass has throttled its water use |
| **ETc adj** | eq. 81: `Ks · Kc · ETo` | Actual water use today, slower when stressed |
| **I** | Ch. 8 "Forecasting": `I ≤ Dr` | Water to apply — refill exactly what's missing, no deep-percolation waste |

Constants, all from the document's tables:

- **Kc = 0.95** (cool-season turf) or **0.85** (warm-season) — Table 12
- **p = 0.40 / 0.50** and **Zr = 0.5–1.0 m** for turf grass — Table 22
- **θFC / θWP** per soil texture — Table 19 (default: loam, 0.25 / 0.12)

### The two answers the farmer asks for

- **"How long until the grass dries out?"** — hours until `Dr` reaches `RAW`,
  extrapolated at today's ETc rate: `(RAW − Dr) / ETc · 24`. It is a
  first-order forecast assuming the weather stays as it is today.
- **"How much water do I need?"** — `Dr` in mm, which is *literally* L/m²
  (1 mm over 1 m² = 1 L), multiplied by the field area for the total.

Units in one sentence: **mm of water = depth**. Saying "apply 28 mm" means
"put 28 liters on every square meter".

---

## 3. Project layout — what each file is for

```
cdio_prototype/
├── platformio.ini          Build config: board = esp32dev, libs, LittleFS
├── data/
│   └── field_data.json     ← the "sensor readings"; uploaded to ESP32 flash
├── include/
│   └── config.h            ← THE FILE YOU EDIT: Wi-Fi, ntfy topic, field, soil
├── src/
│   ├── main.cpp            Orchestrates the 5-step pipeline (setup/loop)
│   ├── field_data.h/.cpp   1. read + validate the JSON from flash
│   ├── fao_et.h/.cpp       2. ETo: Ra (eq. 21) + Hargreaves (eq. 52)
│   ├── soil_water.h/.cpp   3. TAW/RAW/Dr/Ks/irrigation (Chapter 8)
│   ├── alert.h/.cpp        4. verdict + message wording
│   ├── ntfy.h/.cpp         5. Wi-Fi + ntfy.sh HTTPS delivery
│   └── serial_input.h/.cpp live input: Enter / key=value overrides / pasted JSON
└── tools/
    └── verify_fao.py       Run the same math on your PC, checked against the
                            FAO worked examples (Examples 8, 15, 36, 37)
```

**Where does `data/field_data.json` live?** PlatformIO has two upload targets:

- `pio run -t upload` — flashes the compiled **program** into the app partition
- `pio run -t uploadfs` — flashes the **`data/` folder** into a filesystem
  partition (LittleFS) that the program can read at runtime

You need `uploadfs` **once** (to put a data file on a blank board) and
`upload` whenever the code changes. After that, new readings go in live
through the serial monitor — paste a JSON (§7) — no more `uploadfs`.

---

## 4. Why these technical choices?

**PlatformIO instead of the Arduino IDE** — one command builds, installs the
exact ESP32 toolchain and libraries, and opens the serial monitor
(`pio device monitor`). Versions live in `platformio.ini`, so the project
builds identically on any machine — important when handing a prototype to
someone else. (VS Code users: install the "PlatformIO IDE" extension and the
GUI buttons do the same thing.)

**LittleFS + a JSON file instead of hardcoded values** — the ESP32 has flash
memory that can be exposed as a small filesystem. Putting the readings in a
file means "new day, new data" is just re-uploading `data/`, never touching
the code. JSON was chosen over CSV because it self-describes each field
(`"tmax_c": 27.5` can't be confused with humidity) and ArduinoJson validates
it with precise error messages.

**Hargreaves instead of Penman-Monteith** — the prototype measures only
temperature; FAO explicitly designed eq. 52 for that situation. When real
sensors for wind/humidity/radiation arrive, the Penman-Monteith equation can
be added inside `fao_et.cpp` without changing anything downstream.

**ntfy.sh instead of a chat-app API** — free, open source, no account, no
API key, no phone number and no third-party bot: one HTTPS POST to
`https://ntfy.sh/<topic>` with the text as the body. The topic name doubles
as the credential, so nothing sensitive lives in the firmware. It is a
personal-use hosted service without an SLA — perfect for a proof of concept;
`ntfy.cpp` is the only file to change if you later self-host ntfy or move to
another gateway.

**Serial re-trigger instead of a timer loop** — pushing notifications in
a loop would spam the farmer (and the free gateway). A boot-time report plus
manual re-trigger keeps demos deterministic. A production version would run
this from a deep-sleep timer (e.g. every morning at 7:00).

---

## 5. Build & run it

### Prerequisites

- PlatformIO Core (already installed on this machine at
  `~/.platformio/penv/bin/platformio`; or the VS Code "PlatformIO IDE"
  extension)
- A USB cable + any ESP32 DevKit board
- 2.4 GHz Wi-Fi

### Steps

```bash
cd ~/Documents/cdio_prototype

# 0) optional: check the math on your PC first
python3 tools/verify_fao.py

# 1) edit your settings
#    include/config.h  -> Wi-Fi, field latitude/area, soil, grass
#    (phone notifications: see section 6)

# 2) build + upload the program
pio run -t upload

# 3) upload the data file to the ESP32 flash (once — after that, send new
#    data live from the monitor instead, see section 7)
pio run -t uploadfs

# 4) watch it run
pio device monitor        # 115200 baud
```

Expected serial output:

```
[data] ok: potrero_norte, doy=306, T=14.2/27.5/20.9 °C, suelo=21.0%
[calc] ETo por Ra=2.42 mm/día | TAW=91 RAW=36 Dr=28 Ks=1.00 ETc=2.30
============= informe del campo =============
Alerta de campo - potrero_norte
...
========================================
[ntfy] HTTP 200: {"id":"bXkiqB1x0AbRJpN"}
[ntfy] mensaje enviado
Pulsa Enter para repetir el informe; escribe 'ayuda' para ver los comandos.
```

> **First test without Wi-Fi/ntfy topic?** Set `ENABLE_NTFY 0` in
> `include/config.h` — the report still prints on the serial monitor.

---

## 6. Setting up ntfy on your phone

One-time, ~2 minutes, free — **no account, no phone number, no API key**:

1. Invent a long random topic name, e.g. `cdio-field-a7f3k9q2x4z`
   (letters, digits, `-` and `_` only). It is your secret: anyone who knows
   it can read your alerts, so don't use an obvious name.
2. On your phone install the free **ntfy** app
   ([Play Store](https://play.google.com/store/apps/details?id=io.heckel.ntfy),
   [F-Droid](https://f-droid.org/packages/io.heckel.ntfy/),
   [App Store](https://apps.apple.com/app/ntfy/id1625396347)) — or skip the
   app entirely and open `https://ntfy.sh/<your-topic>` in any browser.
3. **Subscribe to the topic** (tap *+* in the app, type the topic name).
   Subscribe *first*: ntfy delivers live, it is not an inbox — anything sent
   while nobody is listening is lost.
4. Put the same topic in `include/config.h`:
   ```c
   #define NTFY_TOPIC  "cdio-field-a7f3k9q2x4z"
   #define ENABLE_NTFY 1
   ```
5. Rebuild and upload: `pio run -t upload`.

Prove the channel works **before involving the ESP32** — from your computer:

```bash
curl -d "hello from the field" https://ntfy.sh/YOUR_TOPIC
```

…or from a browser tab:

```
https://ntfy.sh/YOUR_TOPIC  →  type a message in the "Publish" box
```

Your phone buzzes within a second. Now flash the ESP32 and the same topic
carries the field reports.

**Demo tip:** keep the ntfy app open (or the browser tab visible) while you
re-trigger reports from the serial monitor, so the audience sees the push
notification arrive the moment the ESP32 computes it.

---

## 7. Simulating new field data

### Way 1 — quick overrides in the monitor (fastest, temporary)

Open `pio device monitor` and type, then press Enter:

```
h=15                 soil humidity → 15 %      (verdict flips to "irrigate now")
tmax=31 tmean=26     hotter day → higher ETo
fecha=2026-07-15     different day of year → different sun position
limpiar              back to the file's values
```

These live in RAM only: they survive re-runs but disappear on reboot, which
is exactly what you want for "flip the verdict live" moments.

### Way 2 — paste a whole JSON (permanent, replaces `uploadfs`)

Edit `data/field_data.json` locally, copy its contents, paste them into the
monitor (multi-line is fine — the program waits for the closing `}`). It is
validated first, then **written to the ESP32 flash**, so it becomes the new
baseline and survives reboots. A paste that fails validation keeps the old
file untouched.

### Way 3 — offline edit + `uploadfs` (the classic way)

Edit `data/field_data.json`, then `pio run -t uploadfs` and press Enter in
the monitor. Still useful as a "factory reset" back to what's in the repo.

### The file, field by field

```json
{
  "field_id": "potrero_norte",     // any label for the report
  "date": "2026-11-02",            // drives sun position (day of year)
  "tmin_c": 14.2,                  // daily minimum temperature [°C]
  "tmax_c": 27.5,                  // daily maximum temperature [°C]
  "tmean_c": 20.9,                 // optional; (tmax+tmin)/2 if omitted (eq. 9)
  "ground_humidity_pct": 21.0      // volumetric soil water [%] (21.0 = 0.21 m³/m³)
}
```

Ideas to demo all three verdicts (defaults: loam, Zr 0.7 m ⇒ TAW 91 mm,
RAW 36.4 mm; Dr = 700 · (0.25 − θ)):

| Verdict | Try | Why |
|---|---|---|
| OK | `"ground_humidity_pct": 30` | θ > θFC → Dr clamps to 0, tank full |
| plan irrigation | `"ground_humidity_pct": 20` | Dr ≈ 35 mm, just under RAW 36 mm → stress in ~15 h |
| irrigate now | `"ground_humidity_pct": 15` | Dr ≈ 70 mm, well past RAW → Ks ≈ 0.39, grass already stressed |

---

## 8. Verifying the formulas on your computer

`tools/verify_fao.py` mirrors the C++ math line-for-line and validates it
against the worked examples printed in the FAO document itself:

- Example 8 → extraterrestrial radiation Ra = 32.2 MJ m⁻² d⁻¹
- Example 15 → Ra = 40.6 MJ m⁻² d⁻¹
- Example 36 → TAW = 136 mm, RAW = 54 mm
- Example 37 → the full Ks sequence (0.97, 0.91, 0.85, 0.80, 0.75, 0.70, 0.66, 0.62)

```bash
python3 tools/verify_fao.py
# 12 checks passed, 0 failed
# All FAO cross-checks passed
```

It also runs your current `data/field_data.json` end-to-end, so you can see
every number that goes into the message before flashing anything.

---

## 9. From prototype to product — adding real sensors

The code was cut so that sensors slot in at exactly one place.

| Prototype today | Real version |
|---|---|
| `data/field_data.json` on flash | e.g. **capacitive soil moisture probe** (volumetric %) + **DHT22/BME280** (Tmin/Tmax) read in `loadFieldReading()` |
| Date written by hand | Real-time clock or NTP over Wi-Fi |
| Report on demand (serial key) | Deep-sleep + timer, report every morning |
| ntfy.sh | Self-hosted ntfy server, or any other gateway (only `ntfy.cpp` changes) |
| Single reading | Daily water balance loop (eq. 85) storing `Dr` in NVS between sleeps |

The calculation modules (`fao_et.*`, `soil_water.*`) never need to change —
they only see numbers.

---

## 10. Limitations to be honest about

1. **Hargreaves is an approximation** (FAO eq. 52). FAO notes it underpredicts
   at high wind and overpredicts at high humidity. With a real weather feed,
   switch to Penman-Monteith inside `fao_et.cpp`.
2. **The "time to stress" forecast assumes tomorrow's weather equals today's.**
   A forecast ETo would improve it.
3. **A single reading is a snapshot**, not the FAO daily water balance
   (eq. 85). Without stored history, rain and past irrigation can't be tracked.
4. **No rain is assumed.** A real deployment must subtract precipitation.
5. **One field, one soil, one grass type** — configured in `config.h`.
6. **The hosted ntfy.sh server is free but has no SLA** — fine for a PoC;
   self-host ntfy or add a second channel for production.

---

### References

- Allen, R.G.; Pereira, L.S.; Raes, D.; Smith, M. (1998).
  *Crop evapotranspiration: Guidelines for computing crop water requirements.*
  FAO Irrigation and Drainage Paper 56. Rome.
  <https://www.fao.org/4/X0490E/x0490e00.htm>
  - Chapter 3 (eq. 20–27, 52) — radiation and the temperature-only ETo method
  - Chapter 6 (eq. 58, Table 12) — crop coefficient Kc
  - Chapter 8 (eq. 81–87, Tables 19, 22) — soil water stress & irrigation
    ← *the chapter linked in the project brief*
- ntfy — push notifications via PUT/POST: <https://ntfy.sh/>
  (docs: <https://docs.ntfy.sh/>, source: <https://github.com/binwiederhier/ntfy>)
