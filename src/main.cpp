/* 
	TWANG 
	
	Code at ..
	
	https://github.com/bdring/TWANG
	
	Based on original code by Critters/TWANG	
	
	https://github.com/Critters/TWANG

*/
#define VERSION "2023-06-25"

// Required libs
#include "FastLED.h"
#include "Wire.h"
#include "toneAC.h"
#include "RunningMedian.h"
#include <vl53l4cd_class.h>

#include <stdint.h> // uint8_t type variables

// Included libs
#include "Enemy.h"
#include "Particle.h"
#include "Spawner.h"
#include "Lava.h"
#include "Boss.h"
#include "Conveyor.h"


// LED Strip Setup
#define LED_DATA_PIN             3
#define LED_CLOCK_PIN            13
#define VIRTUAL_LED_COUNT 1000
#define LED_COUNT 144
#define LED_BRIGHTNESS 150

#define APA102_CONVEYOR_BRIGHTNES 8
#define APA102_LAVA_OFF_BRIGHTNESS 4

#define MAX_PLAYER_SPEED     10     // Max move speed of the player
#define LIVES_PER_LEVEL		 3
#define DEFAULT_ATTACK_WIDTH 100  // Width of the wobble attack, world is 1000 wide
#define ATTACK_DURATION      500    // Duration of a wobble attack (ms)
#define BOSS_WIDTH           40

#define SCREENSAVER_TIMEOUT              30000  // time until screen saver

#define MIN_REDRAW_INTERVAL  16    // Min redraw interval (ms) 33 = 30fps / 16 = 63fps
#define USE_GRAVITY          0     // 0/1 use gravity (LED strip going up wall)
#define BEND_POINT           750   // 0/1000 point at which the LED strip goes up the wall
//#define USE_LIFELEDS  // uncomment this to make Life LEDs available (not used in the B. Dring enclosure)

unsigned long previousMillis = 0;           // Time of the last redraw
int levelNumber = 0;

const int TOF_ZERO = 450;   // a fixed zero-point or TOF_RANGE to set it dynamically [mm]
const int TOF_RANGE = 300;  // max range from center until reaching max speed [mm]
const int TOF_NOTHING = -999; // "distance" reported when out of range
const int TOF_MAX = 900;    // minimum distance considered out of range [mm]
int tofOffset = TOF_NOTHING;

int attack_width = DEFAULT_ATTACK_WIDTH;
unsigned long attackMillis = 0;             // Time the attack started
bool attacking = 0;                // Is the attack in progress?
bool attackAvailable = false;

uint8_t AUDIO_VOLUME = 10; // 0-10

CRGB leds[VIRTUAL_LED_COUNT]; // this is set to the max, but the actual number used is set in FastLED.addLeds below
RunningMedian MPUDistanceSamples = RunningMedian(3);

#define DEV_I2C Wire
#define SerialPort Serial
#define interruptPin PIN2
VL53L4CD sensor_vl53l4cd_sat(&DEV_I2C, PIN_A1);
volatile bool tof_measurement_exists = false;

enum stages {
    STARTUP,
    PLAY,
    WIN,
    DEAD,
    SCREENSAVER,
    BOSS_KILLED,
    GAMEOVER
} stage;

int score;
unsigned long stageStartTime;               // Stores the time the stage changed for stages that are time based
unsigned long lastInputTime = 0;
int playerPosition;                // Stores the player position
int playerPositionModifier;        // +/- adjustment to player position
unsigned long killTime;
uint8_t lives;
bool lastLevel = false;

// TODO all animation durations should be defined rather than literals 
// because they are used in main loop and some sounds too.
#define STARTUP_WIPEUP_DUR 200
#define STARTUP_SPARKLE_DUR 1300
#define STARTUP_FADE_DUR 1500

#define GAMEOVER_SPREAD_DURATION 1000
#define GAMEOVER_FADE_DURATION 1500

#define WIN_FILL_DURATION 500     // sound has a freq effect that might need to be adjusted
#define WIN_CLEAR_DURATION 1000
#define WIN_OFF_DURATION 1200

#ifdef USE_LIFELEDS
#define LIFE_LEDS 3
	int lifeLEDs[LIFE_LEDS] = {7, 6, 5}; // these numbers are Arduino GPIO numbers...this is not used in the B. Dring enclosure design
#endif


// POOLS
#define ENEMY_COUNT 10
Enemy enemyPool[ENEMY_COUNT] = {
        Enemy(), Enemy(), Enemy(), Enemy(), Enemy(), Enemy(), Enemy(), Enemy(), Enemy(), Enemy()
};


#define PARTICLE_COUNT 40
Particle particlePool[PARTICLE_COUNT] = {
        Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle()
};

#define SPAWN_COUNT 2
Spawner spawnPool[SPAWN_COUNT] = {
        Spawner(), Spawner()
};

#define LAVA_COUNT 4
Lava lavaPool[LAVA_COUNT] = {
        Lava(), Lava(), Lava(), Lava()
};

#define CONVEYOR_COUNT 2
Conveyor conveyorPool[CONVEYOR_COUNT] = {
        Conveyor(), Conveyor()
};

Boss boss = Boss();

void loadLevel();
void spawnBoss();
void moveBoss();
void spawnEnemy(int pos, int dir, int speed, int wobble);
void spawnLava(int left, int right, int ontime, int offtime, int offset, int state);
void spawnConveyor(int startPoint, int endPoint, int dir);
void cleanupLevel();
void levelComplete();
void nextLevel();
void die();
void tickStartup(unsigned long mm);
void tickEnemies();
void tickBoss();
void drawPlayer();
void drawExit();
void tickSpawners(unsigned long mm);
void tickLava(unsigned long mm);
bool tickParticles();
void tickConveyors(unsigned long mm);
void tickBossKilled(unsigned long mm);
void tickDie(unsigned long mm);
void tickGameover(long mm);
void tickWin(long mm);
void drawLives();
void drawAttack();
int getLED(int pos);
bool inLava(int pos);
void screenSaverTick();
void getInput();
void SFXFreqSweepWarble(int duration, int elapsedTime, int freqStart, int freqEnd, int warble);
void SFXFreqSweepNoise(int duration, int elapsedTime, int freqStart, int freqEnd, uint8_t noiseFactor);
void SFXtilt(int amount);
void SFXattacking();
void SFXdead();
void SFXgameover();
void SFXkill();
void SFXwin();
void SFXbosskilled();
void SFXcomplete();
long map_constrain(long x, long in_min, long in_max, long out_min, long out_max);
void updateLives();

void tof_interrupt() {
    tof_measurement_exists = true;
}

void setup() {

    Serial.begin(115200);
    Serial.print("\r\nTWANG VERSION: ");
    Serial.println(VERSION);

    // MPU
    Wire.begin();

    pinMode(interruptPin, INPUT_PULLUP);
    attachInterrupt(interruptPin, tof_interrupt, FALLING);

    DEV_I2C.begin(); // Initialize I2C bus.
    sensor_vl53l4cd_sat.begin(); // Configure VL53L4CD satellite component.
    sensor_vl53l4cd_sat.VL53L4CD_Off(); // Switch off VL53L4CD satellite component.
    sensor_vl53l4cd_sat.InitSensor(); //Initialize VL53L4CD satellite component.
    sensor_vl53l4cd_sat.VL53L4CD_SetRangeTiming(24, 0);
    sensor_vl53l4cd_sat.VL53L4CD_StartRanging(); // Start Measurements

    // Fast LED
    FastLED.addLeds<APA102, LED_DATA_PIN, LED_CLOCK_PIN, BGR, DATA_RATE_MHZ(20)>(leds, LED_COUNT);
    FastLED.setBrightness(LED_BRIGHTNESS);
    FastLED.setDither(1);

    // Life LEDs
#ifdef USE_LIFELEDS
    for(int i = 0; i<LIFE_LEDS; i++){
        pinMode(lifeLEDs[i], OUTPUT);
        digitalWrite(lifeLEDs[i], HIGH);
    }
#endif

    stage = STARTUP;
    stageStartTime = millis();
    lives = LIVES_PER_LEVEL;
}

void loopPlay(unsigned long mm) {
    if (attacking) {
        if (attackMillis + ATTACK_DURATION < mm) { // end attack
            attacking = false;
        } else { // while attack in progress
            SFXattacking();
        }
    } else { // not attacking currently
        if (attackAvailable && tofOffset == TOF_NOTHING) { // start attack
            attackMillis = mm;
            attacking = true;
            attackAvailable = false;
        } else { // If still not attacking, move!
            playerPosition += playerPositionModifier; // forced move by elevators
            if (tofOffset == TOF_NOTHING) {
                SFXcomplete();
            } else { // some input by the player exists
                attackAvailable = true;
                SFXtilt(tofOffset);
                int moveAmount = tofOffset * MAX_PLAYER_SPEED / TOF_RANGE;
                // not needed: moveAmount = constrain(moveAmount, -MAX_PLAYER_SPEED, MAX_PLAYER_SPEED);
                playerPosition -= moveAmount;
                if(playerPosition < 0) {
                    playerPosition = 0;
                }
                if (playerPosition >= VIRTUAL_LED_COUNT) { // end of strip reached
                    if (boss.Alive()) {
                        // stop player from leaving if boss is alive
                        playerPosition = VIRTUAL_LED_COUNT - 1;
                    } else { // Reached exit!
                        levelComplete();
                        return;
                    }
                }
            }
            if(inLava(playerPosition)){
                die();
            }
        }
    }
    FastLED.clear();
    tickConveyors(mm);
    tickSpawners(mm);
    tickBoss();
    tickLava(mm);
    tickEnemies();
    drawPlayer();
    drawAttack();
    drawExit();
}

void loop() {
    auto mm = millis();

    if (mm - previousMillis >= MIN_REDRAW_INTERVAL) {
        getInput();
        previousMillis = mm;

        if(tofOffset != TOF_NOTHING){ // we are active: hand is moving
            lastInputTime = mm;
            if(stage == SCREENSAVER){
                levelNumber = -1;
                stageStartTime = mm;
                stage = WIN;
            }
        }else{
            if(lastInputTime + SCREENSAVER_TIMEOUT < mm){
                stage = SCREENSAVER;
            }
        }
        if(stage == SCREENSAVER){
            screenSaverTick();
        }else if(stage == STARTUP){
            if (stageStartTime + STARTUP_FADE_DUR > mm) {
                tickStartup(mm);
            } else {
                SFXcomplete();
                levelNumber = 0;
                loadLevel();
            }
        }else if(stage == PLAY){
            loopPlay(mm);
        }else if(stage == DEAD){
            // DEAD			
            SFXdead();
            FastLED.clear();
            tickDie(mm);
            if(!tickParticles()){
                loadLevel();
            }
        }else if(stage == WIN){// LEVEL COMPLETE   
            tickWin(mm);
        }else if(stage == BOSS_KILLED){
            tickBossKilled(mm);
        } else if (stage == GAMEOVER) {
            if (stageStartTime+GAMEOVER_FADE_DURATION > mm)
            {
                tickGameover(mm);
            }
            else
            {
                FastLED.clear();
                levelNumber = 0;
                lives = LIVES_PER_LEVEL;
                loadLevel();
            }
        }

        FastLED.show();
    }
}

// ---------------------------------
// ------------ LEVELS -------------
// ---------------------------------
void loadLevel(){
    // leave these alone
    FastLED.setBrightness(LED_BRIGHTNESS);
    updateLives();
    cleanupLevel();
    lastLevel = false; // this gets changed on the boss level

    /// Defaults...OK to change the following items in the levels below
    attack_width = DEFAULT_ATTACK_WIDTH;
    playerPosition = 0;

    /* ==== Level Editing Guide ===============
    Level creation is done by adding to or editing the switch statement below

    You can add as many levels as you want but you must have a "case"
    for each level. The case numbers must start at 0 and go up without skipping any numbers.

    Don't edit case 0 or the last (boss) level. They are special cases and editing them could
    break the code.

    TWANG uses a virtual 1000 LED grid. It will then scale that number to your strip, so if you
    want something in the middle of your strip use the number 500. Consider the size of your strip
    when adding features. All time values are specified in milliseconds (1/1000 of a second)

    You can add any of the following features.

    Enemies: You add up to 10 enemies with the spawnEnemy(...) functions.
        spawnEnemy(position, direction, speed, wobble);
            position: Where the enemy starts
            direction: If it moves, what direction does it go 0=down, 1=away
            speed: How fast does it move. Typically 1 to 4.
            wobble: 0=regular movement, 1=bouncing back and forth use the speed value
                to set the length of the wobble.

    Spawn Pools: This generates and endless source of new enemies. 2 pools max
        spawnPool[index].Spawn(position, rate, speed, direction, activate);
            index: You can have up to 2 pools, use an index of 0 for the first and 1 for the second.
            position: The location the enemies with be generated from.
            rate: The time in milliseconds between each new enemy
            speed: How fast they move. Typically 1 to 4.
            direction: Directions they go 0=down, 1=away
            activate: The delay in milliseconds before the first enemy

    Lava: You can create 4 pools of lava.
        spawnLava(left, right, ontime, offtime, offset, state);
            left: the lower end of the lava pool
            right: the upper end of the lava pool
            ontime: How long the lave stays on.
            offset: the delay before the first switch
            state: does it start on or off_dir

    Conveyor: You can create 2 conveyors.
        spawnConveyor(startPoint, endPoint, speed)
            startPoint: The close end of the conveyor
            endPoint: The far end of the conveyor
            speed: positive = away, negative = towards you (must be less than +/- player speed)

    ===== Other things you can adjust per level ================

        Player Start position:
            playerPosition = xxx;


        The size of the TWANG attack
            attack_width = xxx;


    */
    switch(levelNumber){
        case 0: // basic introduction 
            playerPosition = 400;
            spawnEnemy(1, 0, 0, 0);
            break;
        case 1:
            // Slow moving enemy			
            spawnEnemy(900, 0, 1, 0);
            break;
        case 2:
            // Spawning enemies at exit every 2 seconds
            spawnPool[0].Spawn(1000, 3000, 2, 0, 0);
            break;
        case 3:
            // Lava intro
            spawnLava(400, 490, 2000, 2000, 0, Lava::OFF);
            spawnEnemy(350, 0, 1, 0);
            spawnPool[0].Spawn(1000, 5500, 3, 0, 0);

            break;
        case 4:
            // Sin enemy
            spawnEnemy(700, 1, 7, 275);
            spawnEnemy(500, 1, 5, 250);
            break;
        case 5:
            // Sin enemy swarm
            spawnEnemy(700, 1, 7, 275);
            spawnEnemy(500, 1, 5, 250);

            spawnEnemy(600, 1, 7, 200);
            spawnEnemy(800, 1, 5, 350);

            spawnEnemy(400, 1, 7, 150);
            spawnEnemy(450, 1, 5, 400);

            break;
        case 6:
            // Conveyor
            spawnConveyor(100, 600, -4);
            spawnEnemy(800, 0, 0, 0);
            break;
        case 7:
            // Conveyor of enemies
            spawnConveyor(50, 1000, 4);
            spawnEnemy(300, 0, 0, 0);
            spawnEnemy(400, 0, 0, 0);
            spawnEnemy(500, 0, 0, 0);
            spawnEnemy(600, 0, 0, 0);
            spawnEnemy(700, 0, 0, 0);
            spawnEnemy(800, 0, 0, 0);
            spawnEnemy(900, 0, 0, 0);
            break;
        case 8:   // spawn train;
            spawnPool[0].Spawn(900, 1300, 2, 0, 0);
            break;
        case 9:   // spawn train skinny attack width;
            attack_width = 32;
            spawnPool[0].Spawn(900, 1800, 2, 0, 0);
            break;
        case 10:  // evil fast split spawner
            spawnPool[0].Spawn(550, 1500, 2, 0, 0);
            spawnPool[1].Spawn(550, 1500, 2, 1, 0);
            break;
        case 11: // split spawner with exit blocking lava
            spawnPool[0].Spawn(500, 1200, 2, 0, 0);
            spawnPool[1].Spawn(500, 1200, 2, 1, 0);
            spawnLava(900, 950, 2200, 800, 2000, Lava::OFF);
            break;
        case 12:
            // Lava run
            spawnLava(195, 300, 2000, 2000, 0, Lava::OFF);
            spawnLava(400, 500, 2000, 2000, 0, Lava::OFF);
            spawnLava(600, 700, 2000, 2000, 0, Lava::OFF);
            spawnPool[0].Spawn(1000, 3800, 4, 0, 0);
            break;
        case 13:
            // Sin enemy #2 practice (slow conveyor)
            spawnEnemy(700, 1, 7, 275);
            spawnEnemy(500, 1, 5, 250);
            spawnPool[0].Spawn(1000, 5500, 4, 0, 3000);
            spawnPool[1].Spawn(0, 5500, 5, 1, 10000);
            spawnConveyor(100, 900, -2);
            break;
        case 14:
            // Sin enemy #2 (fast conveyor)
            spawnEnemy(800, 1, 7, 275);
            spawnEnemy(700, 1, 7, 275);
            spawnEnemy(500, 1, 5, 250);
            spawnPool[0].Spawn(1000, 3000, 4, 0, 3000);
            spawnPool[1].Spawn(0, 5500, 5, 1, 10000);
            spawnConveyor(100, 900, -4);
            break;
        case 15: // (don't edit last level)
            // Boss this should always be the last level			
            spawnBoss();
            break;
    }
    stageStartTime = millis();
    stage = PLAY;
}

void spawnBoss(){
    lastLevel = true;
    boss.Spawn();
    moveBoss();
}

void moveBoss(){
    int spawnSpeed = 1800;
    if(boss._lives == 2) spawnSpeed = 1600;
    if(boss._lives == 1) spawnSpeed = 1000;
    spawnPool[0].Spawn(boss._pos, spawnSpeed, 3, 0, 0);
    spawnPool[1].Spawn(boss._pos, spawnSpeed, 3, 1, 0);
}

/* ======================== spawn Functions =====================================

   The following spawn functions add items to pools by looking for an inactive
   item in the pool. You can only add as many as the ..._COUNT. Additional attempts 
   to add will be ignored.   
   
   ==============================================================================
*/
void spawnEnemy(int pos, int dir, int speed, int wobble){
    for(int e = 0; e<ENEMY_COUNT; e++){  // look for one that is not alive for a place to add one
        if(!enemyPool[e].Alive()){
            enemyPool[e].Spawn(pos, dir, speed, wobble);
            enemyPool[e].playerSide = pos > playerPosition?1:-1;
            return;
        }
    }
}

void spawnLava(int left, int right, int ontime, int offtime, int offset, int state){
    for(int i = 0; i<LAVA_COUNT; i++){
        if(!lavaPool[i].Alive()){
            lavaPool[i].Spawn(left, right, ontime, offtime, offset, state);
            return;
        }
    }
}

void spawnConveyor(int startPoint, int endPoint, int dir){
    for(int i = 0; i<CONVEYOR_COUNT; i++){
        if(!conveyorPool[i]._alive){
            conveyorPool[i].Spawn(startPoint, endPoint, dir);
            return;
        }
    }
}

void cleanupLevel(){
    for(int i = 0; i<ENEMY_COUNT; i++){
        enemyPool[i].Kill();
    }
    for(int i = 0; i<PARTICLE_COUNT; i++){
        particlePool[i].Kill();
    }
    for(int i = 0; i<SPAWN_COUNT; i++){
        spawnPool[i].Kill();
    }
    for(int i = 0; i<LAVA_COUNT; i++){
        lavaPool[i].Kill();
    }
    for(int i = 0; i<CONVEYOR_COUNT; i++){
        conveyorPool[i].Kill();
    }
    boss.Kill();
}

void levelComplete(){
    stageStartTime = millis();
    stage = WIN;
    //if(levelNumber == LEVEL_COUNT){
    if (lastLevel) {
        stage = BOSS_KILLED;
    }
    if (levelNumber != 0)  // no points for the first level
    {
        score = score + (lives * 10);  //
    }
}

void nextLevel(){
    levelNumber ++;
    //if(levelNumber > LEVEL_COUNT)
    if(lastLevel)
        levelNumber = 0;
    lives = LIVES_PER_LEVEL;
    loadLevel();
}

void die(){
    attackAvailable = false;
    if(levelNumber > 0)
        lives--;

    if(lives == 0){
        stage = GAMEOVER;
        stageStartTime = millis();
    }
    else
    {
        for(int p = 0; p < PARTICLE_COUNT; p++){
            particlePool[p].Spawn(playerPosition);
        }
        stageStartTime = millis();
        stage = DEAD;
    }
    killTime = millis();
}

// ----------------------------------
// -------- TICKS & RENDERS ---------
// ----------------------------------
void tickStartup(unsigned long mm)
{
    FastLED.clear();
    if(stageStartTime+STARTUP_WIPEUP_DUR > mm) // fill to the top with green
    {
        int n = min(map(((mm-stageStartTime)), 0, STARTUP_WIPEUP_DUR, 0, LED_COUNT), LED_COUNT);  // fill from top to bottom
        for(int i = 0; i<= n; i++){
            leds[i] = CRGB(0, 255, 0);
        }
    }
    else if(stageStartTime+STARTUP_SPARKLE_DUR > mm) // sparkle the full green bar
    {
        for(int i = 0; i< LED_COUNT; i++){
            if(random8(30) < 28)
                leds[i] = CRGB(0, 255, 0);  // most are green
            else {
                int flicker = random8(250);
                leds[i] = CRGB(flicker, 150, flicker); // some flicker brighter
            }
        }
    }
    else if (stageStartTime+STARTUP_FADE_DUR > mm) // fade it out to bottom
    {
        int n = max(map(((mm-stageStartTime)), STARTUP_SPARKLE_DUR, STARTUP_FADE_DUR, 0, LED_COUNT), 0);  // fill from top to bottom
        int brightness = max(map(((mm-stageStartTime)), STARTUP_SPARKLE_DUR, STARTUP_FADE_DUR, 255, 0), 0);

        for(int i = n; i< LED_COUNT; i++){
            leds[i] = CRGB(0, brightness, 0);
        }
    }
    SFXFreqSweepWarble(STARTUP_FADE_DUR, millis()-stageStartTime, 40, 400, 20);
}

void tickEnemies(){
    for(auto & i : enemyPool){
        if(i.Alive()){
            i.Tick();
            // Hit attack?
            if(attacking){
                if(i._pos > playerPosition-(attack_width/2) && i._pos < playerPosition+(attack_width/2)){
                    i.Kill();
                    SFXkill();
                }
            }
            if(inLava(i._pos)){
                i.Kill();
                SFXkill();
            }
            // Draw (if still alive)
            if(i.Alive()) {
                leds[getLED(i._pos)] = CRGB(255, 0, 0);
            }
            // Hit player?
            if(
                    (i.playerSide == 1 && i._pos <= playerPosition) ||
                    (i.playerSide == -1 && i._pos >= playerPosition)
                    ){
                die();
                return;
            }
        }
    }
}

void tickBoss(){
    // DRAW
    if(boss.Alive()){
        boss._ticks ++;
        for(int i = getLED(boss._pos-BOSS_WIDTH/2); i<=getLED(boss._pos+BOSS_WIDTH/2); i++){
            leds[i] = CRGB::DarkRed;
            leds[i] %= 100;
        }
        // CHECK COLLISION
        if(getLED(playerPosition) > getLED(boss._pos - BOSS_WIDTH/2) && getLED(playerPosition) < getLED(boss._pos + BOSS_WIDTH)){
            die();
            return;
        }
        // CHECK FOR ATTACK
        if(attacking){
            if(
                    (getLED(playerPosition+(attack_width/2)) >= getLED(boss._pos - BOSS_WIDTH/2) && getLED(playerPosition+(attack_width/2)) <= getLED(boss._pos + BOSS_WIDTH/2)) ||
                    (getLED(playerPosition-(attack_width/2)) <= getLED(boss._pos + BOSS_WIDTH/2) && getLED(playerPosition-(attack_width/2)) >= getLED(boss._pos - BOSS_WIDTH/2))
                    ){
                boss.Hit();
                if(boss.Alive()){
                    moveBoss();
                }else{
                    spawnPool[0].Kill();
                    spawnPool[1].Kill();
                }
            }
        }
    }
}

void drawPlayer(){
    uint8_t color1 = 255;
    uint8_t color2 = 0;
    if (tofOffset == TOF_NOTHING) {
        color1 = 100; // less green if "inactive"
    } else {
        color2 = (uint8_t) map(abs(tofOffset), 0, TOF_RANGE, 0, 30);
    }
    leds[getLED(playerPosition)] = CRGB(color2, color1, color2);
}

void drawExit(){
    if(!boss.Alive()){
        leds[LED_COUNT-1] = CRGB(0, 0, 255);
    }
}

void tickSpawners(unsigned long mm){
    for(auto & s : spawnPool){
        if(s.Alive() && s._activate < mm){
            if(s._lastSpawned + s._rate < mm || s._lastSpawned == 0){
                if (abs(playerPosition - s._pos) > 50) {
                    // be nice and don't surprise the player if she's very close
                    spawnEnemy(s._pos, s._dir, s._speed, 0);
                }
                s._lastSpawned = mm;
            }
        }
    }
}

void tickLava(unsigned long mm){
    uint8_t lava_off_brightness = APA102_LAVA_OFF_BRIGHTNESS;
    Lava LP;
    for(auto & i : lavaPool){
        LP = i;
        if(LP.Alive()){
            int A = getLED(LP._left);
            int B = getLED(LP._right);
            int p;
            if(LP._state == Lava::OFF){
                if(LP._lastOn + LP._offtime < mm){
                    LP._state = Lava::ON;
                    LP._lastOn = mm;
                }
                for(p = A; p<= B; p++){
                    auto flicker = random8(lava_off_brightness);
                    leds[p] = CRGB(lava_off_brightness+flicker, (lava_off_brightness+flicker)/1.5, 0);
                }
            }else if(LP._state == Lava::ON){
                if(LP._lastOn + LP._ontime < mm){
                    LP._state = Lava::OFF;
                    LP._lastOn = mm;
                }
                for(p = A; p<= B; p++){
                    if(random8(30) < 29)
                        leds[p] = CRGB(150, 0, 0);
                    else
                        leds[p] = CRGB(180, 100, 0);
                }
            }
        }
        i = LP;
    }
}

bool tickParticles(){
    bool stillActive = false;
    uint8_t brightness;
    for(auto & p : particlePool){
        if(p.Alive()){
            p.Tick(USE_GRAVITY);
            if (p._power < 5){
                brightness = (5 - p._power) * 10;
                leds[getLED(p._pos)] += CRGB(brightness, brightness/2, brightness/2);\
			} else {
                leds[getLED(p._pos)] += CRGB(p._power, 0, 0);\
            }
            stillActive = true;
        }
    }
    return stillActive;
}

void tickConveyors(unsigned long mm){
    int b, speed, n, ss, ee;
    unsigned long m = 10000 + mm;
    playerPositionModifier = 0;
    uint8_t conveyor_brightness;
    conveyor_brightness = APA102_CONVEYOR_BRIGHTNES;
    int levels = 5; // brightness levels in conveyor
    for(auto & i : conveyorPool){
        if(i._alive){
            speed = constrain(i._speed, -MAX_PLAYER_SPEED+1, MAX_PLAYER_SPEED-1);
            ss = getLED(i._startPoint);
            ee = getLED(i._endPoint);
            for(int led = ss; led<ee; led++){
                n = (-led + (m/100)) % levels;
                if(speed < 0) {
                    n = (led + (m/100)) % levels;
                }
                b = map(n, 5, 0, 0, conveyor_brightness);
                if(b > 0) {
                    leds[led] = CRGB(0, 0, b);
                }
            }
            if(playerPosition > i._startPoint && playerPosition < i._endPoint){
                playerPositionModifier = speed;
            }
        }
    }
}

void tickBossKilled(unsigned long mm) // boss funeral
{
    static uint8_t gHue = 0;

    FastLED.setBrightness(255); // super bright!

    int brightness = 0;
    FastLED.clear();

    if(stageStartTime+6500 > mm){
        gHue++;
        fill_rainbow( leds, LED_COUNT, gHue, 7); // FastLED's built in rainbow
        if( random8() < 200) {  // add glitter
            leds[ random16(LED_COUNT) ] += CRGB::White;
        }
        SFXbosskilled();
    }else if(stageStartTime+7000 > mm){
        int n = max(map(((mm-stageStartTime)), 5000, 5500, LED_COUNT, 0), 0);
        for(int i = 0; i< n; i++){
            brightness = (sin(((i*10)+mm)/500.0)+1)*255;
            leds[i].setHSV(brightness, 255, 50);
        }
        SFXcomplete();
    }else{
        nextLevel();
    }
}

void tickDie(unsigned long mm) { // a short bright explosion...particles persist after it.
    const int duration = 200; // milliseconds
    const int width = 10;     // half width of the explosion

    if(stageStartTime+duration > mm) {// Spread red from player position up and down the width

        int brightness = map((mm-stageStartTime), 0, duration, 255, 50); // this allows a fade from white to red

        // fill up
        int n = max(map(((mm-stageStartTime)), 0, duration, getLED(playerPosition), getLED(playerPosition)+width), 0);
        for(int i = getLED(playerPosition); i<= n; i++){
            leds[i] = CRGB(255, brightness, brightness);
        }

        // fill to down
        n = max(map(((mm-stageStartTime)), 0, duration, getLED(playerPosition), getLED(playerPosition)-width), 0);
        for(int i = getLED(playerPosition); i>= n; i--){
            leds[i] = CRGB(255, brightness, brightness);
        }
    }
}

void tickGameover(long mm) {
    int brightness = 0;
    FastLED.clear();
    if(stageStartTime+GAMEOVER_SPREAD_DURATION > mm) // Spread red from player position to top and bottom
    {
        // fill to top
        int n = max(map(((mm-stageStartTime)), 0, GAMEOVER_SPREAD_DURATION, getLED(playerPosition), LED_COUNT), 0);
        for(int i = getLED(playerPosition); i<= n; i++){
            leds[i] = CRGB(255, 0, 0);
        }
        // fill to bottom
        n = max(map(((mm-stageStartTime)), 0, GAMEOVER_SPREAD_DURATION, getLED(playerPosition), 0), 0);
        for(int i = getLED(playerPosition); i>= n; i--){
            leds[i] = CRGB(255, 0, 0);
        }
        SFXgameover();
    }
    else if(stageStartTime+GAMEOVER_FADE_DURATION > mm)  // fade down to bottom and fade brightness
    {
        int n = max(map(((mm-stageStartTime)), GAMEOVER_FADE_DURATION, GAMEOVER_SPREAD_DURATION, 0, LED_COUNT), 0);
        brightness =  map(((mm-stageStartTime)), GAMEOVER_SPREAD_DURATION, GAMEOVER_FADE_DURATION, 255, 0);

        for(int i = 0; i<= n; i++){
            leds[i] = CRGB(brightness, 0, 0);
        }
        SFXcomplete();
    }

}

void tickWin(long mm) {
    FastLED.clear();
    if(stageStartTime+WIN_FILL_DURATION > mm){
        int n = max(map(((mm-stageStartTime)), 0, WIN_FILL_DURATION, LED_COUNT, 0), 0);  // fill from top to bottom
        for(int i = LED_COUNT; i>= n; i--){
            leds[i] = CRGB(0, 255, 0);
        }
        SFXwin();
    }else if(stageStartTime+WIN_CLEAR_DURATION > mm){
        int n = max(map(((mm-stageStartTime)), WIN_FILL_DURATION, WIN_CLEAR_DURATION, LED_COUNT, 0), 0);  // clear from top to bottom
        for(int i = 0; i< n; i++){
            leds[i] = CRGB(0, 255, 0);
        }
        SFXwin();
    }else if(stageStartTime+WIN_OFF_DURATION > mm){   // wait a while with leds off
        leds[0] = CRGB(0, 255, 0);
    }else{
        nextLevel();
    }
}


void drawLives()
{
    // show how many lives are left by drawing a short line of green leds for each life
    SFXcomplete();  // stop any sounds
    FastLED.clear();

    int pos = 0;
    for (int i = 0; i < lives; i++)
    {
        for (int j=0; j<4; j++)
        {
            leds[pos++] = CRGB(0, 255, 0);
            FastLED.show();
        }
        leds[pos++] = CRGB(0, 0, 0);
    }
    FastLED.show();
    delay(1000);
    FastLED.clear();
}

void drawAttack(){
    if(!attacking) return;
    int n = map(millis() - attackMillis, 0, ATTACK_DURATION, 100, 5);
    for(int i = getLED(playerPosition-(attack_width/2))+1; i<=getLED(playerPosition+(attack_width/2))-1; i++){
        leds[i] = CRGB(0, 0, n);
    }
    if(n > 90) {
        n = 255;
        leds[getLED(playerPosition)] = CRGB(255, 255, 255);
    }else{
        n = 0;
        leds[getLED(playerPosition)] = CRGB(0, 255, 0);
    }
    leds[getLED(playerPosition-(attack_width/2))] = CRGB(n, n, 255);
    leds[getLED(playerPosition+(attack_width/2))] = CRGB(n, n, 255);
}

int getLED(int pos){
    // The world is 1000 pixels wide, this converts world units into an LED number
    return constrain((int)map(pos, 0, VIRTUAL_LED_COUNT, 0, LED_COUNT-1), 0, LED_COUNT-1);
}

bool inLava(int pos){
    // Returns if the player is in active lava
    int i;
    Lava LP;
    for(i = 0; i<LAVA_COUNT; i++){
        LP = lavaPool[i];
        if(LP.Alive() && LP._state == Lava::ON){
            if(LP._left < pos && LP._right > pos) return true;
        }
    }
    return false;
}

void updateLives(){
#ifdef USE_LIFELEDS
    // Updates the life LEDs to show how many lives the player has left
		for(int i = 0; i<LIFE_LEDS; i++){
		   digitalWrite(lifeLEDs[i], lives>i?HIGH:LOW);
		}
#endif

    drawLives();
}

// ---------------------------------
// --------- SCREENSAVER -----------
// ---------------------------------
void screenSaverTick(){
    int n, c, i;
    long mm = millis();
    int mode = (mm/30000)%5;

    SFXcomplete(); // make sure there is not sound...play testing showed this to be a problem

    for(i = 0; i<LED_COUNT; i++){
        leds[i].nscale8(250);
    }
    if(mode == 0){
        // Marching green <> orange
        n = (mm/250)%10;
        c = 20+((sin(mm/5000.00)+1)*33);
        for(i = 0; i<LED_COUNT; i++){
            if(i%10 == n){
                leds[i] = CHSV( c, 255, 150);
            }
        }
    }else if(mode >= 1){
        // Random flashes
        randomSeed(mm);
        for(i = 0; i<LED_COUNT; i++){
            if(random8(20) == 0){
                leds[i] = CHSV( 25, 255, 100);
            }
        }
    }
}

void getInput(){
    if (!tof_measurement_exists) {
        return; // nothing new
    }
    tof_measurement_exists = false; // reset
    uint8_t NewDataReady = 0;
    uint8_t status = sensor_vl53l4cd_sat.VL53L4CD_CheckForDataReady(&NewDataReady);

    char report[64];
    // TODO: consider the case where there is no measurement because no hand
    if ((!status) && (NewDataReady != 0)) {
        sensor_vl53l4cd_sat.VL53L4CD_ClearInterrupt(); // (Mandatory) Clear HW interrupt to restart measurements

        // Read measured distance. RangeStatus = 0 means valid data
        VL53L4CD_Result_t results;
        sensor_vl53l4cd_sat.VL53L4CD_GetResult(&results);

        auto dist_mm = (int) results.distance_mm;
        MPUDistanceSamples.add(dist_mm);
        dist_mm = (int) MPUDistanceSamples.getMedian();

        snprintf(report, sizeof(report), "dis %4i sig %3i %5i ",
                 results.distance_mm, results.sigma_mm, dist_mm);
        SerialPort.print(report);

        if (dist_mm > TOF_MAX) { // "nothing" detected
            tofOffset = TOF_NOTHING;
            SerialPort.println("OUT");
        } else {
            bool updateOffset = true;
            if (tofOffset == TOF_NOTHING) { // hand entered the sensor
                // wait until there is less noise
                updateOffset = MPUDistanceSamples.getHighest() - MPUDistanceSamples.getLowest() < 100;
            }
            if (updateOffset) {
                tofOffset = dist_mm - TOF_ZERO;
                tofOffset = constrain(tofOffset, -TOF_RANGE, TOF_RANGE);
                SerialPort.print("USE ");
                SerialPort.println(tofOffset);
            } else {
                SerialPort.println("NOK");
            }
        }
    }
}

// ---------------------------------
// -------------- SFX --------------
// ---------------------------------

/*
   This is used sweep across (up or down) a frequency range for a specified duration.
   A sin based warble is added to the frequency. This function is meant to be called
   on each frame to adjust the frequency in sync with an animation
   
   duration 	= over what time period is this mapped
   elapsedTime 	= how far into the duration are we in
   freqStart 	= the beginning frequency
   freqEnd 		= the ending frequency
   warble 		= the amount of warble added (0 disables)   
   

*/
void SFXFreqSweepWarble(int duration, int elapsedTime, int freqStart, int freqEnd, int warble)
{
    int freq = map_constrain(elapsedTime, 0, duration, freqStart, freqEnd);
    if (warble)
        warble = map(sin(millis()/20.0)*1000.0, -1000, 1000, 0, warble);

    toneAC(freq + warble, AUDIO_VOLUME);
}

/*
   
   This is used sweep across (up or down) a frequency range for a specified duration.
   Random noise is optionally added to the frequency. This function is meant to be called
   on each frame to adjust the frequency in sync with an animation
   
   duration 	= over what time period is this mapped
   elapsedTime 	= how far into the duration are we in
   freqStart 	= the beginning frequency
   freqEnd 		= the ending frequency
   noiseFactor 	= the amount of noise to added/subtracted (0 disables)   
   

*/
void SFXFreqSweepNoise(int duration, int elapsedTime, int freqStart, int freqEnd, uint8_t noiseFactor){
    int freq;
    if (elapsedTime > duration)
        freq = freqEnd;
    else
        freq = map(elapsedTime, 0, duration, freqStart, freqEnd);

    if (noiseFactor)
        noiseFactor = noiseFactor - random8(noiseFactor / 2);

    toneAC(freq + noiseFactor, AUDIO_VOLUME);
}

void SFXtilt(int amount){
    auto f = map(abs(amount), 0, TOF_RANGE, 300, 900) + random8(80);
    if(playerPositionModifier < 0) f -= 500;
    if(playerPositionModifier > 0) f += 200;
    toneAC(f, min(min(abs(amount)/9, 5), AUDIO_VOLUME));
}

void SFXattacking(){
    int freq = map(sin(millis()/2.0)*1000.0, -1000, 1000, 500, 600);
    if(random8(5)== 0){
        freq *= 3;
    }
    toneAC(freq, AUDIO_VOLUME);
}
void SFXdead(){
    SFXFreqSweepNoise(1000, millis()-killTime, 1000, 10, 200);
}

void SFXgameover(){
    SFXFreqSweepWarble(GAMEOVER_SPREAD_DURATION, millis()-killTime, 440, 20, 60);
}

void SFXkill(){
    toneAC(2000, AUDIO_VOLUME, 1000, true);
}
void SFXwin(){
    SFXFreqSweepWarble(WIN_OFF_DURATION, millis()-stageStartTime, 40, 400, 20);
}

void SFXbosskilled(){
    SFXFreqSweepWarble(7000, millis()-stageStartTime, 75, 1100, 60);
}

void SFXcomplete(){
    noToneAC();
}

/*
	This works just like the map function except x is constrained to the range of in_min and in_max
*/
long map_constrain(long x, long in_min, long in_max, long out_min, long out_max)
{
    // constrain the x value to be between in_min and in_max
    if (in_max > in_min){   // map allows min to be larger than max, but constrain does not
        x = constrain(x, in_min, in_max);
    }
    else {
        x = constrain(x, in_max, in_min);
    }

    return map(x, in_min, in_max, out_min, out_max);


}
