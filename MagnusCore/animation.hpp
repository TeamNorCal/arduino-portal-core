#include <Adafruit_NeoPixel.h>

union Color {
    uint32_t w; // As packed value...w for 'word', lacking a better option
    struct {
        uint8_t blue;
        uint8_t green;
        uint8_t red;
        uint8_t white;
    } c; // As individual components...c for 'components'
};

// Convenience functions to initialize Color instances
// Not constructors so Color can be included in unions

Color ToColor(uint32_t color) {
    Color c;
    c.w = color;
    return c;
}

Color ToColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
    Color c;
    c.c.red = red;
    c.c.green = green;
    c.c.blue = blue;
    c.c.white = white;
    return c;
}

Color ToColor(uint8_t red, uint8_t green, uint8_t blue) {
    Color c;
    c.c.red = red;
    c.c.green = green;
    c.c.blue = blue;
    c.c.white = 0;
    return c;
}

// Animation State

struct CommonState {
    uint16_t numPixels;
    unsigned long startTime;
    unsigned long duration; // ms
};

struct MovingPulseState : public CommonState {
    Color color;
    uint16_t pulseLength;
    float pixelsPerMs;
};

struct PulseState : public CommonState {
    Color color;
};

struct RedFlashState : public CommonState {
    bool isRgbw;
};

struct SolidColorState : public CommonState {
    Color color;
};

union AnimationState {
    CommonState      common;
    MovingPulseState movingPulse;
    PulseState       pulse;
    RedFlashState    redFlash;
    SolidColorState  solid;
};

// Contract of animation implemnations
class Animation {
    public:
        void commonInit(unsigned long now, AnimationState& state, Adafruit_NeoPixel& strip) {
            CommonState& s = state.common;
            s.startTime = now;
            s.numPixels = strip.numPixels();
        }

        virtual void start(unsigned long now, AnimationState& state) {
            state.common.startTime = now;
        }

        virtual void doFrame(unsigned long now, AnimationState& state, Adafruit_NeoPixel& strip) =0;
        // How many cycles have completed?
        virtual uint32_t cyclesComplete(unsigned long now, const AnimationState& state) const {
            return (now - state.common.startTime) / state.common.duration;
        }
        // Checks to see if at least one full cycle has completed. Useful for
        // transient animations.
        bool done(unsigned long now, const AnimationState& state) const {
            return cyclesComplete(now, state) > 0;
        }
};

// Animation implemntations

// A pulse of light that moves down the strip of pixels
class MovingPulse : public Animation {
    public:
        // Initialize state to animation start point
        void init(unsigned long now, AnimationState& state, Adafruit_NeoPixel& strip, Color color) {
            commonInit(now, state, strip);
            MovingPulseState& s = state.movingPulse;
            s.duration = AnimationDuration;
            s.color = color;
            s.pulseLength = s.numPixels / PulseDivisor;
            s.pixelsPerMs = (float) s.numPixels / AnimationDuration;
        }

        // Draw a new frame of the animation
        virtual void doFrame(unsigned long now, AnimationState& state, Adafruit_NeoPixel& strip) override {
            MovingPulseState& s = state.movingPulse;

            unsigned long phase = (now - s.startTime) % AnimationDuration; // In ms
            uint16_t startPixel = phase * s.pixelsPerMs;
            for (uint16_t i = 0; i < s.numPixels; i++) {
                if (i >= startPixel && i < startPixel + s.pulseLength) {
                    strip.setPixelColor(i, s.color.w);
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
        // color - the color of the LEDs, the brightness of which will be modulated
        // initialPhase - in range [0.0, 1.0]; the offset within a cycle of the animation at which to start
        void init(unsigned long now, AnimationState& state, Adafruit_NeoPixel& strip, Color color, double initialPhase = 0.0) {
            commonInit(now, state, strip);
            PulseState& s = state.pulse;
            s.duration = AnimationDuration;
            s.color = color;
            // Modify the start time to effect a desired initial phase within the cyle
            double initialPhaseMs = initialPhase * AnimationDuration;
            s.startTime -= initialPhaseMs;
        }

        // Draw a new frame of the animation
        virtual void doFrame(unsigned long now, AnimationState& state, Adafruit_NeoPixel& strip) override {
            PulseState& s = state.pulse;
            //Serial.print("Doing pulse frame "); Serial.println(s.color.w, HEX);

            unsigned long phase = (now - s.startTime) % AnimationDuration; // In ms
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
        const float MinBrightness = 0.1;
};

// A white-red-white flash, appropriate for a portal going from owned to unowned
class RedFlash : public Animation {
    public:
        // Initialize state to animation start point
        //void init(AnimationState& state, Adafruit_NeoPixel& strip, uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
        void init(unsigned long now, AnimationState& state, Adafruit_NeoPixel& strip, bool isRgbw) {
            commonInit(now, state, strip);
            RedFlashState& s = state.redFlash;
            s.duration = AnimationDuration;
            s.isRgbw = isRgbw;
        }

        // Draw a new frame of the animation
        virtual void doFrame(unsigned long now, AnimationState& state, Adafruit_NeoPixel& strip) override {
            RedFlashState& s = state.redFlash;

            unsigned long phase = (now - s.startTime) % AnimationDuration; // In ms

            Color c;
            if (phase < StartRed || phase > EndRed) {
                float pct;
                if (phase < StartRed) {
                    pct = ((float) (StartRed - phase)) / (StartRed - 0L);
                } else /* phase > EndRed */ {
                    pct = ((float) (phase - EndRed)) / (AnimationDuration - EndRed);
                }
                float whiteAmt = 0xff * pct;
                if (s.isRgbw) {
                    c = ToColor(0, 0, 0, whiteAmt);
                } else {
                    c = ToColor(whiteAmt, whiteAmt, whiteAmt);
                }
            } else {
                float pct;
                if (phase < MaxRedStart) {
                    pct = ((float) (phase - StartRed)) / (MaxRedStart - StartRed);
                } else if (phase > MaxRedEnd) {
                    pct = ((float) (EndRed - phase)) / (EndRed - MaxRedEnd);
                } else /* all red */ {
                    pct = 1.0f;
                }
                uint8_t redAmt = 0xff * pct;
                if (s.isRgbw) {
                    c = ToColor(redAmt, 0, 0, 0);
                } else {
                    c = ToColor(redAmt, 0, 0, 0);
                }
            }

            for (uint16_t i = 0; i < s.numPixels; i++) {
                strip.setPixelColor(i, c.w);
            }
            strip.show();
        }

    private:
        const unsigned long AnimationDuration = 1000L; // ms
        const unsigned long StartRed = AnimationDuration * 0.25; // ms - when we start ramping up red
        const unsigned long MaxRedStart = AnimationDuration * 0.45; // ms - when we have full brightness red
        const unsigned long MaxRedEnd = AnimationDuration * 0.55; // ms - when we have full brightness red
        const unsigned long EndRed = AnimationDuration * 0.75; // ms - when we finish fading red and go back to white
};

// A white-red-white flash, appropriate for a portal going from owned to unowned
class SolidColor : public Animation {
    public:
        // Initialize state to animation start point
        void init(unsigned long now, AnimationState& state, Adafruit_NeoPixel& strip, Color c) {
            commonInit(now, state, strip);
            SolidColorState& s = state.solid;
            s.duration = AnimationDuration;
            s.color = c;
        }

        // Draw a new frame of the animation
        virtual void doFrame(unsigned long now, AnimationState& state, Adafruit_NeoPixel& strip) override {
            SolidColorState& s = state.solid;
            for (uint16_t i = 0; i < s.numPixels; i++) {
                strip.setPixelColor(i, s.color.w);
            }
            strip.show();
        }

    private:
        const unsigned long AnimationDuration = 1000L; // ms - arbitrary here
};

// Collection of all supported animations
struct Animations {
    MovingPulse movingPulse;
    Pulse pulse;
    RedFlash redFlash;
    SolidColor solid;
};
