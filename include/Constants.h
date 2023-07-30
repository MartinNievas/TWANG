#pragma once
#include "FastLED.h"

#define VIRTUAL_LED_COUNT 1000
#define LED_COUNT (144 * 2 - 5)

// Max move speed of the player per frame
#define MAX_PLAYER_SPEED 10

// this is set to the max, but the actual number used is set in FastLED.addLeds
CRGB leds[VIRTUAL_LED_COUNT];

int getLED(int pos){
    // The world is 1000 pixels wide, this converts world units into an LED number
    return constrain((int)map(pos, 0, VIRTUAL_LED_COUNT, 0, LED_COUNT-1), 0, LED_COUNT-1);
}
