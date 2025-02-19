#include "Arduino.h"

const float GRAVITY = 0.7; // [1/tick]
const float SLOWDOWN = 0.97;

class Particle {
  public:
    void Spawn(int pos, int baseSpeed);
    void Tick();
    void Kill();
    bool Alive();
    double _pos;
    double _speed; // [pxl/tick * resolution]
    int _life; // [ticks]
    int _maxLife; // [ticks]
};

void Particle::Spawn(int pos, int baseSpeed){
    _pos = (double) pos;
    _speed = (double) baseSpeed + (double) random(-100, 100) / 4.0;
    _maxLife = (int) random(50) + abs((int) _speed) * 2;
    _life = _maxLife;
}

void Particle::Tick(){
    if (!Alive()) {
        return;
    }
    _life--;
    _speed -= GRAVITY;
    _speed *= SLOWDOWN + (double) random(100) / 10000.0;
    _pos += _speed;
    if (_pos > 1000) { // TODO 28-Jul-2023/mvk: use VIRTUAL_LED_COUNT
        // no bouncing on top: _pos = 2000 - _pos;
        if (_speed > 0) {
            _speed = _speed * 0.9;
        }
    } else if (_pos < 0) {
        _pos = -_pos;
        _speed = -_speed;
    }
}

bool Particle::Alive(){
    return _life > 0;
}

void Particle::Kill(){
    _life = 0;
}
