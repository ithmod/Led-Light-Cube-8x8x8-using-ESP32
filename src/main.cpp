/*
 * 8x8x8 LED Cube Control Firmware
 * Architecture: ESP32 Microcontroller + 9x 74HC595 Shift Registers
 * Control Interface: Blynk IoT Platform & Local Push Button
 */

// Blynk Configuration - Must be defined before library imports
#define BLYNK_TEMPLATE_ID " " 
#define BLYNK_TEMPLATE_NAME " "
#define BLYNK_AUTH_TOKEN " "

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Arduino.h>

// Network Credentials
char ssid[] = " ";
char pass[] = " ";

// Shift Register & Input Pin Definitions
#define DATA_PIN    23
#define CLOCK_PIN   18
#define LATCH_PIN    5
#define BUTTON_PIN   4

// 3D Display Buffer: 8 Layers, each containing 8 bytes (64 columns)
volatile uint8_t cube[8][8];
volatile int currentLayer = 0;

// Hardware Timer variables for multiplexing routine
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Global State Variables
bool isPowerOn = true;
int speedMultiplier = 100;

// Buffer Manipulation
void clearCube() { memset((void*)cube, 0, sizeof(cube)); }
void fillCube()  { memset((void*)cube, 0xFF, sizeof(cube)); }

// Bit-banging SPI protocol for 74HC595
void IRAM_ATTR shiftOutByte(uint8_t val) {
  for (int bit = 7; bit >= 0; bit--) {
    digitalWrite(DATA_PIN, (val >> bit) & 1);
    digitalWrite(CLOCK_PIN, HIGH);
    digitalWrite(CLOCK_PIN, LOW);
  }
}

// Interrupt Service Routine (ISR) - Executes at high frequency to maintain Persistence of Vision (POV)
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  
  digitalWrite(LATCH_PIN, LOW);
  
  // Shift the active layer control bit
  shiftOutByte(1 << currentLayer);
  
  // Shift 64 bits (8 bytes) of column data for the current layer
  for (int c = 7; c >= 0; c--) shiftOutByte(cube[currentLayer][c]);
  
  digitalWrite(LATCH_PIN, HIGH);
  
  // Increment layer counter
  currentLayer = (currentLayer + 1) % 8;
  
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Initialize and start the hardware timer (approx. 100Hz refresh rate per full cycle)
void startDisplay() {
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1250, true);
  timerAlarmEnable(timer);
}

// Button Debouncing Variables
volatile bool btnPressed = false;
unsigned long lastDebounce = 0;

// Hardware Interrupt for physical button
void IRAM_ATTR onButton() {
  if (millis() - lastDebounce > 300) {
    btnPressed   = true;
    lastDebounce = millis();
  }
}

/* * ==========================================
 * 3D Animation Logic & Frame Generators
 * ==========================================
 */

int rainY[8][8];
void initRain() {
  for (int c = 0; c < 8; c++)
    for (int r = 0; r < 8; r++)
      rainY[c][r] = random(8);
}
void stepRain() {
  clearCube();
  for (int c = 0; c < 8; c++)
    for (int r = 0; r < 8; r++) {
      cube[rainY[c][r]][r] |= (1 << c);
      rainY[c][r] = (rainY[c][r] - 1 + 8) % 8;
    }
}

int waveOffset = 0;
void stepWave() {
  clearCube();
  for (int col = 0; col < 8; col++) {
    int h = constrain((int)(3.5 + 3.0 * sin((col + waveOffset) * 0.8)), 0, 7);
    for (int row = 0; row < 8; row++)
      cube[h][row] |= (1 << col);
  }
  waveOffset++;
}

int spiralOrder[64][2];
int spiralLen = 0, spiralHead = 0;
void buildSpiralOrder() {
  spiralLen = 0;
  int top=0, bot=7, lft=0, rgt=7;
  while (top<=bot && lft<=rgt) {
    for(int i=lft;i<=rgt;i++){spiralOrder[spiralLen][0]=top; spiralOrder[spiralLen++][1]=i;}
    top++;
    for(int i=top;i<=bot;i++){spiralOrder[spiralLen][0]=i;   spiralOrder[spiralLen++][1]=rgt;}
    rgt--;
    if(top<=bot){for(int i=rgt;i>=lft;i--){spiralOrder[spiralLen][0]=bot; spiralOrder[spiralLen++][1]=i;} bot--;}
    if(lft<=rgt){for(int i=bot;i>=top;i--){spiralOrder[spiralLen][0]=i;   spiralOrder[spiralLen++][1]=lft;} lft++;}
  }
}
void stepSpiral() {
  clearCube();
  for (int i = 0; i < 16; i++) {
    int idx = (spiralHead + i) % spiralLen;
    for (int l = 0; l < 8; l++)
      cube[l][spiralOrder[idx][0]] |= (1 << spiralOrder[idx][1]);
  }
  spiralHead = (spiralHead + 1) % spiralLen;
}

int sweepPos = 0;
bool sweepDir = true;
void stepSweep() {
  clearCube();
  for (int row = 0; row < 8; row++) cube[sweepPos][row] = 0xFF;
  if (sweepDir) { sweepPos++; if (sweepPos == 7) sweepDir = false; }
  else          { sweepPos--; if (sweepPos == 0) sweepDir = true;  }
}

float bounceZ = 0.0;
float bounceDZ = 0.4;
void stepBounce() {
  clearCube();
  bounceZ += bounceDZ;
  if (bounceZ >= 7.0) { bounceZ = 7.0; bounceDZ = -bounceDZ; }
  if (bounceZ <= 0.0) { bounceZ = 0.0; bounceDZ = -bounceDZ; }
  int iz = (int)round(bounceZ);
  for (int row = 0; row < 8; row++) cube[iz][row] = 0xFF;
}

int boxSize = 0;
bool boxGrow = true;
void stepExpandingBox() {
  clearCube();
  int lo = constrain(3 - boxSize, 0, 7);
  int hi = constrain(4 + boxSize, 0, 7);
  for (int l = lo; l <= hi; l++)
    for (int row = lo; row <= hi; row++) {
      bool edgeL = (l == lo || l == hi);
      bool edgeR = (row == lo || row == hi);
      if (edgeL || edgeR) cube[l][row] = 0xFF;
      else                cube[l][row] = (1 << lo) | (1 << hi);
    }
  if (boxGrow) { boxSize++; if (boxSize >= 3) boxGrow = false; }
  else         { boxSize--; if (boxSize <= 0) boxGrow = true;  }
}

int fillCount = 0;
bool fillClearing = false;
void stepRandomFill() {
  if (fillClearing) {
    clearCube(); fillCount = 0; fillClearing = false; return;
  }
  if (fillCount < 512) {
    int attempts = 0;
    while (attempts < 50) {
      int l = random(8), row = random(8), col = random(8);
      if (!(cube[l][row] & (1 << col))) {
        cube[l][row] |= (1 << col);
        fillCount++;
        break;
      }
      attempts++;
    }
  } else {
    fillClearing = true;
  }
}

int propAngle = 0;
void stepPropeller() {
  clearCube();
  float rad = propAngle * PI / 180.0;
  for (int arm = 0; arm < 4; arm++) {
    int x1 = constrain((int)(3.5 + arm * cos(rad)), 0, 7);
    int z1 = constrain((int)(3.5 + arm * sin(rad)), 0, 7);
    int x2 = constrain((int)(3.5 - arm * cos(rad)), 0, 7);
    int z2 = constrain((int)(3.5 - arm * sin(rad)), 0, 7);
    for (int row = 0; row < 8; row++) {
      cube[z1][row] |= (1 << x1);
      cube[z2][row] |= (1 << x2);
    }
  }
  propAngle = (propAngle + 10) % 360;
}

// Hex mapping for alphanumeric characters rendering
uint8_t getCharByte(char ch, int row) {
  switch(ch) {
    case '5': { uint8_t p[]={0x7C,0x40,0x40,0x7C,0x04,0x04,0x7C,0x00}; return p[row]; }
    case '4': { uint8_t p[]={0x08,0x18,0x28,0x48,0x7C,0x08,0x08,0x00}; return p[row]; }
    case '3': { uint8_t p[]={0x7C,0x04,0x04,0x3C,0x04,0x04,0x7C,0x00}; return p[row]; }
    case '2': { uint8_t p[]={0x7C,0x04,0x04,0x7C,0x40,0x40,0x7C,0x00}; return p[row]; }
    case '1': { uint8_t p[]={0x10,0x30,0x10,0x10,0x10,0x10,0x38,0x00}; return p[row]; }
    case 'L': { uint8_t p[]={0x40,0x40,0x40,0x40,0x40,0x40,0x7C,0x00}; return p[row]; }
    case 'E': { uint8_t p[]={0x7C,0x40,0x40,0x7C,0x40,0x40,0x7C,0x00}; return p[row]; }
    case 'D': { uint8_t p[]={0x78,0x44,0x44,0x44,0x44,0x44,0x78,0x00}; return p[row]; }
    case 'C': { uint8_t p[]={0x38,0x44,0x40,0x40,0x40,0x44,0x38,0x00}; return p[row]; }
    case 'U': { uint8_t p[]={0x44,0x44,0x44,0x44,0x44,0x44,0x38,0x00}; return p[row]; }
    case 'B': { uint8_t p[]={0x78,0x44,0x44,0x78,0x44,0x44,0x78,0x00}; return p[row]; }
    case 'I': { uint8_t p[]={0x38,0x10,0x10,0x10,0x10,0x10,0x38,0x00}; return p[row]; }
    case 'G': { uint8_t p[]={0x3C,0x40,0x40,0x4C,0x44,0x44,0x38,0x00}; return p[row]; }
    case 'H': { uint8_t p[]={0x44,0x44,0x44,0x7C,0x44,0x44,0x44,0x00}; return p[row]; }
    case 'T': { uint8_t p[]={0x7C,0x10,0x10,0x10,0x10,0x10,0x10,0x00}; return p[row]; }
    case ' ': { return 0x00; }
    default:  return 0x00;
  }
}

void drawCharOnCube(char ch) {
  for (int l = 0; l < 8; l++) {
    uint8_t rowByte = getCharByte(ch, 7 - l); 
    cube[l][3] |= rowByte; 
    cube[l][4] |= rowByte; 
  }
}

int countdownVal = 5;
void stepCountdown() {
  clearCube();
  if (countdownVal > 0) {
    drawCharOnCube('0' + countdownVal);
    countdownVal--;
  } else {
    countdownVal = 5; 
  }
}

const char textToDisplay[] = "LED CUBE LIGHT ";
const int textLen = sizeof(textToDisplay) - 1;
int textIdx = 0;

void stepTextDisplay() {
  clearCube();
  drawCharOnCube(textToDisplay[textIdx]);
  textIdx++;
  if (textIdx >= textLen) {
    textIdx = 0; 
  }
}

int fwState = 0;
int fwZ = 0;
void stepFirework() {
  clearCube();
  if (fwState == 0) {
    cube[fwZ][3] |= (1 << 3) | (1 << 4);
    cube[fwZ][4] |= (1 << 3) | (1 << 4);
    fwZ++;
    if (fwZ >= 6) { fwState = 1; fwZ = 0; }
  } else {
    int r = fwZ;
    if (r < 4) {
      for (int x = 3 - r; x <= 4 + r; x++) {
        for (int y = 3 - r; y <= 4 + r; y++) {
          if (x == 3 - r || x == 4 + r || y == 3 - r || y == 4 + r) {
            if (x >= 0 && x < 8 && y >= 0 && y < 8) {
              int topZ = constrain(6 + r, 0, 7);
              int botZ = constrain(6 - r, 0, 7);
              cube[topZ][y] |= (1 << x);
              cube[botZ][y] |= (1 << x);
            }
          }
        }
      }
      fwZ++;
    } else {
      fwState = 0; fwZ = 0;
    }
  }
}

int radarAngle = 0;
void stepRadar() {
  clearCube();
  float rad = radarAngle * PI / 180.0;
  for (int r = 0; r <= 4; r++) {
    int x = constrain((int)(3.5 + r * cos(rad)), 0, 7);
    int y = constrain((int)(3.5 + r * sin(rad)), 0, 7);
    for (int z = 0; z < 8; z++) {
      cube[z][y] |= (1 << x);
    }
  }
  radarAngle = (radarAngle + 15) % 360;
}

float surfOffset = 0;
void stepSineSurface() {
  clearCube();
  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 8; y++) {
      float val = sin((x + surfOffset) * 0.8) + cos((y + surfOffset) * 0.8);
      int z = constrain((int)(3.5 + val * 2.0), 0, 7);
      cube[z][y] |= (1 << x);
    }
  }
  surfOffset += 0.5;
}

int rbX = 0, rbY = 0, rbZ = 0, rbSize = 2;
int rbFrames = 0;
void stepRandomBoxes() {
  clearCube();
  if (rbFrames == 0) {
    rbSize = random(2, 5);
    rbX = random(0, 9 - rbSize);
    rbY = random(0, 9 - rbSize);
    rbZ = random(0, 9 - rbSize);
    rbFrames = 10;
  }
  for (int z = rbZ; z < rbZ + rbSize; z++) {
    for (int y = rbY; y < rbY + rbSize; y++) {
      for (int x = rbX; x < rbX + rbSize; x++) {
        bool edgeX = (x == rbX || x == rbX + rbSize - 1);
        bool edgeY = (y == rbY || y == rbY + rbSize - 1);
        bool edgeZ = (z == rbZ || z == rbZ + rbSize - 1);
        if ((edgeX && edgeY) || (edgeX && edgeZ) || (edgeY && edgeZ)) {
           cube[z][y] |= (1 << x);
        }
      }
    }
  }
  rbFrames--;
}

int crossSize = 0;
bool crossGrow = true;
void stepPulseCross() {
  clearCube();
  int min = constrain(3 - crossSize, 0, 7);
  int max = constrain(4 + crossSize, 0, 7);
  for(int i = min; i <= max; i++) {
    cube[3][3] |= (1 << i); cube[3][4] |= (1 << i);
    cube[4][3] |= (1 << i); cube[4][4] |= (1 << i);
    cube[3][i] |= (1 << 3) | (1 << 4);
    cube[4][i] |= (1 << 3) | (1 << 4);
    cube[i][3] |= (1 << 3) | (1 << 4);
    cube[i][4] |= (1 << 3) | (1 << 4);
  }
  if (crossGrow) {
    crossSize++;
    if (crossSize >= 3) crossGrow = false;
  } else {
    crossSize--;
    if (crossSize <= 0) crossGrow = true;
  }
}

// Animation Modes Enumeration
enum Mode {
  MODE_RAIN = 0, MODE_WAVE, MODE_SPIRAL, MODE_LAYER_SWEEP,
  MODE_BOUNCE, MODE_EXPANDING_BOX, MODE_RANDOM_FILL, MODE_PROPELLER,
  MODE_COUNTDOWN, MODE_TEXT, MODE_FIREWORK, MODE_RADAR,
  MODE_SINE_SURFACE, MODE_RANDOM_BOXES, MODE_PULSE_CROSS,
  MODE_COUNT
};

Mode currentMode = MODE_RAIN;

const char* modeNames[] = {
  "Rain","Wave","Spiral","Layer Sweep",
  "Bounce","Expanding Box","Random Fill","Propeller",
  "Countdown", "Text Display", "Firework", "Radar",
  "Sine Surface", "Random Boxes", "Pulse Cross"
};

// Base cycle intervals for each animation (in milliseconds)
const int modeInterval[] = {
  80, 100, 60, 120, 50, 200, 10, 50,
  1000, 600, 60, 50, 80, 50, 100
};

// Reset state variables upon switching to a new animation
void initMode(Mode m) {
  clearCube();
  waveOffset=0; spiralHead=0;
  sweepPos=0; sweepDir=true;
  bounceZ=0; bounceDZ=0.4;
  boxSize=0; boxGrow=true;
  fillCount=0; fillClearing=false;
  propAngle=0;
  countdownVal=5; textIdx=0; 
  fwState=0; fwZ=0; radarAngle=0; surfOffset=0;
  rbFrames=0; crossSize=0; crossGrow=true;
  
  switch (m) {
    case MODE_RAIN:   initRain();       break;
    case MODE_SPIRAL: buildSpiralOrder(); break;
    default: break;
  }
}

/* * ==========================================
 *