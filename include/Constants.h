#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__
#include <Arduino.h>
// JOYSTICK
#define JOYSTICK_ORIENTATION 1 // 0, 1 or 2 to set the angle of the joystick
#define JOYSTICK_DIRECTION 1   // 0/1 to flip joystick direction

typedef struct
{
    uint8_t settings_version; // stores the settings format version

    uint8_t led_type; // dotstar, neopixel

    uint16_t led_count;
    uint8_t led_brightness;

    uint8_t joystick_deadzone;
    uint16_t attack_threshold;

    uint8_t audio_volume;

    uint8_t lives_per_level;

    // saved statistics
    uint16_t games_played;
    uint32_t total_points;
    uint16_t high_score;
    uint16_t boss_kills;

} settings_t;

settings_t user_settings;
#endif