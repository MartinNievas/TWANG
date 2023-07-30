#include "Arduino.h"

#define LAVA_COUNT 4
#define APA102_LAVA_OFF_BRIGHTNESS 5

/*
startPoint: 0 to 1000
endPoint: 0 to 1000, combined with startPoint this sets the location and size of the lava
ontime: How long (ms) the lava is ON for
offtime: How long (ms) the lava is OFF for
offset: How long (ms) after the level starts before the lava turns on, use this to create patterns with multiple lavas
*/
class Lava {
private:
    int _left;
    int _right;
    int _ontime;
    int _offtime;
    int _offset;
    unsigned long _lastOn;
    bool _isActive;
    int _alive = false;
public:
    void Spawn(int left, int right, int ontime, int offtime, int offset, bool isActive) {
        _left = left;
        _right = right;
        _ontime = ontime;
        _offtime = offtime;
        _offset = offset;
        _alive = 1;
        _lastOn = millis() - offset;
        _isActive = isActive;
    }

    void Tick(unsigned long mm) {
        if (!Alive()) {
            return;
        }
        uint8_t lava_off_brightness = APA102_LAVA_OFF_BRIGHTNESS;
        int A = getLED(_left);
        int B = getLED(_right);
        int p;
        if (!_isActive) {
            if (_lastOn + _offtime < mm) {
                _isActive = true;
                _lastOn = mm;
            }
            for (p = A; p <= B; p++) {
                auto flicker = random8(lava_off_brightness);
                leds[p] = CRGB(lava_off_brightness + flicker, (lava_off_brightness + flicker) / 1.5, 0);
            }
        } else {
            if (_lastOn + _ontime < mm) {
                _isActive = false;
                _lastOn = mm;
            }
            for (p = A; p <= B; p++) {
                if (random8(30) < 29)
                    leds[p] = CRGB(150, 0, 0);
                else
                    leds[p] = CRGB(180, 100, 0);
            }
        }
    }

    bool IsBurningAt(int pos) const {
        return Alive() && _isActive && _left <= pos && _right <= pos;
    }

    void Kill() {
        _alive = 0;
    }

    int Alive() const {
        return _alive;
    }
};

class LavaPool {
private:
    EnemyPool &_enemyPool;
    Lava pool[LAVA_COUNT] = {};
public:
    explicit LavaPool(EnemyPool &enemyPool) : _enemyPool(enemyPool) {}

    void Spawn(int left, int right, int ontime, int offtime, int offset, bool isActive) {
        for (auto &lava: pool) {
            if (!lava.Alive()) {
                lava.Spawn(left, right, ontime, offtime, offset, isActive);
            }
        }
    }

    void Tick(unsigned long mm) {
        for (auto &lava: pool) {
            if (lava.Alive()) {
                lava.Tick(mm);
                auto lambda = [&lava](int pos) { return lava.IsBurningAt(pos); };
                _enemyPool.KillIf(lambda);
            }
        }
    }

    void Kill() {
        for (auto &lava: pool) {
            lava.Kill();
        }
    }
};
