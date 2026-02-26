#include <motor_handler.h>

// L298N Motor Control

const int enA = 5; // PWM pin
const int in1 = 6; // Direction pin 1
const int in2 = 7; // Direction pin 2

void motorSetup()
{
    pinMode(enA, OUTPUT);
    pinMode(in1, OUTPUT);
    pinMode(in2, OUTPUT);
}

void motorStartLow()
{
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    analogWrite(enA, 255); //190
}

void motorStartHigh()
{
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    analogWrite(enA, 255);
}
void motorStop()
{
    analogWrite(enA, 0);
}