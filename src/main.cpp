#include <Arduino.h>
#include <gsm_handler.h>
#include <dht_handler.h>
#include <soil_moisture_handler.h>
#include <pressure_handler.h>
#include <flow_handler.h>
// #include <relay_handler.h>
#include <motor_handler.h>

String phoneNumber = "09089865033";

unsigned long lastDhtRead = 0;
const unsigned long dhtInterval = 1000;

bool pumpOn = false;

// NEW: track last SMS-notified state
bool lastNotifiedPumpState = false;
bool hasNotifiedYet = false;

unsigned long lastSmsTime = 0;
const unsigned long smsCooldown = 5000;

// ✅ NEW: Build the full sensor report message (same as Serial output)
String buildSensorReport(float temperature, float humidity, int moisture, float flowrate, bool pumpOn)
{
  String msg = "------ SENSOR READINGS ------\n";
  msg += "Temperature (C): " + String(temperature, 2) + "\n";
  msg += "Humidity (%): " + String(humidity, 2) + "\n";
  msg += "Soil Moisture (%): " + String(moisture) + "\n";
  msg += "Water Flow Rate (L/min): " + String(flowrate, 2) + "\n";
  msg += "Pump Status: ";
  msg += (pumpOn ? "ON" : "OFF");
  return msg;
}

void setup()
{
  motorSetup();
  gsmSetup();              // ✅ enable GSM
  Serial.begin(9600);

  dhtSetup();
  soilMoisturePercent();
  waterFlowSetup();
}

void loop()
{
  gsmLoop();               // ✅ enable GSM loop (if your handler needs it)
  waterFlowLoop();

  unsigned long currentMillis = millis();

  if (currentMillis - lastDhtRead >= dhtInterval)
  {
    dhtLoop();
    lastDhtRead = currentMillis;

    float temperature = getTemperatureC();
    float humidity = getHumidity();
    int moisture = soilMoisturePercent();
    float flowrate = getFlowRate();

    // Decide desired pump state based on sensor rules
    bool desiredPumpOn = (moisture <= 50);

    // Apply motor control based on desired state
    if (desiredPumpOn)
    {
      if (temperature >= 22)
        motorStartHigh();
      else
        motorStartLow();
    }
    else
    {
      motorStop();
    }

    // Update actual pumpOn variable
    pumpOn = desiredPumpOn;

    // ===== SERIAL PRINT ALL VALUES =====
    Serial.println("------ SENSOR READINGS ------");
    Serial.print("Temperature (C): ");
    Serial.println(temperature);

    Serial.print("Humidity (%): ");
    Serial.println(humidity);

    Serial.print("Soil Moisture (%): ");
    Serial.println(moisture);

    Serial.print("Water Flow Rate (L/min): ");
    Serial.println(flowrate);

    Serial.print("Pump Status: ");
    Serial.println(pumpOn ? "ON" : "OFF");
    Serial.println("-----------------------------");
    // ===================================

    // Send SMS only if:
    // 1) cooldown passed, AND
    // 2) pump state changed vs last SMS-notified state (or first time)
    bool stateChanged = (!hasNotifiedYet) || (pumpOn != lastNotifiedPumpState);
    bool cooldownPassed = (millis() - lastSmsTime >= smsCooldown);

    if (stateChanged && cooldownPassed)
    {
      // ✅ NEW: Full report message
      String smsMessage = buildSensorReport(temperature, humidity, moisture, flowrate, pumpOn);

      if (!sendSMS(phoneNumber, smsMessage))
      {
        Serial.println("\nSMS FAILED (power/signal/SIM/wiring).");
      }
      else
      {
        Serial.println("Sensor report SMS sent!");
        lastSmsTime = millis();
        lastNotifiedPumpState = pumpOn;
        hasNotifiedYet = true;
      }
    }
  }
}