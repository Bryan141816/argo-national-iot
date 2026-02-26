#include <Arduino.h>
#include <gsm_handler.h>
#include <dht_handler.h>
#include <soil_moisture_handler.h>
#include <pressure_handler.h>
#include <flow_handler.h>
#include <motor_handler.h>

// ================== USER SETTINGS ==================
static const String PHONE_NUMBER = "09089865033";   // recommended format: +639xxxxxxxxx
static const unsigned long SENSOR_INTERVAL_MS = 1000;
static const unsigned long SMS_COOLDOWN_MS    = 5000;
static const int MOISTURE_THRESHOLD_PERCENT   = 50;
static const float TEMP_HIGH_SPEED_C          = 22.0;
// ===================================================

// runtime timers
static unsigned long lastSensorReadMs = 0;
static unsigned long lastSmsSentMs    = 0;

// pump + SMS state tracking
static bool pumpOn = false;
static bool lastNotifiedPumpState = false;
static bool hasNotifiedYet = false;

// Build the full sensor report (same content as Serial print)
static String buildSensorReport(float temperature, float humidity, int moisture, float flowrate, bool pumpOnNow)
{
  String msg;
  msg.reserve(180); // helps avoid heap fragmentation (optional)

  msg += "------ SENSOR READINGS ------\n";
  msg += "Temperature (C): " + String(temperature, 2) + "\n";
  msg += "Humidity (%): " + String(humidity, 2) + "\n";
  msg += "Soil Moisture (%): " + String(moisture) + "\n";
  msg += "Water Flow Rate (L/min): " + String(flowrate, 2) + "\n";
  msg += "Pump Status: ";
  msg += (pumpOnNow ? "ON" : "OFF");

  return msg;
}

static void printReadings(float temperature, float humidity, int moisture, float flowrate, bool pumpOnNow)
{
  Serial.println();
  Serial.println("------ SENSOR READINGS ------");
  Serial.print("Temperature (C): ");     Serial.println(temperature, 2);
  Serial.print("Humidity (%): ");        Serial.println(humidity, 2);
  Serial.print("Soil Moisture (%): ");   Serial.println(moisture);
  Serial.print("Water Flow Rate (L/min): "); Serial.println(flowrate, 2);
  Serial.print("Pump Status: ");         Serial.println(pumpOnNow ? "ON" : "OFF");
  Serial.println("-----------------------------");
}

void setup()
{
  Serial.begin(9600);
  delay(200);

  Serial.println("Booting system...");

  motorSetup();
  gsmSetup(); // GSM init

  dhtSetup();
  (void)soilMoisturePercent();
  waterFlowSetup();

  Serial.println("System ready.");
}

void loop()
{
  gsmLoop();
  waterFlowLoop();

  const unsigned long now = millis();

  // read sensors on interval
  if (now - lastSensorReadMs < SENSOR_INTERVAL_MS) return;
  lastSensorReadMs = now;

  dhtLoop();

  const float temperature = getTemperatureC();
  const float humidity    = getHumidity();
  const int moisture      = soilMoisturePercent();
  const float flowrate    = getFlowRate();

  // Decide pump logic
  const bool desiredPumpOn = (moisture <= MOISTURE_THRESHOLD_PERCENT);

  // Apply motor control
  if (desiredPumpOn)
  {
    if (temperature >= TEMP_HIGH_SPEED_C) motorStartHigh();
    else                                  motorStartLow();
  }
  else
  {
    motorStop();
  }

  pumpOn = desiredPumpOn;

  // Serial log
  printReadings(temperature, humidity, moisture, flowrate, pumpOn);

  // SMS conditions:
  const bool stateChanged   = (!hasNotifiedYet) || (pumpOn != lastNotifiedPumpState);
  const bool cooldownPassed = (now - lastSmsSentMs >= SMS_COOLDOWN_MS);

  if (stateChanged && cooldownPassed)
  {
    const String smsMessage = buildSensorReport(temperature, humidity, moisture, flowrate, pumpOn);

    Serial.println("Attempting to send SMS report...");

    if (sendSMS(PHONE_NUMBER, smsMessage))
    {
      Serial.println("✅ SMS report sent.");
      lastSmsSentMs = now;
      lastNotifiedPumpState = pumpOn;
      hasNotifiedYet = true;
    }
    else
    {
      Serial.println("❌ SMS failed (check power/signal/SIM/wiring).");
      // Note: cooldown stays based on last successful send.
      // If you want to retry faster on failure, tell me.
    }
  }
}