#!/usr/bin/env python3
"""
bienalas_01 emulator — pygame LED matrix visualizer

Exact port of the bytebeat + symmetry logic from bienalas_01/bienalas_01.ino.
Edit FORMULAS below to experiment with new patterns; press the index digit key
(0–9) at runtime to select a formula.

Arithmetic fidelity note:
  On Arduino Nano (AVR), 'int' is 16-bit.  'unsigned ut = unsigned(t)' in
  bytebeat() keeps only the lower 16 bits of t.  The pattern() argument 'it'
  is also a 16-bit 'int' parameter, so iterations is truncated before the
  formula runs.  This emulator replicates that truncation so the patterns
  match the hardware exactly.

Controls:
  0–9    select formula directly
  SPACE  toggle pause
  +/-    speed up / slow down  (±5 ms per step)
  R      randomize iteration seed
  A      toggle automatic formula switching
  ESC    quit
"""

import sys
import random
import pygame

# ── hardware constants — must match bienalas_01.ino ───────────────────────────
COLS_PER_PANEL = 8
NUM_PANELS     = 8
BUFFER_SIZE    = COLS_PER_PANEL * NUM_PANELS   # 64 entries

# ── timing — matches .ino defaults ───────────────────────────────────────────
DTIME_DEFAULT = 45      # ms per animation frame  (dtime = 45 in .ino)
OFF_TIME_MS   = 6000    # pause duration between bursts  (offTime = 6000)
BURST_FRAMES  = 32      # frames per running burst before auto-pause

# ── display geometry ──────────────────────────────────────────────────────────
ROWS     = 32           # 4 bytes × 8 bits per buffer entry
LED_PX   = 10           # pixel side of each LED square
GAP      = 2            # pixels between adjacent LEDs
PAN_GAP  = 8            # extra separator between panels
PAD      = 14           # window edge padding
STATUS_H = 44           # height of status bar at bottom

# ── colours ───────────────────────────────────────────────────────────────────
C_BG    = ( 16,  16,  16)
C_OFF   = ( 35,  35,  35)
C_ON    = (255, 148,  24)   # warm amber — matches typical LED color
C_TEXT  = (175, 175, 175)
C_PAUSE = ( 80, 130, 220)
C_DIM   = ( 95,  95,  95)

# ── bytebeat formulas ─────────────────────────────────────────────────────────
# Each entry: (name, expression_string)
# 'ut' is a 16-bit unsigned integer (matches AVR 'unsigned int' behaviour).
# The result is masked to 8 bits automatically.
#
# Add your own formulas below the existing four; select them at runtime with
# the corresponding digit key.
FORMULAS: list[tuple[str, str]] = [
    ("formula_0",  "ut * (((ut >> 12) | (ut >> 8)) & (31 & (ut >> 4)))"),
    ("formula_1",  "ut * (((ut >> 12) & (ut >> 8)) ^ (31 & (ut >> 3)))"),
    ("formula_2",  "ut * (((ut >> 23) & (ut >> 13)) ^ (19 & (ut >> 5)))"),
    ("test_grid",  "85 << (ut % 2)"),
    # ── add custom formulas here ──────────────────────────────────────────────
    # ("my_formula", "ut * ((ut >> 10) ^ (ut >> 6))"),
]


# ── pre-compile formula strings to fast callables ────────────────────────────
def _compile_formulas() -> list:
    fns = []
    for name, expr in FORMULAS:
        try:
            fn = eval(f"lambda ut: ({expr}) & 0xFF")  # noqa: S307
        except SyntaxError as exc:
            print(f"Formula '{name}' syntax error: {exc}", file=sys.stderr)
            fn = lambda ut: 0  # noqa: E731  silent fallback
        fns.append(fn)
    return fns

_FNS = _compile_formulas()


# ── bytebeat engine ───────────────────────────────────────────────────────────

def reverse_byte(b: int) -> int:
    """Bit-reversal of one byte — exact port of reverse() in bienalas_01.ino."""
    b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4)
    b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2)
    b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1)
    return b & 0xFF


def bytebeat(t: int, formula_idx: int) -> int:
    """
    Port of bytebeat() from bienalas_01.ino.
    AVR 'unsigned ut = unsigned(t)' keeps only the lower 16 bits of t.
    """
    ut = t & 0xFFFF          # 16-bit unsigned — critical for AVR fidelity
    return _FNS[formula_idx](ut)


def pattern(it16: int, col: int, pan: int, formula_idx: int) -> int:
    """
    Port of pattern() — it16 is already the lower 16 bits of iterations
    (matches the 'int it' parameter truncation on AVR).
    """
    t = (it16 + pan * COLS_PER_PANEL + col) & 0xFFFF
    return bytebeat(t, formula_idx)


def update(buf: list, iterations: int, formula: int) -> int:
    """
    Port of update() from bienalas_01.ino.
    Returns the (possibly updated) formula index.
    'iterations' is the full 32-bit counter.
    """
    # periodic formula advance — uses the full 32-bit iterations value
    if iterations % 32000 == 0:
        formula = (formula + 1) % len(FORMULAS)

    # pattern generation uses only the lower 16 bits (AVR int truncation)
    it16 = iterations & 0xFFFF

    for column in range(COLS_PER_PANEL):
        for panel in range(NUM_PANELS // 2):
            line  = pattern(it16, column, panel, formula)
            rline = reverse_byte(line)

            # row bytes [rline|rline|line|line]:
            #   bytes 0-1 (line)  → original pattern
            #   bytes 2-3 (rline) → bit-reversed → top↔bottom mirror within panel
            entry = (
                (rline << 24) |
                (rline << 16) |
                (line  <<  8) |
                 line
            ) & 0xFFFF_FFFF

            idx      = column + COLS_PER_PANEL * panel
            anti_idx = (COLS_PER_PANEL - 1 - column) + COLS_PER_PANEL * (NUM_PANELS - 1 - panel)
            # anti_idx mirrors: column → left↔right, panel → fills bottom half

            buf[idx]      = entry
            buf[anti_idx] = entry

    return formula


# ── window helpers ────────────────────────────────────────────────────────────

def window_size() -> tuple[int, int]:
    cell = LED_PX + GAP
    w = PAD * 2 + NUM_PANELS * COLS_PER_PANEL * cell + (NUM_PANELS - 1) * PAN_GAP
    h = PAD * 2 + ROWS * cell + STATUS_H
    return w, h


def draw(
    screen:   pygame.Surface,
    buf:      list,
    font:     pygame.font.Font,
    formula:  int,
    iters:    int,
    running:  bool,
    dtime:    int,
    auto_sw:  bool,
) -> None:

    screen.fill(C_BG)
    cell = LED_PX + GAP

    for panel in range(NUM_PANELS):
        for col in range(COLS_PER_PANEL):
            entry = buf[col + COLS_PER_PANEL * panel]
            for row in range(ROWS):
                lit   = (entry >> row) & 1
                color = C_ON if lit else C_OFF
                x = PAD + (panel * COLS_PER_PANEL + col) * cell + panel * PAN_GAP
                y = PAD + row * cell
                pygame.draw.rect(screen, color, (x, y, LED_PX, LED_PX))

    # ── status bar ────────────────────────────────────────────────────────────
    _, h = screen.get_size()
    fname, fexpr = FORMULAS[formula] if formula < len(FORMULAS) else ("?", "?")
    sc = C_PAUSE if not running else C_TEXT
    state_s = "PAUSE" if not running else "RUN  "
    auto_s  = "AUTO" if auto_sw else "MANU"

    line1 = font.render(
        f"[{state_s}] [{auto_s}]  formula {formula}: {fname}"
        f"   it={iters:,}   spd={dtime}ms",
        True, sc,
    )
    line2 = font.render(
        f"  expr: {fexpr[:92]}",
        True, C_DIM,
    )
    screen.blit(line1, (PAD, h - STATUS_H + 4))
    screen.blit(line2, (PAD, h - STATUS_H + 22))


# ── main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    pygame.init()
    font   = pygame.font.SysFont("monospace", 12)
    screen = pygame.display.set_mode(window_size())
    pygame.display.set_caption("bienalas_01 — LED matrix emulator")
    clock  = pygame.time.Clock()

    buf        = [0] * BUFFER_SIZE
    iterations = random.randint(0, 9_999_999)          # random(10E6) in .ino
    formula    = random.randint(0, min(2, len(FORMULAS) - 1))
    switch1    = True    # True = RUNNING phase, False = PAUSE burst
    counter    = 0
    last_off   = 0
    dtime      = DTIME_DEFAULT
    auto_sw    = True    # enable auto pause/formula switching
    last_upd   = pygame.time.get_ticks()

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            elif event.type == pygame.KEYDOWN:
                k = event.key

                if k == pygame.K_ESCAPE:
                    running = False

                elif pygame.K_0 <= k <= pygame.K_9:
                    idx = k - pygame.K_0
                    if idx < len(FORMULAS):
                        formula = idx

                elif k == pygame.K_SPACE:
                    switch1 = not switch1
                    if not switch1:
                        last_off = pygame.time.get_ticks()

                elif k in (pygame.K_PLUS, pygame.K_EQUALS, pygame.K_KP_PLUS):
                    dtime = max(5, dtime - 5)

                elif k in (pygame.K_MINUS, pygame.K_KP_MINUS):
                    dtime = min(500, dtime + 10)

                elif k == pygame.K_r:
                    iterations = random.randint(0, 9_999_999)

                elif k == pygame.K_a:
                    auto_sw = not auto_sw

        now = pygame.time.get_ticks()
        if now - last_upd >= dtime:
            if switch1:
                iterations += 1
                formula = update(buf, iterations, formula)
                counter += 1

                if auto_sw and (counter % BURST_FRAMES) == 0:
                    switch1  = False
                    last_off = now
                    if random.randint(0, 9) < 3:
                        formula = random.randint(0, min(2, len(FORMULAS) - 1))
            else:
                if now > last_off + OFF_TIME_MS:
                    switch1 = True

            last_upd = now

        draw(screen, buf, font, formula, iterations, switch1, dtime, auto_sw)
        pygame.display.flip()
        clock.tick(120)

    pygame.quit()
    sys.exit(0)


if __name__ == "__main__":
    main()
