#include <Adafruit_NeoPixel.h>

union Color {
    uint32_t w; // As packed value...w for 'word', lacking a better option
    struct {
        uint8_t white;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } c; // As individual components...c for 'components'
};

// Convenience functions to initialize Color instances
// Not constructors so Color can be included in unions

Color ToColor(uint32_t color) {
    Color c;
    c.w = color;
}

Color ToColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
    Color c;
    c.c.red = red;
    c.c.green = green;
    c.c.blue = blue;
    c.c.white = white;
}

// Animation State

struct CommonState {
    uint16_t numPixels;
    unsigned long startTime;
};

struct MovingPulseState : public CommonState {
    uint32_t color;
    uint16_t pulseLength;
    float pixelsPerMs;
};

struct PulseState : public CommonState {
    Color color;
};

union AnimationState {
    MovingPulseState movingPulse;
    PulseState       pulse;
};

// Contract of animation implemnations
class Animation {
    public:
//        virtual void init(AnimationState& state, Adafruit_NeoPixel& strip) = 0;
        virtual void doFrame(AnimationState& state, Adafruit_NeoPixel& strip) =0;
};

// Animation implemntations

// A pulse of light that moves down the strip of pixels
class MovingPulse : public Animation {
    public:
        // Initialize state to animation start point
        void init(AnimationState& state, Adafruit_NeoPixel& strip, uint32_t color) {
            MovingPulseState& s = state.movingPulse;
            uint16_t numPixels = strip.numPixels();
            s.startTime = millis();
            s.color = color;
            s.pulseLength = numPixels / PulseDivisor;
            s.numPixels = numPixels;
            s.pixelsPerMs = (float) numPixels / AnimationDuration;
        }

        // Draw a new frame of the animation
        virtual void doFrame(AnimationState& state, Adafruit_NeoPixel& strip) override {
            MovingPulseState& s = state.movingPulse;

            unsigned long phase = (millis() - s.startTime) % AnimationDuration; // In ms
            uint16_t startPixel = phase * s.pixelsPerMs;
            //Serial.print("Start pixel: "); Serial.println(startPixel, DEC);
            for (uint16_t i = 0; i < s.numPixels; i++) {
                if (i >= startPixel && i < startPixel + s.pulseLength) {
                    strip.setPixelColor(i, s.color);
                } else {
                    strip.setPixelColor(i, 0, 0, 0);
                }
            }
            strip.show();
        }

    private:
        const unsigned long AnimationDuration = 4000l; // ms
        const uint16_t PulseDivisor = 8; // 1/PulseDivisor defines the size of the pulse
};

// A 'breathing'-type pulse effect - the LEDs brighten and dim repeatedly
class Pulse : public Animation {
    public:
        // Initialize state to animation start point
        //void init(AnimationState& state, Adafruit_NeoPixel& strip, uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
        void init(AnimationState& state, Adafruit_NeoPixel& strip, Color color) {
            PulseState& s = state.pulse;
            uint16_t numPixels = strip.numPixels();
            s.numPixels = numPixels;
            s.startTime = millis();
            s.color = color;
        }

        // Draw a new frame of the animation
        virtual void doFrame(AnimationState& state, Adafruit_NeoPixel& strip) override {
            PulseState& s = state.pulse;

            unsigned long phase = (millis() - s.startTime) % AnimationDuration; // In ms
            float brightness;
            if (phase < BrightPoint) {
                brightness = ((float) phase) / BrightPoint;
            } else {
                brightness= ((float) AnimationDuration - phase) / (AnimationDuration - BrightPoint);
            }
            float factor = (1.0f - MinBrightness) * brightness + MinBrightness;
            uint8_t white = s.color.c.white * factor;
            uint8_t red = s.color.c.red * factor;
            uint8_t green = s.color.c.green * factor;
            uint8_t blue = s.color.c.blue * factor;

            for (uint16_t i = 0; i < s.numPixels; i++) {
                strip.setPixelColor(i, red, green, blue,white);
            }
            strip.show();
        }

    private:
        const unsigned long AnimationDuration = 4000l; // ms
        const float FullBrightnessPoint = 0.5; // As a fraction of 1. At what point in the cycle should we achieve full brightness?
        const unsigned long BrightPoint = AnimationDuration * FullBrightnessPoint;
        const float MinBrightness = 0.25;
};

// Collection of all supported animations
struct Animations {
    MovingPulse movingPulse;
    Pulse pulse;
};
