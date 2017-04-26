#include <Adafruit_NeoPixel.h>
#include <stdlib.h>
#include <string.h>
#include "animation.hpp"
#include "circbuff.hpp"

// first communication pin for neo pixel string
#define BASE_PIN 2

#define NUM_STRINGS 1 // one for each resonator
//#define NUM_STRINGS 8 // one for each resonator

const uint16_t LEDS_PER_STRAND = 50;
const bool RGBW_SUPPORT = false;

// Mask to clear 'upper case' ASCII bit
const char CASE_MASK = ~0x20;

enum Direction { NORTH = 0, NORTHEAST, EAST, SOUTHEAST, SOUTH, SOUTHWEST, WEST, NORTHWEST };
enum Ownership {  neutral = 0, enlightened, resistance };

struct pixel_string {
  uint16_t phase;
  uint32_t timing;
};

// Static animation implementations singleton
Animations animations;

AnimationState states[NUM_STRINGS];

//Animation *pCurrAnimation;

CircularBuffer<Animation *> animationQueue;

pixel_string strings[NUM_STRINGS];

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
//Adafruit_NeoPixel strip = Adafruit_NeoPixel(120, BASE_PIN, NEO_RGBW + NEO_KHZ800);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(50, BASE_PIN, (RGBW_SUPPORT ? NEO_RGBW : NEO_RGB) + NEO_KHZ800);


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
    else if ( ch == 0x0a || ch == 0x13 ) // carriage return or linefeed (ignore
    {
    }
    else
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
uint8_t owner;    // 0=neutral, 1=enl, 2=res


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

    strings[i].phase = 0;
    strings[i].timing = 0;
  }

  dir = NORTH; // begin on the north resonator
  owner = neutral;
  percent = 0;
}

// the loop function runs over and over again forever
void loop()
{
  uint16_t i, val;

  if( collect_serial() )
  {
      // we have valid buffer of serial input
      char cmd = command[0] & CASE_MASK;
      switch (cmd) {
          case '*':
            Serial.println("Magnus Core Node");
            break;
          case 'E':
          case 'R':
              owner = cmd == 'E' ? enlightened : resistance;
              percent = getPercent(&command[1]);
              uint8_t red, green, blue, white;
              Color c;
              c = ToColor(0x00, 
                  owner == enlightened ? 0xff : 0x00,
                  owner == resistance ? 0xff : 0x00,
                  0x00);
              animations.pulse.init(states[0], strip, c);
//              pCurrAnimation = &animations.pulse;
              animationQueue.setTo(&animations.pulse);
              break;

          case 'N':
              owner = neutral;
              animations.redFlash.init(states[0], strip, RGBW_SUPPORT);
              animationQueue.setTo(&animations.redFlash);
              animationQueue.add(&animations.solid);
              percent = 20;
              break;

          default:
              Serial.print("Unkwown command "); Serial.println(cmd);
              break;
      }
      Serial.print((char *)command); Serial.print(" - ");Serial.print(command[0],DEC);Serial.print(": "); 
      Serial.print("owner "); Serial.print(owner,DEC); Serial.print(", percent "); Serial.println(percent,DEC); 
  }

  if(strings[dir].timing < millis() )
  {
      strings[dir].timing += 10; // every 100 milliseconds we will check this direction

      strip.setPin(dir+BASE_PIN);  // pick the string

      uint8_t red, green, blue, white;
      red = 0x20; green = 0; blue = 0x20; white = 0x20;
      bool useAnimations = false;
      if( owner == neutral ) 
      {
          red = 0x40; green = 0x40; blue = 0x40; white = 0x40;
          //      red = 0; green = 0; blue = 0; white = 0x40;
          useAnimations = true;
      }
      else if( owner == resistance )
      {
          red = 0; green = 0x1f; blue = 0xff;; white = 0;
          useAnimations = true;
      }
      else if( owner == enlightened )
      {
          red = 0x1f; green = 0xff; blue = 0; white = 0;
          useAnimations = true;
      }

      if (useAnimations) {
          unsigned int queueSize = animationQueue.size();
          if (queueSize > 1) {
              if (animationQueue.peek()->done(states[0])) {
                  animationQueue.remove();
                  queueSize--;
                  animationQueue.peek()->start(states[0]);
              }
          }
          if (queueSize > 0) {
              //strip.setBrightness(255);
              strip.setBrightness((uint8_t)((uint16_t)(255*(percent/100.0))));
              //pCurrAnimation->doFrame(states[0], strip);
              animationQueue.peek()->doFrame(states[0], strip);
          }
      } else {
          for(i=0; i < strip.numPixels(); i++)
          {
              //strip.setPixelColor(i, green,blue,red); // for RGB order is funny?
              //        strip.setPixelColor(i, green, red, blue,white);
              strip.setPixelColor(i, red, green, blue);
              //        float scale = ((millis()/100)%100)/200.0 + 0.5;
              float scale = 1.0f;
              float pct = percent * scale;
              //        Serial.print("Percent: "); Serial.println(pct, 4);
              strip.setBrightness((uint8_t)((uint16_t)(255*(pct/100.0))));
          }
          strip.show();
      }

      dir++;
      if( dir >= NUM_STRINGS )
          dir = 0;
  }


}

