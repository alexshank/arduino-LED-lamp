/*
 * Alex Shank
 * Spring 2021
 */
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

/*
 * LED strip constants and object
 */
#define LED_PIN   9
#define LED_COUNT 60                    // same as using strip.numPixels(), but probably faster
#define BRIGHTNESS 20
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);


/*
 * constants needed for creating rainbow color effects
 * 
 * RAINBOW_RESOLUTION_FACTOR controls how finely the color 
 * wheel is divided (must be at least 1). If set to 1, you
 * have just enough color resolution to make each LED a
 * different color of the rainbow
 */
#define COLOR_WHEEL_LEN 65536           // for rainbow effects that use color wheel
#define RAINBOW_RESOLUTION_FACTOR 4     
#define NUM_RAINBOW_COLORS (LED_COUNT * RAINBOW_RESOLUTION_FACTOR)
#define MINUTES_25 10000


/*
 * button pins and button LED pins
 */
#define BUTTON_RED        12        // change color
#define BUTTON_RED_LED    A3
#define BUTTON_WHITE      11        // change effect
#define BUTTON_WHITE_LED  A4
#define BUTTON_GREEN      10        // start pomodoro timer
#define BUTTON_GREEN_LED  A5


/*
 * create array of effect functions to easily cycle through
 * NOTE: problem with this method is that every function has
 * to have the same function signature (unused parameters at times)
 */
#define NUM_OF_EFFECTS 2                  // used to wrap effectIndex back to first effect
typedef void (*func_pointer)(uint32_t);
void linearFill(uint32_t);            // function prototypes for all effect functions
void rainbowFull(uint32_t);
const func_pointer EFFECTS[] = {          // array of pointers to each effect function
  linearFill,
  rainbowFull
};


/*
 * variables used in interrupt service routine
 */
volatile bool endEffect = false;        // flags set in ISR
volatile bool timerActive = false;
volatile bool colorChange = false;
volatile bool effectChange = false;
volatile uint8_t colorIndex = 0;
volatile uint8_t effectIndex = 0;
volatile bool redButtonState = true;    // buttons are active low, so start start high/true
volatile bool whiteButtonState = true;
volatile bool greenButtonState = true;


/*
 * array of chosen colors to cycle through (strip.Color()
 * is used in setup() to calculate final color values passed
 * to the LED strip - so colors are not defined here)
 */
#define NUM_PRESET_COLORS 11
uint32_t COLORS[NUM_PRESET_COLORS];


/*
 * initialize pins, colors, LED strip, and serial communication
 */
void setup() {
  // start serial data transmision
  Serial.begin(9600);

  // create colors that can be be cycled through
  COLORS[0]  = strip.Color(255, 255, 255);      // white
  COLORS[1]  = strip.Color(  0, 127, 255);     // light blue
  COLORS[2]  = strip.Color(  0,   0, 255);     // dark blue
  COLORS[3]  = strip.Color(127,   0, 255);     // violet
  COLORS[4]  = strip.Color(255,   0, 255);     // hot pink
  COLORS[5]  = strip.Color(255,   0, 127);     // pink
  COLORS[6]  = strip.Color(255,   0,   0);     // red
  COLORS[7]  = strip.Color(255, 127,   0);     // orange
  COLORS[8]  = strip.Color(255, 255,   0);     // yellow
  COLORS[9]  = strip.Color(  0, 255,   0);     // dark green
  COLORS[10] = strip.Color(  0, 255, 255);     // cyan

  // initialize button inputs
  pinMode(BUTTON_RED, INPUT);
  digitalWrite(BUTTON_RED, HIGH);
  pciSetup(BUTTON_RED);
  pinMode(BUTTON_WHITE, INPUT);
  digitalWrite(BUTTON_WHITE, HIGH);
  pciSetup(BUTTON_WHITE);
  pinMode(BUTTON_GREEN, INPUT);
  digitalWrite(BUTTON_GREEN, HIGH);
  pciSetup(BUTTON_GREEN);

  // initialize button LEDs
  pinMode(BUTTON_RED_LED, OUTPUT);
  digitalWrite(BUTTON_RED_LED, LOW);
  pinMode(BUTTON_WHITE_LED, OUTPUT);
  digitalWrite(BUTTON_WHITE_LED, LOW);
  pinMode(BUTTON_GREEN_LED, OUTPUT);
  digitalWrite(BUTTON_GREEN_LED, LOW);

  // initialize LED strip
  strip.begin();                    // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();                     // Turn OFF all pixels ASAP
  strip.setBrightness(BRIGHTNESS);

  // main program loop ready to start
  Serial.write("Initialization complete!");
}


/*
 * main program loop
 */
void loop() {
  // run pomodoro timer if active
  if(timerActive){
    flashButton(BUTTON_GREEN_LED);
    digitalWrite(BUTTON_GREEN_LED, HIGH);
    unsigned long startTime = millis();
    while(millis() - startTime < MINUTES_25 and timerActive);
    digitalWrite(BUTTON_GREEN_LED, LOW);
    timerActive = false;
  }
  // update effect color
  else if(colorChange){
    flashButton(BUTTON_RED_LED);
    colorIndex = (colorIndex == NUM_PRESET_COLORS - 1) ? 0 : colorIndex + 1;
    colorChange = false;
  }
  // update effect
  else if(effectChange){
    flashButton(BUTTON_WHITE_LED);
    effectIndex = (effectIndex == NUM_OF_EFFECTS - 1) ? 0 : effectIndex + 1;
    effectChange = false;
  }

  // run effect function until interrupt sets this true
  endEffect = false;
  EFFECTS[effectIndex](COLORS[colorIndex]);
}


/*
 * setup pin change interrupts on D10, D11, D12 (buttons)
 */
// utility function for setting up interrupts
void pciSetup(byte pin){
  *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // enable pin
  PCIFR |= bit (digitalPinToPCICRbit(pin));                    // clear any outstanding interrupt
  PCICR |= bit (digitalPinToPCICRbit(pin));                    // enable interrupt for the group
}

// ISR for D8-D13
ISR(PCINT0_vect) {
  // poll all button states
  bool redNewState = digitalRead(BUTTON_RED);
  bool whiteNewState = digitalRead(BUTTON_WHITE);
  bool greenNewState = digitalRead(BUTTON_GREEN);
  
  // check if red button was pressed (color change)
  if(redNewState == false && redButtonState == true){
    colorChange = true;
    endEffect = true;   // color has changed, stop current effect being dispalyed
  }

  // check if white button was pressed (effect change)
  if(whiteNewState == false && whiteButtonState == true){
    effectChange = true;
    endEffect = true;   // effect has changed, stop current effect being dispalyed
  }

  // check if green button was pressed (start pomodoro timer)
  if(greenNewState == false && greenButtonState == true){
    timerActive = true;
    endEffect = true;
  }
  // check if green button was released (end pomodoro timer)
  else if(greenNewState == true && greenButtonState == false){
    timerActive = false;
  }

  // update saved button states
  redButtonState = redNewState;
  whiteButtonState = whiteNewState;
  greenButtonState = greenNewState;
}


/*
 * utility functions
 */
void flashButton(byte pin){
  for(int i = 0; i < 4; i++){
    digitalWrite(pin, HIGH);
    delay(100);
    digitalWrite(pin, LOW);
    delay(100);
  }
}

 
// setting pixel to black color turns it off
void clearStrip(){
  fillStrip(strip.Color(0, 0, 0));
}

// get next hue in rainbow for rainbow effects
long getNextRainbowHue(long currentPixelHue){
  long nextPixelHue = currentPixelHue + COLOR_WHEEL_LEN / NUM_RAINBOW_COLORS;
  return nextPixelHue % COLOR_WHEEL_LEN;  // if exceeded COLOR_WHEEL_LEN, wrap back into range
}

// convert an HSV value to a strip.Color value with gamma correction
uint32_t hueToColor(long pixelHue){
  return strip.gamma32(strip.ColorHSV(pixelHue));
}

// fill all pixels with color
void fillStrip(uint32_t color){
  for(int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}


/*
 * effect functions
 */
// fill strip with single color
void linearFill(uint32_t color) {
    clearStrip();
    for(int i=0; i < LED_COUNT and !endEffect; i++) {
      strip.setPixelColor(i, color);
      strip.show();
      delay(50);
    }
    delay(1000);
}

// fill entire strip with each rainbow color all at once
void rainbowFull(uint32_t color) {
  long pixelHue = 0;
  for(int i = 0; i < NUM_RAINBOW_COLORS and !endEffect; i++){
    pixelHue = getNextRainbowHue(pixelHue);
    fillStrip(hueToColor(pixelHue));
    delay(500);
  }
}
