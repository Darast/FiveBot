#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include <Arduino.h>

#include "Car.h"

#define UPDATE_FREQUENCY (1000 / 12.8)

static Car* car;

Car::Car(): mMotors {
    Motor( 4,  5),
    Motor( 8,  9),
    Motor(11, 10),
    Motor( 7,  6),
}, mEncoders {
    RotaryEncoder(DDRD, PD2, PD3, PCIE2, PCMSK2, PCINT18, PCINT19), // Pins 2 & 3.
    RotaryEncoder(DDRC, PC0, PC1, PCIE1, PCMSK1, PCINT8, PCINT9), // Pins A0 & A1.
    RotaryEncoder(DDRC, PC2, PC3, PCIE1, PCMSK1, PCINT10, PCINT11), // Pins A2 & A3.
    RotaryEncoder(DDRB, PB4, PB5, PCIE0, PCMSK0, PCINT4, PCINT5), // Pins 12 & 13.
}, mWheels {
    Wheel(mMotors[0], mEncoders[0]),
    Wheel(mMotors[1], mEncoders[1]),
    Wheel(mMotors[2], mEncoders[2]),
    Wheel(mMotors[3], mEncoders[3]),
}, mPosition {
    0,
    0,
}, mOrientation(
    0
) {
    setupSerial();
    setupUpdateTimer();
    mMotors[0].setPower(10);
    mMotors[1].setPower(10);
    mMotors[2].setPower(10);
    mMotors[3].setPower(10);
}

void Car::setupSerial() {
    Serial.begin(2000000);
}

void Car::setupUpdateTimer() {
    // Enable timer 2 compare match B interrupts
    TIMSK2 = _BV(OCIE2B);
    // Set timer 2 to CTC mode (Clear Timer on Compare) with OCR2A
    TCCR2A = _BV(WGM21);
    // Enable the timer with prescaler set to 1024
    TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20);
    // Set the delay to 12.8ms (OCR2A * prescaler / F_CPU)
    OCR2A = 200;
}

void Car::timerLoop() {
    updateOdometry();
}

void Car::updateOdometry() {
    mWheels[0].update();
    mWheels[1].update();
    mWheels[2].update();
    mWheels[3].update();
    const int w[] = {
        mEncoders[0].mPosition,
        mEncoders[1].mPosition,
        mEncoders[2].mPosition,
        mEncoders[3].mPosition,
    };
    mEncoders[0].mPosition = 0;
    mEncoders[1].mPosition = 0;
    mEncoders[2].mPosition = 0;
    mEncoders[3].mPosition = 0;
    const float R = 9.4 / 2, L1 = 15, L2 = 15;
    mPosition[0] += R / 4 * (w[0] + w[1] - w[2] - w[3]);
    mPosition[1] += R / 4 * (w[0] - w[1] - w[2] + w[3]);
    mOrientation += R / 4 / (L1 + L2) * (-w[0] + w[1] - w[2] + w[3]);
}

void Car::mainLoop() {
    // Transmit the state from time to time
    static int skip = 0;
    skip = (skip + 1) % 255;
    if (not skip) {
        printEncoderPositions();
        printEncoderErrors();
        printWheelSpeeds();
        printOdometry();
        Serial.println();
    }
    // Let the ATmega sleep until the next interrupt
    sleep_mode();
}

void Car::printEncoderPositions() const {
    Serial.print("positions");
    for (int i = 0; i < 4; ++i) {
        Serial.print("|");
        Serial.print(mEncoders[i].mPosition);
    }
    Serial.println();
}

void Car::printEncoderErrors() const {
    Serial.print("errors");
    for (int i = 0; i < 4; ++i) {
        Serial.print("|");
        Serial.print(mEncoders[i].getErrorCount());
    }
    Serial.println();
}

void Car::printWheelSpeeds() const {
    Serial.print("speeds");
    for (int i = 0; i < 4; ++i) {
        Serial.print("|");
        Serial.print(mWheels[i].getAngularSpeed(UPDATE_FREQUENCY));
    }
    Serial.println();
}

void Car::printOdometry() const {
    Serial.print("odometry|");
    Serial.print(mPosition[0]);
    Serial.print("|");
    Serial.print(mPosition[1]);
    Serial.print("|");
    Serial.print(mOrientation);
    Serial.println();
}

ISR(PCINT0_vect) {
    const unsigned char port = PINB;
    car->mEncoders[3].update(port);
}

ISR(PCINT1_vect) {
    const unsigned char port = PINC;
    car->mEncoders[1].update(port);
    car->mEncoders[2].update(port);
}

ISR(PCINT2_vect) {
    const unsigned char port = PIND;
    car->mEncoders[0].update(port);
}

ISR(TIMER2_COMPB_vect) {
    car->timerLoop();
}

void setup() {
    car = new Car;
    Serial.println(__FUNCTION__);
}

void loop() {
    car->mainLoop();
}