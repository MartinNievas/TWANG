/* 
	TWANG 
	
	Code at ..
	
	https://github.com/bdring/TWANG
	
	Based on original code by Critters/TWANG	
	
	https://github.com/Critters/TWANG

*/
// Required libs
#include <FastLED.h>
#include <Wire.h>
#include "Adafruit_MPU6050.h"
#include <toneAC.h>
#include <RunningMedian.h>

// Included libs
#include "Enemy.h"
#include "Particle.h"
#include "Spawner.h"
#include "Lava.h"
#include "Boss.h"
#include "Conveyor.h"
#include "Constants.h"

// LED Strip Setup
#define LED_DATA_PIN             3
#define LED_CLOCK_PIN            13
#define VIRTUAL_LED_COUNT 1000
#define LED_COUNT 144 * 2 - 5
#define LED_BRIGHTNESS 120 // was 60

#define WS2812_CONVEYOR_BRIGHTNES 40
#define WS2812_LAVA_OFF_BRIGHTNESS 15


#define DIRECTION            1     // 0 = right to left, 1 = left to right

#define START_LEVEL 0 // 15 for boss
#define MAX_PLAYER_SPEED     10     // Max move speed of the player per frame
#define LIVES_PER_LEVEL		 10
#define DEFAULT_ATTACK_WIDTH 150    // Width of the wobble attack, world is 1000 wide
#define ATTACK_DURATION      300    // Duration of a wobble attack (ms)
#define BOSS_WIDTH           40

#define SCREENSAVER_TIMEOUT  30000  // time until screen saver

#define MIN_REDRAW_INTERVAL  16    // Min redraw interval (ms) 33 = 30fps / 16 = 63fps

unsigned long previousMillis = 0;           // Time of the last redraw
int levelNumber = START_LEVEL;

bool demoMode = false;

int attackWidth = 0;
unsigned long attackMillis = 0;             // Time the attack started
bool attacking = 0;                // Is the attack in progress?
bool attackAvailable = false;
int attackCenter;

const uint8_t AUDIO_VOLUME = 10; // 0-10

CRGB leds[VIRTUAL_LED_COUNT]; // this is set to the max, but the actual number used is set in FastLED.addLeds below

// MPU
Adafruit_MPU6050 accelgyro;
RunningMedian MPUAngleSamples = RunningMedian(5);
RunningMedian MPUWobbleSamples = RunningMedian(5);

int joystickTilt = 0;              // Stores the angle of the joystick
int joystickWobble = 0;            // Stores the max amount of wobble

#define DEV_I2C Wire
#define SerialPort Serial

enum stages {
    STARTUP,
    PLAY,
    WIN,
    DEAD,
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

int exitPosition;

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

#define SPAWN_COUNT 3
Spawner spawnPool[SPAWN_COUNT] = {
        Spawner(), Spawner(), Spawner()
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
void drawExit(unsigned long mm);
void tickSpawners(unsigned long mm);
void tickLava(unsigned long mm);
bool tickParticles();
void tickConveyors(unsigned long mm);
void tickBossKilled(unsigned long mm);
void tickDie(unsigned long mm);
void tickGameover(unsigned long mm);
void tickWin(long mm);
void drawAttack(unsigned long mm);
int getLED(int pos);
bool inLava(int pos);
void getInput();
void SFXFreqSweepWarble(int duration, int elapsedTime, int freqStart, int freqEnd, int warble);
void SFXFreqSweepNoise(int duration, int elapsedTime, int freqStart, int freqEnd, uint8_t noiseFactor);
void SFXtilt(int amount);
void SFXattacking(unsigned long mm);
void SFXdead();
void SFXgameover();
void SFXkill();
void SFXwin();
void SFXbosskilled();
void SFXcomplete();
long map_constrain(long x, long in_min, long in_max, long out_min, long out_max);

void setup() {
    Serial.begin(115200);

    // MPU
    Wire.begin();

    // Try to initialize!
    // TODO: Initialize
    //if (!accelgyro.begin()) {
    //  Serial.println("Failed to find MPU6050 chip");
    //  while (1) {
    //    delay(10);
    //  }
    //}
    Serial.println("MPU6050 Found!");

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

int lifeParticlePos[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
double lifeParticleSpeed[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void spawnLifeParticles() {
    //auto spacing = VIRTUAL_LED_COUNT / (lives + 1);
    auto spacing = 20;
    for (auto i = 0; i < lives; i++) {
        lifeParticlePos[i] = spacing + i * spacing;
        lifeParticleSpeed[i] = 0;
    }
}

void drawLifeParticles(unsigned long mm) {
    uint8_t color = constrain(map(mm - stageStartTime, 0, 500, 50, 255), 0, 255);
    for (auto i = 0; i < lives; i++) {
        if (lifeParticlePos[i] == 0) {
            continue;
        }
        auto particleIsRight = playerPosition < lifeParticlePos[i];
        auto gravity = 0.5;
        if (particleIsRight) {
            gravity *= -1;
        }
        if (mm - stageStartTime < 500) {
            gravity = 0;
        }
        lifeParticlePos[i] += lifeParticleSpeed[i];
        lifeParticleSpeed[i] += gravity;
        auto particleIsStillRight = playerPosition < lifeParticlePos[i];
        if (particleIsRight != particleIsStillRight) {
            // we are home
            lifeParticlePos[i] = 0;
            SFXkill();
        } else { // draw it
            leds[getLED(lifeParticlePos[i])] = CRGB(color / 2, 0, color * 2 / 3);
        }
    }
}

int getPlayerSpeed() {
    // TODO: Update max player speed
    return MAX_PLAYER_SPEED / 10;
}

void drawBackground() {
    //for (auto i = 0; i < LED_COUNT; i++) {
    //    leds[i] = CRGB(leds[i].DarkMagenta).fadeToBlackBy(242);
    //}
}

void loopPlay(unsigned long mm) {
    if (attacking) {
        if (attackMillis + ATTACK_DURATION < mm) { // end attack
            attacking = false;
        } else { // while attack in progress
            SFXattacking(mm);
        }
    } else { // not attacking currently
        if (attackAvailable && joystickWobble > user_settings.attack_threshold) { // start attack
            attackMillis = mm;
            attackCenter = playerPosition;
            attacking = true;
            attackAvailable = false;
        }
    }
    playerPosition += playerPositionModifier; // forced move by elevators

    if(!attacking){
		SFXtilt(joystickTilt);
        int moveAmount = (joystickTilt/6.0);
        if(DIRECTION) moveAmount = -moveAmount;
        moveAmount = constrain(moveAmount, -MAX_PLAYER_SPEED, MAX_PLAYER_SPEED);
        playerPosition -= moveAmount;
        if(playerPosition < 0) playerPosition = 0;
		// stop player from leaving if boss is alive
		if (boss.Alive() && playerPosition >= VIRTUAL_LED_COUNT) // move player back
			playerPosition = VIRTUAL_LED_COUNT - 1;
					
        if(playerPosition >= VIRTUAL_LED_COUNT && !boss.Alive()) {
            // Reached exit!
            levelComplete();
            return;
        }
    }

    if(inLava(playerPosition)){
        die();
    }

    FastLED.clear();
    drawBackground();
    tickConveyors(mm);
    drawAttack(mm);
    tickBoss();
    drawLifeParticles(mm);
    tickSpawners(mm);
    tickLava(mm);
    tickEnemies();
    drawPlayer();
    drawExit(mm);
}

void loop() {
    auto mm = millis();

    if (mm - previousMillis >= MIN_REDRAW_INTERVAL) {
        // TODO: Get input
        // getInput();
        previousMillis = mm;
        if(stage == STARTUP){
            Serial.println("Startup");
            if (stageStartTime + STARTUP_FADE_DUR > mm) {
                tickStartup(mm);
            } else {
                SFXcomplete();
                levelNumber = START_LEVEL;
                loadLevel();
            }
        }else if(stage == PLAY){
            Serial.println("Play");
            Serial.println(playerPosition);
            playerPosition++;
            loopPlay(mm);
        }else if(stage == DEAD){
            Serial.println("Dead");
            SFXdead();
            FastLED.clear();
            tickDie(mm);
            if(!tickParticles()){
                loadLevel();
            }
        } else if(stage == WIN){// LEVEL COMPLETE
            Serial.println("Win");
            tickWin(mm);
        } else if(stage == BOSS_KILLED){
            tickBossKilled(mm);
        } else if (stage == GAMEOVER) {
            if (stageStartTime+GAMEOVER_FADE_DURATION > mm) {
                tickGameover(mm);
            } else {
                FastLED.clear();
                levelNumber = START_LEVEL;
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
    cleanupLevel();
    spawnLifeParticles();
    lastLevel = false; // this gets changed on the boss level

    /// Defaults...OK to change the following items in the levels below
    attackWidth = 0;
    playerPosition = 0;
    exitPosition = VIRTUAL_LED_COUNT;

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
            speed: How fast does it move. Typically, 1 to 4.
            wobble: 0=regular movement, 1=bouncing back and forth use the speed value
                to set the length of the wobble.

    Spawn Pools: This generates and endless source of new enemies. 2 pools max
        spawnPool[index].Spawn(position, rate, speed, direction, activate);
            index: You can have up to 2 pools, use an index of 0 for the first and 1 for the second.
            position: The location the enemies with be generated from.
            rate: The time in milliseconds between each new enemy
            speed: How fast they move. Typically, 1 to 4.
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
            spawnEnemy(950, 0, 1, 0);
            break;
        case 2:
            // Spawning enemies at exit every X seconds
            spawnPool[0].Spawn(900, 3000, 2, 0, 0);
            break;
        case 3:
            // Lava intro
            spawnLava(400, 490, 2000, 2000, 0, Lava::OFF);
            spawnEnemy(350, 0, 1, 0);
            spawnPool[0].Spawn(900, 5500, 3, 0, 0);

            break;
        case 4:
            // Sin enemy
            spawnEnemy(700, 1, 7, 275);
            spawnEnemy(500, 1, 5, 250);
            break;
        case 5:
            // Sin enemy swarm
            spawnEnemy(700 + random8(10), 1, 7, 225 + random8(20));
            spawnEnemy(500 + random8(10), 1, 5, 210 + random8(20));
            spawnEnemy(600 + random8(10), 1, 7, 190 + random8(20));
            spawnEnemy(800 + random8(10), 1, 5, 300 + random8(30));
            spawnEnemy(400 + random8(10), 1, 7, 140 + random8(20));
            spawnEnemy(450 + random8(10), 1, 5, 330 + random8(30));
            break;
        case 6:
            // Conveyor
            spawnConveyor(200, 700, -6);
            spawnEnemy(850, 0, 0, 0);
            spawnPool[0].Spawn(0, 1000, 2, 1, 5000);
            break;
        case 7:
            // Conveyor of enemies
            spawnConveyor(50, 950, 4);
            spawnEnemy(300, 0, 0, 0);
            spawnEnemy(400, 0, 0, 0);
            spawnEnemy(500, 0, 0, 0);
            spawnEnemy(600, 0, 0, 0);
            spawnEnemy(700, 0, 0, 0);
            spawnEnemy(800, 0, 0, 0);
            spawnEnemy(900, 0, 0, 0);
            break;
        case 8:   // spawn train
            spawnPool[0].Spawn(950, 1300, 2, 0, 0);
            spawnPool[1].Spawn(0, 10000, 4, 1, 5000 + random(10000));
            spawnEnemy(700, 0, 2, 0);
            break;
        case 9:   // spawn train upside down
            playerPosition = VIRTUAL_LED_COUNT / 10 * 9;
            exitPosition = 0;
            spawnPool[0].Spawn(50, 2100, 2, 1, 0);
            spawnPool[1].Spawn(1000, 10000, 4, 0, 10000 + random(10000));
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
            spawnLava(195, 280, 2000, 2000, 0, Lava::OFF);
            spawnLava(400, 480, 2000, 2000, 0, Lava::OFF);
            spawnLava(600, 680, 2000, 2000, 0, Lava::OFF);
            spawnPool[0].Spawn(990, 3800, 4, 0, 0);
            break;
        case 13:
            // Sin enemy #2 practice (slow conveyor)
            spawnEnemy(700, 1, 7, 275);
            spawnEnemy(500, 1, 5, 250);
            spawnPool[0].Spawn(990, 5500, 4, 0, 3000);
            spawnPool[1].Spawn(10, 5500, 5, 1, 10000);
            spawnConveyor(100, 900, -2);
            break;
        case 14:
            // Sin enemy #2 (fast conveyor)
            spawnEnemy(800, 1, 7, 275);
            spawnEnemy(700, 1, 7, 275);
            spawnEnemy(500, 1, 5, 250);
            spawnPool[0].Spawn(990, 3000, 4, 0, 3000);
            spawnPool[1].Spawn(0, 5500, 5, 1, 10000);
            spawnConveyor(100, 900, -5);
            spawnLava(600, 650, 2000, 2000, 0, Lava::OFF);
            spawnLava(800, 850, 2000, 2000, 0, Lava::OFF);
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
    exitPosition = VIRTUAL_LED_COUNT;
    if(boss._lives == 2) {
        spawnSpeed = 1600;
        exitPosition = 0;
    }
    if(boss._lives == 1) {
        spawnSpeed = 1000;
        exitPosition = VIRTUAL_LED_COUNT;
    }
    spawnPool[0].Spawn(boss._pos - BOSS_WIDTH / 2, spawnSpeed, 3, 0, 0);
    spawnPool[1].Spawn(boss._pos + BOSS_WIDTH / 2, spawnSpeed, 3, 1, 0);
    //enemyPool[0].Spawn(boss._pos, 0, 0, 0);
}

/* ======================== spawn Functions =====================================

   The following spawn functions add items to pools by looking for an inactive
   item in the pool. You can only add as many as the ..._COUNT. Additional attempts 
   to add will be ignored.   
   
   ==============================================================================
*/
void spawnEnemy(int pos, int dir, int speed, int wobble){
    for(auto & e : enemyPool){  // look for one that is not alive for a place to add one
        if(!e.Alive()){
            e.Spawn(pos, dir, speed, wobble);
            e.playerSide = pos > playerPosition?1:-1;
            return;
        }
    }
}

void spawnLava(int left, int right, int ontime, int offtime, int offset, int state){
    for(auto & i : lavaPool){
        if(!i.Alive()){
            i.Spawn(left, right, ontime, offtime, offset, state);
            return;
        }
    }
}

void spawnConveyor(int startPoint, int endPoint, int dir){
    for(auto & i : conveyorPool){
        if(!i._alive){
            i.Spawn(startPoint, endPoint, dir);
            return;
        }
    }
}

void cleanupLevel(){
    for(auto & i : enemyPool){
        i.Kill();
    }
    for(auto & i : particlePool){
        i.Kill();
    }
    for(auto & i : spawnPool){
        i.Kill();
    }
    for(auto & i : lavaPool){
        i.Kill();
    }
    for(auto & i : conveyorPool){
        i.Kill();
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
        levelNumber = START_LEVEL;
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
    } else {
        for(auto & p : particlePool){
            p.Spawn(playerPosition, getPlayerSpeed());
        }
        stageStartTime = millis();
        stage = DEAD;
    }
    killTime = millis();
}

// ----------------------------------
// -------- TICKS & RENDERS ---------
// ----------------------------------
void tickStartup(unsigned long mm) {
    FastLED.clear();
    if(stageStartTime+STARTUP_WIPEUP_DUR > mm) // fill to the top with green
    {
        int n = min(map(((mm-stageStartTime)), 0, STARTUP_WIPEUP_DUR, 0, LED_COUNT), LED_COUNT);  // fill from top to bottom
        for(int i = 0; i<= n; i++){
            leds[i] = CRGB(0, 100, 0);
        }
    }
    else if(stageStartTime+STARTUP_SPARKLE_DUR > mm) // sparkle the full green bar
    {
        for(int i = 0; i< LED_COUNT; i++){
            if(random8(50) == 0) {
                int flicker = random8(250);
                leds[i] = CRGB(flicker, 150, flicker); // some flicker brighter
            } else {
                leds[i] = CRGB(0, 100, 0);  // most are green
            }
        }
    }
    else if (stageStartTime+STARTUP_FADE_DUR > mm) // fade it out to bottom
    {
        int n = max(map(((mm-stageStartTime)), STARTUP_SPARKLE_DUR, STARTUP_FADE_DUR, 0, LED_COUNT), 0);  // fill from top to bottom
        int brightness = max(map(((mm-stageStartTime)), STARTUP_SPARKLE_DUR, STARTUP_FADE_DUR, 100, 0), 0);

        for(int i = n; i< LED_COUNT; i++){
            leds[i] = CRGB(0, brightness, 0);
        }
    }
    SFXFreqSweepWarble(STARTUP_FADE_DUR, mm-stageStartTime, 40, 400, 20);
}

void tickEnemies(){
    for(auto & i : enemyPool){
        if(i.Alive()){
            i.Tick();
            // Hit attack?
            if(attacking){
                if(i._pos > attackCenter-(attackWidth / 2) && i._pos < attackCenter + (attackWidth / 2)){
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
        for(int i = getLED(boss._pos-BOSS_WIDTH/2); i<=getLED(boss._pos+BOSS_WIDTH/2); i++){
            if (i % 2 == 0) {
                leds[i] = CRGB::DarkRed;
                leds[i] %= 100;
            } else {
                leds[i] = CRGB::DarkViolet;
            }
        }
        // CHECK COLLISION
        if(getLED(playerPosition) > getLED(boss._pos - BOSS_WIDTH/2)
                && getLED(playerPosition) < getLED(boss._pos + BOSS_WIDTH)){
            die();
            return;
        }
        // CHECK FOR ATTACK
        if(attacking){
            if(
                    (getLED(attackCenter+(attackWidth / 2)) >= getLED(boss._pos - BOSS_WIDTH / 2) && getLED(attackCenter + (attackWidth / 2)) <= getLED(boss._pos + BOSS_WIDTH / 2)) ||
                    (getLED(attackCenter-(attackWidth / 2)) <= getLED(boss._pos + BOSS_WIDTH / 2) && getLED(attackCenter - (attackWidth / 2)) >= getLED(boss._pos - BOSS_WIDTH / 2))
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
    leds[getLED(playerPosition)] = CRGB(0, 255, 0);
}

void drawExit(unsigned long mm){
    auto alt = (255 - LED_BRIGHTNESS) / 2;
    auto middle = LED_BRIGHTNESS + alt;
    auto color = (uint8_t) (middle + sin((double) (mm - stageStartTime) / 500.0) * alt);
    leds[getLED(exitPosition)] = CRGB(0, 0, color);
}

void tickSpawners(unsigned long mm){
    unsigned long fadeInMs = 500;
    for(auto & s : spawnPool){
        if(s.Alive() && s._activate < mm){
            auto nextSpawnAt = s._lastSpawned + s._rate;
            if(nextSpawnAt - fadeInMs < mm) { // spawn soon!
                uint8_t x = 100 - (nextSpawnAt - mm) / 5;
                leds[getLED(s._pos)] = CRGB(2 * x, x, 0);
            }
            if(nextSpawnAt < mm || s._lastSpawned == 0){
                if (abs(playerPosition - s._pos) > 80) {
                    // be nice and don't surprise the player if she's very close
                    spawnEnemy(s._pos, s._dir, s._speed, 0);
                }
                s._lastSpawned = mm;
            }
        }
    }
}

void tickLava(unsigned long mm){
    uint8_t lava_off_brightness = WS2812_LAVA_OFF_BRIGHTNESS;
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
                    leds[p] = CRGB(lava_off_brightness+flicker, (lava_off_brightness+flicker)/1.2, 0);
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
        if (p.Alive()) {
            p.Tick();
            if (p._life <= 5) {
                brightness = (5 - p._life) * 10;
                leds[getLED((int) p._pos)] += CRGB(brightness, brightness/2, brightness/2);
            } else {
                brightness = map(p._life, 0, p._maxLife, 50, 255);
                uint8_t orange = random8(25);
                leds[getLED((int) p._pos)] = CRGB(brightness, orange, 0);
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
    conveyor_brightness = WS2812_CONVEYOR_BRIGHTNES;
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
    if (demoMode) { // DONT SHOW in demo mode
        nextLevel();
        return;
    }

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

void tickGameover(unsigned long mm) {
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
        for(int i = LED_COUNT; i>= n; i-=2){
            leds[i] = CRGB(0, 255, 0);
        }
        SFXwin();
    }else if(stageStartTime+WIN_CLEAR_DURATION > mm){
        int n = max(map(((mm-stageStartTime)), WIN_FILL_DURATION, WIN_CLEAR_DURATION, LED_COUNT, 0), 0);  // clear from top to bottom
        for(int i = 1; i< n; i+=2){
            leds[i] = CRGB(0, 255, 0);
        }
        SFXwin();
    }else if(stageStartTime+WIN_OFF_DURATION > mm){   // wait a while with leds off
        leds[0] = CRGB(0, 255, 0);
    }else{
        nextLevel();
        return;
    }
    int showRand = map(mm - stageStartTime, 0, WIN_OFF_DURATION, 50, 255);
    for(int i = 0; i<LED_COUNT; i++){
        if(random8(showRand) == 0) {
            int flicker = random8(map(mm - stageStartTime, 0, WIN_OFF_DURATION, 200, 0));
            leds[i] = CRGB(flicker/2, flicker, flicker/2); // some flicker
        }
    }
}

void drawAttack(unsigned long mm){
    if(!attacking) {
        return;
    }
    attackWidth = map(mm - attackMillis, 0, ATTACK_DURATION, 5, DEFAULT_ATTACK_WIDTH);
    uint8_t color = 255;
    int centerLed = getLED(attackCenter);
    // aka getLED without constraint such that the attack can reach over borders
    auto leftLed = (int) map(attackCenter - (attackWidth / 2), 0, VIRTUAL_LED_COUNT, 0, LED_COUNT - 1);
    bool edge = true;
    for (int i = leftLed; i <= centerLed; i++) {
        int ii = centerLed + (centerLed - i);
        auto pxl = CRGB(0, 0, color);
        if (edge) {
            pxl = CRGB(100, 100, 255);
            edge = false;
        }
        if (i >= 0) {
            leds[i] = pxl;
        }
        if (ii < LED_COUNT) {
            leds[ii] = pxl;
        }
        if (color <= 25) {
            break;
        }
        color -= 25;
    }
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

int lastDemoSpeed = 0;

void getInput(){
    // This is responsible for the player movement speed and attacking.
    // You can replace it with anything you want that passes a -90>+90 value to joystickTilt
    // and any value to joystickWobble that is greater than ATTACK_THRESHOLD (defined at start)
    // For example you could use 3 momentary buttons:
        // if(digitalRead(leftButtonPinNumber) == HIGH) joystickTilt = -90;
        // if(digitalRead(rightButtonPinNumber) == HIGH) joystickTilt = 90;
        // if(digitalRead(attackButtonPinNumber) == HIGH) joystickWobble = ATTACK_THRESHOLD;
	int16_t ax, ay, az;
	int16_t gx, gy, gz;

    // accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    sensors_event_t mpu_a, mpu_g, mpu_temp;
    accelgyro.getEvent(&mpu_a, &mpu_g, &mpu_temp);
    ax = mpu_a.acceleration.x;
    ay = mpu_a.acceleration.y;
    az = mpu_a.acceleration.z;
    gx = mpu_g.gyro.x;
    gy = mpu_g.gyro.y;
    gz = mpu_g.gyro.z;

    int a = (JOYSTICK_ORIENTATION == 0?ax:(JOYSTICK_ORIENTATION == 1?ay:az))/166;
    int g = (JOYSTICK_ORIENTATION == 0?gx:(JOYSTICK_ORIENTATION == 1?gy:gz));
	
    if(abs(a) < user_settings.joystick_deadzone) a = 0;
    if(a > 0) a -= user_settings.joystick_deadzone;
    if(a < 0) a += user_settings.joystick_deadzone;
    MPUAngleSamples.add(a);
    MPUWobbleSamples.add(g);

    joystickTilt = MPUAngleSamples.getMedian();
    if(JOYSTICK_DIRECTION == 1) {
        joystickTilt = 0-joystickTilt;
    }
    joystickWobble = abs(MPUWobbleSamples.getHighest());

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
    // TODO: Update TOF
    auto f = map(abs(amount), 0, 0, 300, 900) + random8(80);
    if(playerPositionModifier < 0) f -= 500;
    if(playerPositionModifier > 0) f += 200;
    toneAC(f, min(min(abs(amount)/9, 5), AUDIO_VOLUME));
}

void SFXattacking(unsigned long mm){
    int freq = map(sin(mm / 2.0) * 1000.0, -1000, 1000, 500, 600);
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
