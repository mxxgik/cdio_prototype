#!/usr/bin/env python3
"""
verify_fao.py — cross-check the prototype's formulas against worked examples
from FAO Irrigation & Drainage Paper 56 (the document this project uses).

Why this exists: you can validate every calculation on your computer *before*
flashing anything to the ESP32. The Python below is a line-for-line mirror of
src/fao_et.cpp and src/soil_water.cpp; if all checks pass, the C++ logic is
doing what the FAO document says.

Run:  python3 tools/verify_fao.py
"""

import json
import math
import pathlib
import sys

PASSED = []
FAILED = []


def check(name, got, expected, tol):
    ok = abs(got - expected) <= tol
    (PASSED if ok else FAILED).append(name)
    flag = "ok  " if ok else "FAIL"
    print(f"[{flag}] {name}: got {got:.3f}, expected {expected:.3f} (+/-{tol})")


# ---------------------------------------------------------------------------
# Mirror of src/fao_et.cpp
# ---------------------------------------------------------------------------
def extraterrestrial_radiation(day_of_year, latitude_deg):
    """FAO eq. 21 (+ eq. 22..27) -> MJ m-2 day-1"""
    phi = math.radians(latitude_deg)                                   # eq. 22
    dr = 1 + 0.033 * math.cos(2 * math.pi * day_of_year / 365)         # eq. 23
    dec = 0.409 * math.sin(2 * math.pi * day_of_year / 365 - 1.39)     # eq. 24
    ws = math.acos(max(-1.0, min(1.0, -math.tan(phi) * math.tan(dec))))  # eq. 25
    term = ws * math.sin(phi) * math.sin(dec) + math.cos(phi) * math.cos(dec) * math.sin(ws)
    ra = (24 * 60 / math.pi) * 0.0820 * dr * term                      # eq. 21
    return max(ra, 0.0)


def hargreaves_eto(day_of_year, latitude_deg, tmin, tmax, tmean):
    """FAO eq. 52 -> mm/day (Ra converted with eq. 20)"""
    ra_mm = 0.408 * extraterrestrial_radiation(day_of_year, latitude_deg)  # eq. 20
    return 0.0023 * (tmean + 17.8) * math.sqrt(max(tmax - tmin, 0)) * ra_mm  # eq. 52


# ---------------------------------------------------------------------------
# Mirror of src/soil_water.cpp
# ---------------------------------------------------------------------------
def soil_water(theta_fc, theta_wp, root_depth, p, kc, theta_measured, eto, area_m2):
    taw = 1000 * (theta_fc - theta_wp) * root_depth        # eq. 82
    raw = p * taw                                          # eq. 83
    dr = 1000 * (theta_fc - theta_measured) * root_depth   # eq. 87
    dr = min(max(dr, 0), taw)                              # eq. 86
    ks = 1.0 if dr <= raw else (taw - dr) / ((1 - p) * taw)  # eq. 84
    etc = kc * eto                                         # eq. 58
    etc_adj = ks * kc * eto                                # eq. 81
    hours = 0.0 if (dr >= raw or etc < 0.05) else (raw - dr) / etc * 24
    return dict(taw=taw, raw=raw, dr=dr, ks=ks, etc=etc,
                etc_adj=etc_adj, hours=hours, irrig_mm=dr,
                irrig_total_l=dr * area_m2)


# ===========================================================================
# 1) Validation against the document's own worked examples
# ===========================================================================
print("--- FAO worked examples ---")

# EXAMPLE 8 (Chapter 3): Ra on 3 September at 20°S -> 32.2 MJ m-2 d-1
check("Ex.8  Ra (J=246, lat=-20)", extraterrestrial_radiation(246, -20), 32.2, 0.1)

# EXAMPLE 15 (Chapter 3): Ra on 15 July at 45°43'N -> 40.6 MJ m-2 d-1
check("Ex.15 Ra (J=196, lat=45.72)", extraterrestrial_radiation(196, 45.72), 40.6, 0.1)

# EXAMPLE 36 (Chapter 8): tomato on silt, Zr=0.8: TAW=136 mm, RAW=54 mm
r = soil_water(theta_fc=0.32, theta_wp=0.15, root_depth=0.8, p=0.40, kc=1.2,
               theta_measured=0.32, eto=5.0, area_m2=1)
check("Ex.36 TAW (tomato, silt)", r["taw"], 136, 1.0)
check("Ex.36 RAW (tomato, silt)", r["raw"], 54, 1.0)

# EXAMPLE 37 (Chapter 8): Ks sequence as Dr grows past RAW=64 (TAW=160)
p, taw = 0.40, 160.0
for dr, expected in [(67, 0.97), (72.8, 0.91), (78.3, 0.85), (83.4, 0.80),
                     (88.2, 0.75), (92.6, 0.70), (96.9, 0.66), (100.8, 0.62)]:
    ks = (taw - dr) / ((1 - p) * taw)                      # eq. 84
    check(f"Ex.37 Ks at Dr={dr}", ks, expected, 0.005)

# ===========================================================================
# 2) End-to-end run with the prototype's own sample data file
# ===========================================================================
print("\n--- end-to-end with data/field_data.json + config.h defaults ---")

root = pathlib.Path(__file__).resolve().parent.parent
reading = json.loads((root / "data" / "field_data.json").read_text())

# include/config.h defaults: loam, cool-season grass, lat 40.4168, Zr 0.7, 1000 m2
THETA_FC, THETA_WP, ZR, P, KC, LAT, AREA = 0.25, 0.12, 0.70, 0.40, 0.95, 40.4168, 1000.0

date = reading["date"]
year, month, day = map(int, date.split("-"))
month_days = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334]
doy = month_days[month - 1] + day
if (year % 4 == 0 and year % 100 != 0) or year % 400 == 0:
    if month > 2:
        doy += 1

tmean = reading.get("tmean_c", (reading["tmax_c"] + reading["tmin_c"]) / 2)
eto = hargreaves_eto(doy, LAT, reading["tmin_c"], reading["tmax_c"], tmean)
w = soil_water(THETA_FC, THETA_WP, ZR, P, KC,
               reading["ground_humidity_pct"] / 100.0, eto, AREA)

print(f"day of year      : {doy}")
print(f"ETo (Hargreaves) : {eto:.2f} mm/day")
print(f"ETc = Kc*ETo     : {w['etc']:.2f} mm/day")
print(f"TAW / RAW        : {w['taw']:.0f} / {w['raw']:.0f} mm")
print(f"Dr (depletion)   : {w['dr']:.0f} mm  -> stressed: {w['dr'] >= w['raw']}")
print(f"Ks               : {w['ks']:.2f}")
print(f"time to stress   : {w['hours']:.0f} h")
print(f"water to refill  : {w['irrig_mm']:.0f} mm = "
      f"{w['irrig_mm']:.0f} L/m2 = {w['irrig_total_l']:.0f} L total")

# --- sanity assertions on the end-to-end numbers ---------------------------
assert 0.0 < eto < 15.0, "ETo outside plausible range"
assert 0.0 <= w["ks"] <= 1.0, "Ks must be within [0,1]"
assert w["dr"] == min(max(w["dr"], 0), w["taw"]), "Dr not clamped"

print(f"\n{len(PASSED)} checks passed, {len(FAILED)} failed")
if FAILED:
    print("FAILED:", *FAILED, sep="\n  - ")
    sys.exit(1)
print("All FAO cross-checks passed")
