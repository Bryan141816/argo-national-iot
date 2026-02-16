#include <Arduino.h>
#include <gsm_handler.h>
#include <dht_handler.h>
#include <soil_moisture_handler.h>
#include <pressure_handler.h>
#include <flow_handler.h>
#include <relay_handler.h>
String phoneNumber = "+639925838621";
String smsMessage  = "Hello! Kamote This is a test SMS from Arduino + SIM800L.";

unsigned long lastDhtRead = 0;
const unsigned long dhtInterval = 1000; 

void setup()
{
  gsmSetup();
  dhtSetup();
  soilMoisturePercent();
  pressureSetup();
  waterFlowSetup();
  relaySetup();

  if (!sendSMS(phoneNumber, smsMessage))
  {
    Serial.println("\nSMS FAILED (power/signal/SIM/wiring).");
  }
}

void loop()
{
  gsmLoop();
  waterFlowLoop();

  unsigned long currentMillis = millis();

  if (currentMillis - lastDhtRead >= dhtInterval)
  {
    lastDhtRead = currentMillis;
    dhtLoop();
    // getTemperatureC();
    // getHumidity();
    // getPressure();
    // soilMoisturePercent();
    // getFlowRate();
    // setRelay(LOW);
  }
}
