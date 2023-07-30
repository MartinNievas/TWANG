#include "Arduino.h"
#include "Constants.h"

#define PARTICLE_COUNT 40

const float GRAVITY = 0.7; // [1/tick]
const float SLOWDOWN = 0.97;

class Particle {
    double _pos;
    double _speed; // [pxl/tick * resolution]
    int _life; // [ticks]
    int _maxLife; // [ticks]

public:
    void Spawn(int pos, int baseSpeed) {
        _pos = (double) pos;
        _speed = (double) baseSpeed + (double) random(-100, 100) / 4.0;
        _maxLife = (int) random(50) + abs((int) _speed) * 2;
        _life = _maxLife;
        Serial.println("spawn " + String(_life));
    }

    void Tick() {
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
        Draw();
    }

    bool Alive() const {
        Serial.println("alive " + String(_life));
        return _life > 0;
    }

    void Kill() {
        _life = 0;
    }

private:
    void Draw() const {
        uint8_t brightness;
        if (_life <= 5) {
            brightness = (5 - _life) * 10;
            leds[getLED((int) _pos)] += CRGB(brightness, brightness / 2, brightness / 2);
        } else {
            brightness = map(_life, 0, _maxLife, 50, 255);
            uint8_t orange = random8(25);
            leds[getLED((int) _pos)] = CRGB(brightness, orange, 0);
        }
    }
};

class ParticlePool {
public:
    void Spawn(int pos, int baseSpeed) const {
        for (auto i = 0; i < PARTICLE_COUNT; i++) {
            auto particle = _particles[i];
            particle.Spawn(pos, baseSpeed);
        }
    }

    bool Tick() const {
        for (auto i = 0; i < PARTICLE_COUNT; i++) {
            auto particle = _particles[i];
            particle.Tick();
        }
        for (auto i = 0; i < PARTICLE_COUNT; i++) {
            auto particle = _particles[i];
            if (particle.Alive()) {
                return true;
            }
        }
        return false;
    }

    void Kill() const {
        for (auto particle: _particles) {
            particle.Kill();
        }
    }

private:
    Particle _particles[PARTICLE_COUNT] = {};
};
