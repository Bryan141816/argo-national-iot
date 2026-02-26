#include <pressure_handler.h>

static uint8_t pressureSensorPin = A1;

// Calibration points
const float rawZero = 100.0;   // raw value at 0 bar
const float rawMax  = 200.0;   // raw value at 4 bar
const float maxBar  = 4.0;     // max pressure

void pressureSetup()
{
  pinMode(pressureSensorPin, INPUT);
}

float getPressure()
{
  float raw = analogRead(pressureSensorPin);

  // Linear conversion
  float pressureBar = ((raw - rawZero) * maxBar) /
                      (rawMax - rawZero);

  // Clamp below 0
  if (pressureBar < 0)
  {
    pressureBar = 0;
  }

  // Clamp above maxBar
  if (pressureBar > maxBar)
  {
    pressureBar = maxBar;
  }

  // Serial.print("Pressure: ");
  // Serial.println(raw);
  

  return pressureBar;
}
