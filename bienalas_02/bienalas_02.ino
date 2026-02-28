
// bienalas_01 — Arduino Nano
// Merges santafe_01 structure with simetria_04 formula switching.
#include <limits.h>
#include <SPI.h>

// --- pins (Arduino Nano) ---
#define LATCH     6
#define COLS_DATA 7
#define COLS_SH   8
#define CONFIG_JUMPER 2   // pulled LOW → TESTING mode

// --- constants ---
#define COLS_PER_PANEL    8
#define NUM_PANELS        8
#define NUM_FORMULAS         11     // total cases in bytebeat()
#define NUM_AUTO_FORMULAS    9      // indices 0–8 auto-cycle; ≥ 9 are test-only
#define FORMULA_ADVANCE_ITERS 32000 // frames between periodic formula steps (~24 min at 45ms)

// --- buffer ---
unsigned long buffer[COLS_PER_PANEL * NUM_PANELS] = {};

// --- bytebeat ---
unsigned long iterations = 0;

// --- display ---
unsigned int  latchTime = 500; // us
unsigned long ptime     = 0;
unsigned long dtime     = 30;

// --- state machine ---
#define STOP    0
#define RUNNING 1
#define TESTING 2
#define FIXED   3   // animate continuously, never switch formula

int  state     = RUNNING;
bool animating = true;

int  formula  = 0;
long offTime  = 6000;
long lastOff  = 0;

unsigned char counter  = 0;
unsigned char burstLen = 32;


void setup() {
  pinMode(LATCH,     OUTPUT);
  pinMode(COLS_DATA, OUTPUT);
  pinMode(COLS_SH,   OUTPUT);
  pinMode(CONFIG_JUMPER, INPUT_PULLUP);

  Serial.begin(115200);
  SPI.begin();
  SPI.setBitOrder(LSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV8);

  getRandomSeed();
  iterations = random(10E6);
  for (int i = 0; i < sizeof(buffer) / sizeof(long); i++) {
    buffer[i] = 0;
  }
  formula   = random(0, NUM_AUTO_FORMULAS - 1);
  burstLen  = random(8, 64);
  if (digitalRead(CONFIG_JUMPER) == LOW) state = TESTING;
}

void getRandomSeed() {
  uint16_t seed = 0;
  for (int i = 0; i < 1E3; i++) seed += seed ^ analogRead(random(6));
  randomSeed(seed);
}


void loop() {
  if ((unsigned long)(millis() - ptime) > dtime) {

    switch (state) {
      case RUNNING:
        if (animating) {
          update();
          if (iterations % FORMULA_ADVANCE_ITERS == 0) formula = (formula + 1) % NUM_AUTO_FORMULAS;
          counter++;
          if (counter >= burstLen) {
            counter   = 0;
            animating = false;
            lastOff   = millis();
            offTime   = random(5000, 9000);
            // dtime     = random(20, 80);
            burstLen  = random(80, 640);
            if (random(10) < 3) formula = random(0, NUM_AUTO_FORMULAS - 1);
          }
        } else {
          if (millis() > (lastOff + offTime)) {
            animating = true;
          }
        }
        break;

      case TESTING:
        formula  = NUM_FORMULAS - 1;
        offTime  = 2000;
        if (millis() > (lastOff + offTime)) {
          update();
          lastOff = millis();
        }
        break;

      case FIXED:
        if (animating) {
          update();
        }
        break;
    }

    ptime = millis();
  }
  render();
}


static inline char bytebeat(long t, int formula) {
  unsigned ut = unsigned(t);
  switch (formula) {
    case 0:  return ut * (((ut >> 12) | (ut >> 8)) & (31 & (ut >> 4)));
    case 1:  return ut * (((ut >> 12) & (ut >> 8)) ^ (31 & (ut >> 3)));
    case 2:  return ut * (((ut >> 23) & (ut >> 13)) ^ (19 & (ut >> 5)));
    case 3:  return ut>>16|((ut>>4)%16)|((ut>>4)%192)|(ut*ut%64)|(ut*ut%96)|(ut>>16)*(ut|ut>>5);
    case 4:  return ut*(ut^ut+(ut>>15|1)^(ut-1280^ut)>>10);
    case 5:  return ut>>6^ut&37|ut+(ut^ut>>11)-ut*((ut%24?2:6)&ut>>11)^ut<<1&(ut&598?ut>>4:ut>>10);
    case 6:  return ((ut/2*(15&(0x234568a0UL>>(ut>>8&28))))|ut/2>>(ut>>11)^ut>>12)+(ut/16&ut&24);
    case 7:  return (ut*9&ut>>4|ut*5&ut>>7|ut*3&ut/1024)-1;
    case 8:  return ut*(((ut>>9)&10)|((ut>>11)&24)^((ut>>10)&15&(ut>>15)));
    case 9:  return 85 << (ut % 2);         // test_grid
    case 10: return 0xFF;                   // all LEDs on — hardware test
    // ── add new formulas here; update NUM_FORMULAS above ─────
    default: return 0;
  }
}

unsigned char pattern(int it, int col, int pan) {
  return bytebeat(it + pan * COLS_PER_PANEL + col, formula);
}

void update() {
  iterations++;

  for (int column = 0; column < COLS_PER_PANEL; column++) {
    // compute top half; antiId reflects each entry to the bottom half
    for (int panel = 0; panel < NUM_PANELS / 2; panel++) {

      unsigned char line  = pattern(iterations, column, panel);
      unsigned char rline = reverse(line);          // top↔bottom mirror within row bytes

      // row bytes [rline|rline|line|line]: rline mirrors top↔bottom inside each panel
      unsigned long entry = ((unsigned long)rline << 24)
                          | ((unsigned long)rline << 16)
                          | ((unsigned long)line  <<  8)
                          |  (unsigned long)line;

      int id     = column + COLS_PER_PANEL * panel;
      int antiId = (COLS_PER_PANEL - 1 - column) + COLS_PER_PANEL * (NUM_PANELS - 1 - panel);
      // antiId: (COLS_PER_PANEL-1-column) → left↔right mirror; (NUM_PANELS-1-panel) → bottom half

      buffer[id]     = entry;
      buffer[antiId] = entry;
    }
  }
}

void render() {
  for (int column = 0; column < COLS_PER_PANEL; column++) {
    for (int panel = 0; panel < NUM_PANELS; panel++) {
      SPI.transfer(buffer[column + (COLS_PER_PANEL * panel)]);
      SPI.transfer(buffer[column + (COLS_PER_PANEL * panel)] >> 8);
      SPI.transfer(buffer[column + (COLS_PER_PANEL * panel)] >> 16);
      SPI.transfer(buffer[column + (COLS_PER_PANEL * panel)] >> 24);
    }
    latch(column);
  }
}


unsigned char reverse(unsigned char b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

void black() {
  for (int column = 0; column < COLS_PER_PANEL; column++) {
    SPI.transfer(0);
    SPI.transfer(0);
    latch(column);
  }
}

void latch(int column) {
  shiftOut(COLS_DATA, COLS_SH, LSBFIRST, 1 << column);
  digitalWrite(LATCH, LOW);
  delayMicroseconds(latchTime);
  digitalWrite(LATCH, HIGH);
}
