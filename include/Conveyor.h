#include "Arduino.h"

#define CONVEYOR_COUNT 2
#define APA102_CONVEYOR_BRIGHTNESS 10

class Conveyor {
    int _startPoint;
    int _endPoint;
    int _speed;
    bool _alive;

public:
    void Spawn(int startPoint, int endPoint, int speed) {
        _startPoint = startPoint;
        _endPoint = endPoint;
        _speed = constrain(speed, -MAX_PLAYER_SPEED + 1, MAX_PLAYER_SPEED - 1);
        _alive = true;
    }

    void Kill() {
        _alive = false;
    }

    bool Alive() const {
        return _alive;
    }

    void Tick(unsigned long mm) const {
        if (!_alive) {
            return;
        }
        unsigned long m = 10000 + mm;
        int levels = 5; // brightness levels in conveyor
        auto ss = getLED(_startPoint);
        auto ee = getLED(_endPoint);
        for (int led = ss; led < ee; led++) {
            auto n = (-led + (m / 100)) % levels;
            if (_speed < 0) {
                n = (led + (m / 100)) % levels;
            }
            auto b = map(n, 5, 0, 0, APA102_CONVEYOR_BRIGHTNESS);
            if (b > 0) {
                leds[led] = CRGB(0, 0, b);
            }
        }
    }

    int PlayerSpeedModifier(int playerPosition) const {
        if (_alive && playerPosition > _startPoint && playerPosition < _endPoint) {
            return _speed;
        }
        return 0;
    }
};

class ConveyorPool {
    Conveyor _pool[CONVEYOR_COUNT] = {};
public:
    void Kill() {
        for (auto conveyor: _pool) {
            conveyor.Kill();
        }
    }

    void Tick(unsigned long mm) {
        for (auto const& conveyor: _pool) {
            conveyor.Tick(mm);
        }
    }

    void Spawn(int startPoint, int endPoint, int dir){
        for(auto conveyor : _pool){
            if(!conveyor.Alive()){
                conveyor.Spawn(startPoint, endPoint, dir);
                return;
            }
        }
    }

    int PlayerSpeedModifier(int playerPosition) {
        int speed = 0;
        for (auto const& conveyor: _pool) {
            speed += conveyor.PlayerSpeedModifier(playerPosition);
        }
        return speed;
    }
};

