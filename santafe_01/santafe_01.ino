#include <limits.h>;
#include <SPI.h>

#define LATCH 6           // used for ROWS and COLS shifters
#define COLS_DATA 7       // multiplexed  on the columns
#define COLS_SH   8

// 8 paneles de 8 columnas de 32 filas.
#define COLS_PER_PANEL 8
#define NUM_PANELS 8
unsigned long buffer[COLS_PER_PANEL * NUM_PANELS] = {}; 

// bytebeat
unsigned long iterations = 0;
unsigned long l;
// unsigned char line = 255;


// display
unsigned int latchTime = 500; // us
unsigned long ptime = 0;
unsigned long dtime = 45;


// state
#define STOP 0
#define RUNNING 1
#define TESTING 2

int state = RUNNING;
bool switch1 = true;

int formula = 0;
long offTime = 6E3;
long lastOff = 0;



void setup() {

  pinMode(LATCH, OUTPUT);
  pinMode(COLS_DATA, OUTPUT);
  pinMode(COLS_SH, OUTPUT);

  Serial.begin(115200);
  SPI.begin ();
  SPI.setBitOrder(LSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider (SPI_CLOCK_DIV8);


  getRandomSeed();
  // has to start somewhere...
  iterations = random(10E6);
  for (int i = 0; i < sizeof(buffer) / sizeof(long); i++) {
    buffer[i] = 0;
  }

  formula = random(0, 3);
  
}

void getRandomSeed()
{
  uint16_t seed = 0;
  for (int i = 0; i < 1E3; i++) seed += seed ^ analogRead(random(6));
  randomSeed(seed);
}

unsigned char counter = 0;

void loop() {
  if ((unsigned long)(millis() - ptime) > dtime) {
    

    switch (state) {
      case RUNNING:
        if(switch1) {
          update();
          counter++;    
          if ((counter % 32) == 0) {
            switch1 = false; // toggle
            lastOff = millis();
          }
        }else{
          if(millis() > (lastOff+offTime)){            
            switch1 = true;
          }  
        }
      break;

      case TESTING:
        // Serial.println("vago");
        // Serial.println(millis());
        // Serial.println(lastOff+offTime);
        formula = 3;
        offTime = 2000;
        if(millis() > (lastOff+offTime)){
          update();          
          lastOff = millis();
        }
      break;
    }

    ptime = millis();

  }
  render();
}


static inline char bytebeat(long t, int formula)
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
    case 3:
      return   85 << (ut % 2);
      break;
  }

}

unsigned char pattern(int it, int col, int pan) {
  // tengo que ubicar ese 'char' para esta formula para el lugar que le
  // corresponde en el tapiz
  return bytebeat(it + pan * COLS_PER_PANEL + col, formula);
}

void update() {
  iterations = (iterations + 1);
  //https://www.arduino.cc/reference/en/language/structure/bitwise-operators/bitshiftleft/
  for (int column = 0 ; column < COLS_PER_PANEL; column++) {
    // solo voy a 'generar' un cuarto del tapiz y lo voy a reflejar
    // vertical y horizontalmente
    for (int panel = 0; panel < NUM_PANELS / 2; panel++) {  

      // la columna para cada panel
      unsigned char line = pattern(iterations, column, panel);

      // no se que hace esta linea
      // line |= (unsigned long) (line >> column & 1) << 31;
      // line |= (unsigned long) (line >> column & 1) << 32;

      // --------------------------------------------
      // simetria horizontal
      // --------------------------------------------
      int id = column + (COLS_PER_PANEL * panel);
      buffer[id] =  long(line) ;
      buffer[id] |=  long(line) << 8;      
      buffer[id] |=  long(reverse(line)) << 16;
      buffer[id] |=  long(reverse(line)) << 24;

      // --------------------------------------------
      // simetria vertical
      // -------------------------------------------
      // tengo que invertir el id del panel y también la columna
      int antiId  = (COLS_PER_PANEL - 1 - column) + (COLS_PER_PANEL * (NUM_PANELS - panel - 1));
      buffer[antiId] =  long(line) ;
      buffer[antiId] |=  long(line) << 8;
      buffer[antiId] |=  long(reverse(line)) << 16;
      buffer[antiId] |=  long(reverse(line)) << 24;

    }
  }
}

void render() {

  for (int column = 0 ; column < COLS_PER_PANEL; column++) {
    // para cada panel tengo que pasarle 4 bytes
    for (int panel = 0; panel < NUM_PANELS; panel++) {  
      // esos bytes son...
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
  for (int column = 0 ; column < COLS_PER_PANEL; column++) {
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
