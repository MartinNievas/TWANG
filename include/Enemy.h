#include "Arduino.h"
#include "Constants.h"
#include "Audio.h"

#define ENEMY_COUNT 20

class Enemy {
private:
    int _speed;
    int _origin;
    int _pos;
    int _wobble;
    bool _alive = false;

public:
    void Spawn(int pos, int speed, int wobble) {
        _pos = pos;
        _wobble = wobble;    // 0 = no, >0 = yes, value is width of wobble
        _origin = pos;
        _speed = speed;
        _alive = true;
    }

    void Tick(unsigned long mm) {
        if (!Alive()) {
            return;
        }
        if (_wobble > 0) {
            _pos = _origin + (sin((mm / 3000.0) * _speed) * _wobble);
        } else {
            _pos += _speed;
            if (_pos >= VIRTUAL_LED_COUNT || _pos < 0) {
                Kill(); // silent out of range
            }
        }
    }

    void Draw() {
        leds[getLED(_pos)] = CRGB(255, 0, 0);
    }

    bool HitPlayer(int playerPosition) const {
        return (_pos < playerPosition && playerPosition < _origin) ||
               (_origin < playerPosition && playerPosition < _pos);
    }

    int Pos() const {
        return _pos;
    }

    void Kill() {
        _alive = false;
    }

    bool Alive() const {
        return _alive;
    }
};

class EnemyPool {
private:
    Enemy pool[ENEMY_COUNT] = {};
public:
    void Spawn(int pos, int speed, int wobble) {
        for (auto &enemy: pool) {
            if (!enemy.Alive()) {
                enemy.Spawn(pos, speed, wobble);
            }
        }
    }

    template <typename F>

    void KillIf(F func) {
        for (auto &enemy: pool) {
            if (func(enemy.Pos())) {
                enemy.Kill();
                SFXkill();
            }
        }
    }

    void Tick(unsigned long mm) {
        for (auto &enemy: pool) {
            enemy.Tick(mm);
        }
    }

    void Kill() {
        for (auto &enemy: pool) {
            enemy.Kill();
        }
    }
};
