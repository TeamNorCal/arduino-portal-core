#include <Adafruit_NeoPixel.h>
#include <stdlib.h>
#include <string.h>
#include "animation.hpp"
#include "circbuff.hpp"

// first communication pin for neo pixel string
const unsigned int BASE_PIN = 2;

const unsigned int NUM_STRINGS = 1;
//#define NUM_STRINGS 8 // one for each resonator

const uint16_t LEDS_PER_STRAND = 240;
const bool RGBW_SUPPORT = true;
const unsigned int QUEUE_SIZE = 3;

// Mask to clear 'upper case' ASCII bit
const char CASE_MASK = ~0x20;

enum Direction { NORTH = 0, NORTHEAST, EAST, SOUTHEAST, SOUTH, SOUTHWEST, WEST, NORTHWEST };
enum Ownership {  neutral = 0, enlightened, resistance };

// Static animation implementations singleton
Animations animations;

typedef CircularBuffer<Animation *, QUEUE_SIZE> QueueType;

AnimationState states[NUM_STRINGS][QUEUE_SIZE];
QueueType animationQueues[NUM_STRINGS];

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
//Adafruit_NeoPixel strip = Adafruit_NeoPixel(120, BASE_PIN, NEO_RGBW + NEO_KHZ800);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LEDS_PER_STRAND, BASE_PIN, (RGBW_SUPPORT ? NEO_GRBW : NEO_RGB) + NEO_KHZ800);


// Serial I/O
const int ioSize = 64;
char command[32];
int8_t in_index = 0;

void start_serial(void)
{
  in_index = 0;
  Serial.begin(115200);
}

bool collect_serial(void)
{
  while ( Serial.available() > 0 )
  {
    int8_t ch = Serial.read();
    if ( ch == '\n' )
    {
      command[in_index] = 0; // terminate
      if ( in_index > 0 )
      {
        in_index = 0;
        return true;
      }
    }
    else if (ch != '\r') // ignore CR
    {
      command[in_index] = ch;
      if ( in_index < (sizeof(command)/sizeof(command[0])) - 2 ) {
        in_index++;
      }
      //if( in_index >= 32 ) //sizeof(command) )
      //  in_index = 0;
    }
  }
  return false;
}

uint8_t getPercent(const char *buffer)
{
  unsigned long inVal = strtoul(buffer, NULL, 10);
  return uint8_t( constrain(inVal, 0, 100) );
}

uint8_t dir;
uint8_t percent;
Ownership owner;    // 0=neutral, 1=enl, 2=res


// the setup function runs once when you press reset or power the board
void setup()
{
  start_serial();
  strip.begin();
  uint8_t i;
  for ( i = 0; i < NUM_STRINGS; i++)
  {
    strip.setPin(i + BASE_PIN);
    strip.show(); // Initialize all pixels to 'off'
  }

  dir = 0;
  owner = neutral;
  percent = 0;
}

// the loop function runs over and over again forever
void loop()
{
    uint16_t i, val;
    // Voodoo magic - timing is slow without a delay thrown in
    // I think writing the LEDs disables interrupts which affects incremneting of time
    delay(10);

    if( collect_serial() )
    {
        // we have valid buffer of serial input
        char cmd = command[0];
        switch (cmd) {
            Color c;
            case '*':
                Serial.println("Magnus Core Node");
                break;
            case 'E':
            case 'e':
            case 'R':
            case 'r':
                owner = (cmd & CASE_MASK) == 'E' ? enlightened : resistance;
                percent = getPercent(&command[1]);
                uint8_t red, green, blue, white;
                c = ToColor(0x00, 
                        owner == enlightened ? 0xff : 0x00,
                        owner == resistance ? 0xff : 0x00,
                        0x00);
                for (int i = 0; i < NUM_STRINGS; i++) {
                    QueueType& animationQueue = animationQueues[i];
                    animationQueue.setTo(&animations.pulse);
                    unsigned int stateIdx = animationQueue.lastIdx();
                    double initialPhase = ((double) i) / NUM_STRINGS;
                    animations.pulse.init(states[i][stateIdx], strip, c, initialPhase);
                }
                break;

            case 'N':
            //case 'n':
                owner = neutral;
                percent = 20;
                if (RGBW_SUPPORT) {
                    c = ToColor(0x00, 0x00, 0x00, 0xff);
                } else {
                    c = ToColor(0xff, 0xff, 0xff);
                }
                for (int i = 0; i < NUM_STRINGS; i++) {
                    QueueType& animationQueue = animationQueues[i];
                    animationQueue.setTo(&animations.redFlash);
                    unsigned int stateIdx = animationQueue.lastIdx();
                    animations.redFlash.init(states[i][stateIdx], strip, RGBW_SUPPORT);
                    animationQueue.add(&animations.solid);
                    stateIdx = animationQueue.lastIdx();
                    animations.solid.init(states[i][stateIdx], strip, c);
                }
                break;

            default:
                Serial.print("Unkwown command "); Serial.println(cmd);
                break;
        }
        Serial.print((char *)command); Serial.print(" - ");Serial.print(command[0],DEC);Serial.print(": "); 
        Serial.print("owner "); Serial.print(owner,DEC); Serial.print(", percent "); Serial.println(percent,DEC); 
    }

    strip.setPin(dir + BASE_PIN);  // pick the string

    QueueType& animationQueue = animationQueues[dir];
    unsigned int queueSize = animationQueue.size();
    if (queueSize > 1) {
        if (animationQueue.peek()->done(states[dir][animationQueue.currIdx()])) {
            animationQueue.remove();
            queueSize--;
            animationQueue.peek()->start(states[dir][animationQueue.currIdx()]);
        }
    }
    if (queueSize > 0) {
        //strip.setBrightness(255);
        strip.setBrightness((uint8_t)((uint16_t)(255*(percent/100.0))));
        //pCurrAnimation->doFrame(states[0], strip);
        animationQueue.peek()->doFrame(states[dir][animationQueue.currIdx()], strip);
    }

    // Update one strand each time through the loop
    dir = (dir + 1) % NUM_STRINGS;
}


