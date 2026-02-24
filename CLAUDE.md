# La Respuesta

Procedural LED art display using bytebeat algorithms on 74HC595 shift registers. Drives an 8x8 panel grid (8 columns × 8 panels) of LEDs with animated, symmetrical patterns generated from bitwise math.

## Hardware

**Targets:** Arduino UNO/Nano and Wemos D1 Mini (ESP8266)

### Pin mappings

| Signal    | Arduino UNO/Nano | Wemos D1 Mini       |
|-----------|------------------|---------------------|
| LATCH     | 6                | D1 (GPIO5)          |
| COLS_DATA | 7                | D2 (GPIO4)          |
| COLS_SH   | 8                | D3 (GPIO0)          |
| SPI MOSI  | 11 (hardware)    | hardware default     |
| SPI SCK   | 13 (hardware)    | hardware default     |

Column selection uses `shiftOut()` (software). Row data uses hardware SPI (`SPI.transfer()`, LSBFIRST, MODE0, DIV8).

**Known-good reference sketches:** `santafe_01` (Arduino UNO/Nano) and `simetria_04` (Wemos D1 Mini).

## Build & upload

No platformio.ini or Makefile — use Arduino IDE or `arduino-cli`.

```bash
# Compile
arduino-cli compile -b arduino:avr:uno santafe_01/

# Upload
arduino-cli upload -b arduino:avr:uno -p /dev/ttyUSB0 santafe_01/

# For Wemos D1 Mini
arduino-cli compile -b esp8266:esp8266:d1_mini simetria_04/
arduino-cli upload -b esp8266:esp8266:d1_mini -p /dev/ttyUSB0 simetria_04/
```

## Dependencies

Only standard Arduino libraries: `<SPI.h>` and `<limits.h>`. No external libraries.

## Architecture

### Update-render loop

All sketches decouple animation from display refresh:

```
loop() {
  if (millis() - ptime > dtime)  →  update()   // generate next frame (15-500ms)
  drawColumns()                  →  render      // continuous column multiplexing
}
```

`dtime` controls animation FPS (2–67 Hz). Display refresh runs as fast as possible for flicker-free multiplexing.

### Data model

```c
unsigned long panels[64];  // 8 columns × 8 panels, 32 bits (4 bytes) per entry
```

Each `panels[column + 8*panel]` holds 4 bytes of row data sent via SPI, then the column is latched active via `shiftOut()`.

### Bytebeat formulas

Three core formulas generate patterns from a time counter `ut`:

- **Formula 0:** `ut * (((ut >> 12) | (ut >> 8)) & (31 & (ut >> 4)))`
- **Formula 1:** `ut * (((ut >> 12) & (ut >> 8)) ^ (31 & (ut >> 3)))`
- **Formula 2:** `ut * (((ut >> 23) & (ut >> 13)) ^ (19 & (ut >> 5)))`

Each returns a byte. The active formula switches periodically (random or counter-based).

### Symmetry

4-way mirror from a single pattern calculation:

1. Compute one byte via bytebeat for position `(column, panel)`
2. Write original byte to lower 16 bits of `panels[id]`
3. Write bit-reversed byte (`reverse()`) to upper 16 bits → horizontal mirror
4. Compute `antiId` = opposite corner → vertical mirror
5. Write same structure to `antiId` → full bilateral symmetry

`reverse()` does byte-level bit reversal using swap-based algorithm (nibbles → pairs → bits).

## Sketch progression

Each folder contains a single `.ino` file with matching name.

### Phase 1 — Shift register basics
- `helloWorld_595/` — minimal 74HC595 example
- `oneByOne_595/` — serial-controlled demo
- `data_595/` — pre-calculated pattern array

### Phase 2 — Bytebeat exploration (Arduino)
- `test_01/` — first animated bytebeat, single column buffer
- `test_02/` — extends to 8×8 panel grid
- `test_03/` — static patterns with byte reversal
- `test_04/` — parametric pattern generation + symmetry
- `test_05/` — multiple formulas, play/pause state, random switching

### Phase 3 — Platform expansion (ESP8266)
- `test_06_esp8266/` — port of test_05 to Wemos D1 Mini
- `praxis_01/` — practical Wemos implementation

### Phase 4 — Symmetry refinement (Wemos D1 Mini)
- `simetria_01/` — initial symmetry on D1 Mini (note: has typo `D3v` in pin define)
- `simetria_02/` — probability-based formula switching
- `simetria_03/` — aggressive formula variation, 15ms timing
- `simetria_04/` — tuned timing (15ms) and switching rates

### Phase 5 — Production
- `santafe_01/` — state machine (RUNNING/TESTING/STOP), counter-based pause, 4th formula, best-commented version

## Known issues

- `test_01/test_01.ino`: `latchTime` = 10µs (vs 500µs standard) — very short latch pulse, may cause display glitches if shift registers can't respond in time.
- `test_06_esp8266/test_06_esp8266.ino`: uses raw GPIO numbers instead of Wemos D-pin aliases (`LATCH=8` → GPIO15, `COLS_DATA=4` → GPIO2, `COLS_SH=3` → GPIO0). GPIO15 is a boot strapping pin that must be LOW at boot — connecting shift register hardware here may cause boot failures on Wemos D1 Mini. Use `simetria_04` as the reference ESP8266 sketch instead.
- `simetria_01/simetria_01.ino`: fixed — pins corrected to D1/D2/D3 (were Arduino-style integers 6/7/8), and `diex()` call in `pattern()` fixed to pass required `formula` argument.
