/**************************************************************
  bytebeat 2018
  parc. lima.
**************************************************************/
#include <limits.h>;
#include <SPI.h>

#define LATCH 6           // ROWS and COLS
#define COLS_DATA 7       
#define COLS_SH   8       
// #define ROWS_DATA  11  // MOSI (SPI)
// #define ROWS_SH  13  // SCK (SPI)

#define col_0 B00000001
#define col_1 B00000010
#define col_2 B00000100
#define col_3 B00001000
#define col_4 B00010000
#define col_5 B00100000
#define col_6 B01000000
#define col_7 B10000000

uint8_t cols[] = {col_0, col_1 , col_2 , col_3 , col_4 , col_5 , col_6 , col_7};

#define NUM_COLS 8
uint16_t value = 0;


uint16_t white[] = {
  B11111111 << 8 | B11111111,
  B11111111 << 8 | B11111111,
  B11111111 << 8 | B11111111,
  B11111111 << 8 | B11111111,
  B11111111 << 8 | B11111111,
  B11111111 << 8 | B11111111,
  B11111111 << 8 | B11111111,
  B11111111 << 8 | B11111111
};

uint16_t checkers[] = {
  B10101010 << 8 | B10101010,
  B10101010 << 8 | B10101010,
  B10101010 << 8 | B10101010,
  B10101010 << 8 | B10101010,
  B10101010 << 8 | B10101010,
  B10101010 << 8 | B10101010,
  B10101010 << 8 | B10101010,
  B10101010 << 8 | B10101010,
};


char data[NUM_COLS] = {};
unsigned long lines[8] = {};
unsigned long iterations = 0;
unsigned long l;
unsigned char line = 255;


static inline char diex(long t)
{
   unsigned ut = unsigned(t);
//   return ut * (((ut >> 12) | (ut >> 8)) & (31 & (ut >> 4))); //1
//   return ut * (((ut >> 12) & (ut >> 8)) ^ (31 & (ut >> 3))); 
   return ut * (((ut >> 23) & (ut >> 13)) ^ (19 & (ut >> 5))); 
}

void setup() {

  pinMode(LATCH, OUTPUT);
  pinMode(COLS_DATA, OUTPUT);
  pinMode(COLS_SH, OUTPUT);

  digitalWrite(SS, HIGH);  // ensure SS stays high
  Serial.begin(115200);
  SPI.begin ();

  SPI.setBitOrder(LSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider (SPI_CLOCK_DIV8);


  getRandomSeed();
  iterations = random(10E6);
}

void getRandomSeed()
{
  uint16_t seed = 0;
  for (int i = 0; i < 1E3; i++) seed += seed ^ analogRead(random(6));
  randomSeed(seed);
}

// http://unixwiz.net/techtips/reading-cdecl.html
unsigned long count = 0;
unsigned long ptime = 0;
unsigned long dtime = 35;


void loop() {

  if ((unsigned long)(millis() - ptime) > dtime) {
    update();
    ptime = millis();
  }
  render2();
}


void update()
{
  iterations = (iterations + 1);
  line = diex(iterations);
  for (int r = 0; r < NUM_COLS; r++) {
      // TOP DOWN
      lines[r] >>= 1;
      lines[r] |= (unsigned long) (line >> r & 1) << 31;
      // BOTTOM UP
//    lines[r] <<= 1;
//    lines[r] |= line >> r & 1 ;
  }
}

void render2() {
  for (int column = 0 ; column < NUM_COLS; column++) {
    SPI.transfer(lines[column]);
    SPI.transfer(lines[column] >> 8);
    SPI.transfer(lines[column] >> 16);
    SPI.transfer(lines[column] >> 24);
    latch(column);
  }
}

void render() {
  for (int column = 0 ; column < NUM_COLS; column++) {
    SPI.transfer(checkers[column]);
    SPI.transfer(checkers[column]);
    SPI.transfer(checkers[column]);
    SPI.transfer(checkers[column]);
    latch(column);
  }
}

void black() {
  for (int column = 0 ; column < NUM_COLS; column++) {
    SPI.transfer(0);
    SPI.transfer(0);
    latch(column);
  }
}

void latch(int column) {
  shiftOut(COLS_DATA, COLS_SH, LSBFIRST, cols[column]);
  digitalWrite(LATCH, LOW);
  delayMicroseconds(10);
  digitalWrite(LATCH, HIGH);
}

