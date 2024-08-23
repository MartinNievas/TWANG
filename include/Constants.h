#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

#include <Arduino.h>

// change this whenever the saved settings are not compatible with a change
// it force a load from defaults.
#define SETTINGS_VERSION 4

// LED
#define NUM_LEDS             288
#define MIN_LEDS				60
#define MAX_LEDS				1000

#define BRIGHTNESS           150
#define MIN_BRIGHTNESS			10
#define MAX_BRIGHTNESS 			255

// JOYSTICK
#define JOYSTICK_ORIENTATION 0     // 0, 1 or 2 to set the angle of the joystick
#define JOYSTICK_DIRECTION   1     // 0/1 to flip joystick direction

#define ATTACK_THRESHOLD     40 // The threshold that triggers an attack

#define JOYSTICK_DEADZONE    7     // Angle to ignore
#define MIN_JOYSTICK_DEADZONE 3
#define MAX_JOYSTICK_DEADZONE 12

// PLAYER
#define MAX_PLAYER_SPEED    10     // Max move speed of the player

#define LIVES_PER_LEVEL		3
#define MIN_LIVES_PER_LEVEL 3
#define MAX_LIVES_PER_LEVEL 9

//AUDIO
#define MAX_VOLUME           10
#define MIN_VOLUME							0
#define MAX_VOLUME							10


typedef struct
{
    uint8_t settings_version; // stores the settings format version

    uint8_t led_type; // dotstar, neopixel

    uint16_t led_count;
    uint8_t led_brightness;

    uint8_t joystick_deadzone;
    int attack_threshold;

    uint8_t audio_volume;

    uint8_t lives_per_level;

    // saved statistics
    uint16_t games_played;
    uint32_t total_points;
    uint16_t high_score;
    uint16_t boss_kills;

} settings_t;

settings_t user_settings;

void reset_settings() {
	user_settings.settings_version = SETTINGS_VERSION;
	
	user_settings.led_brightness = BRIGHTNESS;
	
	user_settings.joystick_deadzone = JOYSTICK_DEADZONE;
	user_settings.attack_threshold = ATTACK_THRESHOLD;
	
	user_settings.audio_volume = MAX_VOLUME;
	
	user_settings.lives_per_level = LIVES_PER_LEVEL;
	
	user_settings.games_played = 0;
	user_settings.total_points = 0;
	user_settings.high_score = 0;	
	user_settings.boss_kills = 0;
	
	
}
#endif