/*
 * Alex Shank
 * Spring 2021
 */
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

// LED strip constants and object
#define LED_PIN   9
#define LED_COUNT 60
#define BRIGHTNESS 20
#define DELAY_TIME 25
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// button pins and button LED pins
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
#define NUM_OF_EFFECTS 4                  // used to wrap effectIndex back to first effect
typedef void (*func_pointer)(uint32_t, int);
void colorWipe(uint32_t, int);            // function prototypes for all effect functions
void theaterChase(uint32_t, int);
void rainbow(uint32_t, int);
void theaterChaseRainbow(uint32_t, int);
const func_pointer EFFECTS[] = {          // array of pointers to each effect function
  colorWipe,
  theaterChase,
  rainbow,
  theaterChaseRainbow
};


/*
 * variables used in interrupt service routine
 */
volatile bool endEffect = false;
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
#define NUM_OF_COLORS 12
uint32_t COLORS[NUM_OF_COLORS];

/*
 * initialize pins, colors, LED strip, and serial communication
 */
void setup() {
  // start serial data transmision
  Serial.begin(9600);

  // create colors that can be be cycled through
  COLORS[0]  = strip.Color(  0, 127, 255);     // light blue
  COLORS[1]  = strip.Color(  0,   0, 255);     // dark blue
  COLORS[2]  = strip.Color(127,   0, 255);     // violet
  COLORS[3]  = strip.Color(255,   0, 255);     // hot pink
  COLORS[4]  = strip.Color(255,   0, 127);     // pink
  COLORS[5]  = strip.Color(255,   0,   0);     // red
  COLORS[6]  = strip.Color(255, 127,   0);     // orange
  COLORS[7]  = strip.Color(255, 255,   0);     // yellow
  COLORS[8]  = strip.Color(127, 255,   0);     // light green
  COLORS[9]  = strip.Color(  0, 255,   0);     // dark green
  COLORS[10] = strip.Color(  0, 255, 127);     // lime green
  COLORS[11] = strip.Color(  0, 255, 255);     // cyan

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

  // TODO implement button LED effects to notify color/effect/timer change will occur
  
  endEffect = false;    // run effect function until interrupt sets this true
  EFFECTS[effectIndex](COLORS[colorIndex], DELAY_TIME);
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
    // check if red button was pressed (color change)
    bool redNewState = digitalRead(BUTTON_RED);
    if(redNewState == false && redButtonState == true){
      // red button pressed
      redButtonState = false;
      colorIndex = (colorIndex == NUM_OF_COLORS - 1) ? 0 : colorIndex + 1;
      endEffect = true;   // color has changed, stop current effect being dispalyed
    }else if(redNewState == true and redButtonState == false){
      // red button released
      redButtonState = true;
    }

    // check if white button was pressed (effect change)
    bool whiteNewState = digitalRead(BUTTON_WHITE);
    if(whiteNewState == false && whiteButtonState == true){
      // white button pressed
      whiteButtonState = false;
      effectIndex = (effectIndex == NUM_OF_EFFECTS - 1) ? 0 : effectIndex + 1;
      endEffect = true;   // color has changed, stop current effect being dispalyed
    }else if(whiteNewState == true and whiteButtonState == false){
      // white button released
      whiteButtonState = true;
    }

    // TODO check if green button input has changed (stationary button,
    // so button release will have functionality)
}

/*
 * effect functions
 */
// fill strip with single color
void colorWipe(uint32_t color, int wait) {
  while(!endEffect){
      for(int i=0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, color);
        strip.show();
        delay(wait);
      }
  }
}

// Theater-marquee-style chasing lights
void theaterChase(uint32_t color, int wait) {
  for(int a=0; a<10; a++) {
    for(int b=0; b<3; b++) {
      strip.clear();
      for(int c=b; c<strip.numPixels(); c += 3) {
        strip.setPixelColor(c, color);
      }
      strip.show(); // Update strip with new contents
      delay(wait);  // Pause for a moment
    }
  }
}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
// TODO the wait being passed currently is way to long, need to divide by some constant
void rainbow(uint32_t color, int wait) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this outer loop:
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256) {
    for(int i=0; i < strip.numPixels(); i++) { // For each pixel in strip...
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
    delay(wait);
  }
}

// Rainbow-enhanced theater marquee. Pass delay time (in ms) between frames.
// TODO the wait being passed currently is way to long, need to divide by some constant
void theaterChaseRainbow(uint32_t color, int wait) {
  int firstPixelHue = 0;     // First pixel starts at red (hue 0)
  for(int a=0; a<30; a++) {  // Repeat 30 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (strip.numPixels() steps):
        int      hue   = firstPixelHue + c * 65536L / strip.numPixels();
        uint32_t color = strip.gamma32(strip.ColorHSV(hue)); // hue -> RGB
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show();                // Update strip with new contents
      delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}
