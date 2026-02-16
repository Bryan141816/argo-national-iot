#pragma once
#include <Arduino.h>

void dhtSetup();
void dhtLoop();

float getTemperatureC();
float getHumidity();
