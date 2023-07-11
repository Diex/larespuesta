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

uint16_t value = 0;

#define NUM_COLS 8
#define NUM_PANELS 8
char data[NUM_COLS] = {};
unsigned long panels[8 * 8] = {}; // 8 paneles de 8 columnas de 32 bits.
unsigned long iterations = 0;
unsigned long l;
unsigned char line = 255;





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

  for (int i = 0; i < sizeof(panels) / sizeof(long); i++) {
    panels[i] = 0; //random(ULONG_MAX);
  }
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
unsigned long dtime = 15;

unsigned int latchTime = 500; // us
int state = 0;
bool isRunning = true;
int form = 2;

void loop() {
  
  if ((unsigned long)(millis() - ptime) > dtime) {    
    if(isRunning) update();    
    ptime = millis();
    if(random(5000) < 50){
      isRunning = !isRunning;      
      if(random(10) < 1){
        form = random(0,3);  
      }
    } 
       
  }

  drawColumns();
}


static inline char diex(long t, int formula)
{
  unsigned ut = unsigned(t);

  switch (formula) {
    case 0:
      return ut * (((ut >> 12) | (ut >> 8)) & (31 & (ut >> 4))); //1
      break;
    case 1:
      return ut * (((ut >> 12) & (ut >> 8)) ^ (31 & (ut >> 3)));
      break;
    case 2:
      return ut * (((ut >> 23) & (ut >> 13)) ^ (19 & (ut >> 5)));
      break;
  }

}

unsigned char pattern(int it, int col, int pan) {
  return diex(it + pan * NUM_COLS + col, form);
}

void update() {
  iterations = (iterations + 1);
  if(iterations == 32E3) form = form +1 % 3;
  // esos bytes son...
  //https://www.arduino.cc/reference/en/language/structure/bitwise-operators/bitshiftleft/
  for (int column = 0 ; column < NUM_COLS; column++) {
    for (int panel = 0; panel < NUM_PANELS / 2; panel++) {  // para cada panel tengo que pasarle 4 bytes      
      line = pattern(iterations, column, panel);
      
      line |= (unsigned long) (line >> column & 1) << 31;

      int id = column + (NUM_COLS * panel);
      panels[id] =  0 ;
      // --------------------------------------------
      panels[id] |=  long(line) ;
      panels[id] |=  long(line) << 8;
      // simetria horizontal
      panels[id] |=  long(reverse(line)) << 16;
      panels[id] |=  long(reverse(line)) << 24;

      // --------------------------------------------
      // simetria vertical
      // -------------------------------------------
      int antiId  = (NUM_COLS - 1 - column) + (NUM_COLS * (NUM_PANELS - panel - 1));
      panels[antiId] =  0 ;
      panels[antiId] |=  long(line) ;
      panels[antiId] |=  long(line) << 8;
      // simetria horizontal
      panels[antiId] |=  long(reverse(line)) << 16;
      panels[antiId] |=  long(reverse(line)) << 24;
      // --------------------------------------------

    }
  }
}

void drawColumns() {
  //  this works...
  // in MSBFIRST mode
  //  for(int c = 0; c < 8; c++){
  //    for(int p = 0; p < 8; p++){
  //      SPI.transfer(B11000000);
  //      SPI.transfer(B00000000);
  //      SPI.transfer(B00000000);
  //      SPI.transfer(B00000011);
  //    }
  //    latch(c);
  //  }
  //
  //  return;
  //


  for (int column = 0 ; column < NUM_COLS; column++) {
    for (int panel = 0; panel < NUM_PANELS; panel++) {  // para cada panel tengo que pasarle 4 bytes
      // esos bytes son...
      SPI.transfer(panels[column + (NUM_COLS * panel)]);
      SPI.transfer(panels[column + (NUM_COLS * panel)] >> 8);
      SPI.transfer(panels[column + (NUM_COLS * panel)] >> 16);
      SPI.transfer(panels[column + (NUM_COLS * panel)] >> 24);
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
  for (int column = 0 ; column < NUM_COLS; column++) {
    SPI.transfer(0);
    SPI.transfer(0);
    latch(column);
  }
}

void latch(int column) {
  shiftOut(COLS_DATA, COLS_SH, LSBFIRST, cols[column]);
  digitalWrite(LATCH, LOW);
  delayMicroseconds(latchTime);
  digitalWrite(LATCH, HIGH);
}




