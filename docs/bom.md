# Bill of materials

Prices are September 2026, shipping to San Francisco (Bayview). SF sales tax is 8.625 % and
every seller below collects it. Items marked **verify** are typical Amazon prices that need a
live check at checkout; Adafruit and Bambu prices were read from their sites on 2026-09-14.

## Hardware: about $41 (with tax), everything new, solder-free

| # | Part | Exact item | Price | Where |
|---|---|---|---|---|
| 1 | ESP32 + 2.8" touch display | AITRIP 1-pack **ESP32-2432S028R** ("Cheap Yellow Display"). Ships with USB cable, JST pigtails, Dupont wires | ~$14 verify | [Amazon B0FCXDVBVZ](https://www.amazon.com/dp/B0FCXDVBVZ) |
| 2 | LED strip | BTF-LIGHTING **WS2812B 5 V, 60 LEDs/m, 1 m, IP30**, black PCB (white PCB: B01CDTE9UC). Bare 22 AWG leads on the ends | ~$9 verify | [Amazon B01CDTED80](https://www.amazon.com/dp/B01CDTED80) |
| 3 | 5 V / 2 A USB wall adapter | Amazon Basics 12 W one-port USB-A (2.4 A). Any 5 V 2 A+ charger works; the board's included cable is USB-A → micro-USB | ~$7 verify | [Amazon B0773J79KC](https://www.amazon.com/dp/B0773J79KC) or Best Buy Insignia 12 W ([SKU 6406429](https://www.bestbuy.com/site/6406429.p), ~$13) |
| 4 | Solder-free connectors | **WAGO 221-413** lever nuts (3-conductor, 24–12 AWG), pack of 10. Need 3 | ~$8 verify | [Amazon B06XGYXVXR](https://www.amazon.com/dp/B06XGYXVXR), Lowe's Bayshore (~$10), or [Home Depot 318072979](https://www.homedepot.com/p/318072979) |

| Scenario | Subtotal | Tax | Total |
|---|---|---|---|
| Items 1–4, all Amazon Prime ([one-click cart](shopping-list.html)) | $38 | $3.30 | **≈ $41** |
| Amazon board + strip, WAGO from Lowe's, charger from Best Buy | $46 | $4.00 | **≈ $50** |
| eBay board ($13.99) + AliExpress strip (~$4), 2–3 week wait | $33 | $2.85 | **≈ $36** |

### Optional

| Part | Exact item | Price | Where |
|---|---|---|---|
| USB-C panel-mount jack for the plinth back (cleaner than a cable slot) | AAOTOKK 1 ft USB-C M/F panel mount | ~$9 verify | [Amazon B08HS6X44P](https://www.amazon.com/dp/B08HS6X44P) |
| same, cheaper part, slower | Adafruit small round USB-C panel mount (12–18 mm hole) | $4.50 + ~$6 USPS | [Adafruit 6069](https://www.adafruit.com/product/6069) |
| Level shifter, only if the first LED flickers | Adafruit 74AHCT125 | $1.50 | [Adafruit 1787](https://www.adafruit.com/product/1787) |
| 2-pack of the display board (spare for another project) | ESP32-2432S028R 2-pack | ~$20 verify | [Amazon B0GX5X7BH6](https://www.amazon.com/dp/B0GX5X7BH6) |

Not needed: hookup wire (the board's pigtails reach the strip leads), soldering iron. A series
resistor on the data line is expected to be unnecessary with a 25 cm lead; add 300–500 Ω if LED 1
misbehaves (untested on hardware yet).

## Filament (separate; Bambu Lab P1S)

| Part | Exact item | Price | Where |
|---|---|---|---|
| Silver PLA, 1 kg — arena base + lid + plinth ring, top plate, bottom lid (~480 g) | Bambu PLA Basic Silver, with spool $18.99 / refill $15.99 | $16–19 | [Bambu US store](https://us.store.bambulab.com/products/pla-basic-filament) |
| White PLA — beam tube, spine and socket (~35 g; the tube is a single-wall vase print) | Bambu PLA Basic White refill, or ELEGOO / SUNLU white, or Bambu Jade White at Best Buy today ($22.99) | $13–23 | [Amazon search](https://www.amazon.com/s?k=white+pla+filament+1kg) |
| alt: one spool only | Print everything in white and skip the silver | $13–19 | — |
| optional accent | Bambu PLA Basic Purple for the plinth | $18.99 | Bambu store |

No clear PETG: a single 0.45 mm white PLA wall diffuses the strip well.

## Local pickup near Bayview

| Store | Distance | What | Notes |
|---|---|---|---|
| **Lowe's**, 491 Bayshore Blvd, SF 94124 · Mon–Sat 6a–10p, Sun 7a–8p · (415) 486-8611 | in the neighborhood | WAGO 221 lever nuts, USB chargers | Ask for the **221-413** (24–12 AWG); the site lists the bigger 221-613 |
| **Home Depot Colma II**, 2 Colma Blvd · same hours · (650) 755-9600 | ~6 mi | WAGO 221-413 10-pack (item 60338741) | backup for Lowe's |
| **Best Buy**, 1717 Harrison St, SF | ~3 mi | Insignia 12 W USB charger; Bambu PLA Basic (Jade White, Gray, Black) $22.99–24.99 | in-store pickup |
| **Central Computers**, 837 Howard St | ~4 mi | nothing useful (no ESP32 boards; PC ARGB strips at $35+) | skip |
| **Jameco**, 1355 Shoreway Rd, Belmont · will-call ≥ $20, 2 h processing | ~18 mi | Adafruit NeoPixel strips, Adafruit USB-C panel jack 4218 ($9.95) | no CYD board |
| **Micro Center**, 5201 Stevens Creek Blvd, Santa Clara | ~45 mi | Inland WS2812B 1 m strip, Inland PLA from $9.99, Bambu PLA, Inland ESP32 (no screen) | only if already down there |

**The display board is not stocked anywhere local.** Micro Center's WaveShare display boards
use a different driver and pinout. Order it on Amazon; Prime same-day / next-day covers SF.

## Wiring (solder-free)

```
CYD header P1  (VIN, TX, RX, GND)      CYD connector CN1 (GND, IO22, IO27, 3V3)
        VIN ──[WAGO]── strip 5V  (red)          IO22 ──[WAGO]── strip DIN (green)
        GND ──[WAGO]── strip GND (white/black)
```

- Use the JST pigtails that come with the board. Fold the thin pigtail wire back on itself so
  the WAGO clamps it; the strip's 22 AWG lead goes in the other port.
- 14 LEDs at the default brightness ≈ 0.45 A plus the board ≈ 0.2 A. The firmware caps the strip at
  0.9 A, so 1.1 A worst case: fine on a 2 A charger.
- 3.3 V data into a 5 V strip works with a short lead. If the first LED flickers, add the
  level shifter above.
