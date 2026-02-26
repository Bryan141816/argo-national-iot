#include <soil_moisture_handler.h>

static uint8_t moisturePin = A0;
static int dryVal = 1023;
static int wetVal = 530;

void soilMoistureSetup()
{
  pinMode(moisturePin, INPUT);
}
int soilMoisturePercent()
{
  int raw = analogRead(moisturePin);
  int pct = map(raw, dryVal, wetVal, 0, 100);
  return constrain(pct, 0, 100);
}