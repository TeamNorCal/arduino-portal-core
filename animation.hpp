#include <Adafruit_NeoPixel.h>

// Animation State

struct CommonState {
    uint16_t numPixels;
    unsigned long startTime;
};

struct PulseState : public CommonState {
    uint16_t pulseLength;
    float pixelsPerMs;
};

union AnimationState {
    PulseState pulse;
};

// Contract of animation implemnations
class Animation {
    public:
        virtual void init(AnimationState& state, Adafruit_NeoPixel& strip) = 0;
        virtual void doFrame(AnimationState& state, Adafruit_NeoPixel& strip) =0;
};

// Animation implemntations

class Pulse : public Animation {
    public:
        // Initialize state to animation start point
        virtual void init(AnimationState& state, Adafruit_NeoPixel& strip) override {
            uint16_t numPixels = strip.numPixels();
            state.pulse.startTime = millis();
            state.pulse.numPixels = numPixels;
            state.pulse.pulseLength = numPixels / PulseDivisor;
            state.pulse.pixelsPerMs = (float) numPixels / AnimationDuration;
        }

        // Draw a new frame of the animation
        virtual void doFrame(AnimationState& state, Adafruit_NeoPixel& strip) override {

            unsigned long phase = (millis() - state.pulse.startTime) % AnimationDuration; // In ms
            uint16_t startPixel = phase * state.pulse.pixelsPerMs;
            //Serial.print("Start pixel: "); Serial.println(startPixel, DEC);
            for (uint16_t i = 0; i < state.pulse.numPixels; i++) {
                if (i >= startPixel && i < startPixel + state.pulse.pulseLength) {
                    strip.setPixelColor(i, 0, 0xff, 0);
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

// Collection of all supported animations
struct Animations {
    Pulse pulse;
};
