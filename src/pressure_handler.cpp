#include <pressure_handler.h>

static uint8_t pressureSensorPin = A1;
const float pressureZero = 90.4;
const float pressureMax = 1023.6;
const float pressuretransducermaxPSI = 100.0;

void pressureSetup()
{
  pinMode(pressureSensorPin, INPUT);
}

float getPressure()
{
  float pressureValue = analogRead(pressureSensorPin);

  pressureValue = ((pressureValue - pressureZero) * pressuretransducermaxPSI) /
                  (pressureMax - pressureZero);
  if (pressureValue < 1.5)
  {
    pressureValue = 0;
  }
  return pressureValue;
}