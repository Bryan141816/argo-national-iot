#include <Arduino.h>
#include <gsm_handler.h>
#include <dht_handler.h>
#include <soil_moisture_handler.h>
#include <pressure_handler.h>
#include <flow_handler.h>
// #include <relay_handler.h>
#include <motor_handler.h>

// ================= USER SETTINGS =================
static const String PHONE_NUMBER = "09999698585";   // Recommended: +639xxxxxxxxx

static const unsigned long SENSOR_INTERVAL_MS = 1000;
static const unsigned long SMS_COOLDOWN_MS    = 5000;

// Soil moisture thresholds
static const int SOIL_LOW_ON_PERCENT   = 50;  // Soil < 50% means "low"
static const int SOIL_HIGH_OFF_PERCENT = 80;  // Soil >= 80% means "high" -> OFF always

// Temperature thresholds
static const float TEMP_HIGH_C = 22.0;  // High temp > 22C
static const float TEMP_LOW_C  = 16.0;  // Low temp  < 16C

// Humidity thresholds for your flow rules
// NOTE: you said "low RH" but wrote ">80-90%". That's high RH.
// This implementation treats "low RH" as <= thresholds below.
static const float HUMIDITY_A1_MAX = 80.0;  // A1 needs RH <= 80%
static const float HUMIDITY_A2_MAX = 35.0;  // A2 needs RH <= 35%
// ================================================

enum ConditionCode
{
  COND_A1,   // low soil, high temp
  COND_A2,   // low soil, low temp
  COND_B1,   // high soil, high temp
  COND_B2,   // high soil, low temp
  COND_MID   // anything else (middle zones)
};

enum PumpMode
{
  PUMP_OFF,
  PUMP_LOW,
  PUMP_HIGH
};

static unsigned long lastSensorReadMs = 0;
static unsigned long lastSmsSentMs    = 0;

static bool pumpOn = false;
static PumpMode pumpMode = PUMP_OFF;

// SMS state tracking (send only on ON/OFF change)
static bool lastNotifiedPumpState = false;
static bool hasNotifiedYet = false;

// -------------------- Helpers --------------------
static const char* conditionToString(ConditionCode c)
{
  switch (c)
  {
    case COND_A1: return "A1";
    case COND_A2: return "A2";
    case COND_B1: return "B1";
    case COND_B2: return "B2";
    default:      return "MID";
  }
}

static const char* pumpModeToString(PumpMode mode)
{
  switch (mode)
  {
    case PUMP_HIGH: return "HIGH FLOW (2-4 bar)";
    case PUMP_LOW:  return "LOW FLOW (1-2 bar)";
    default:        return "OFF";
  }
}

static ConditionCode determineCondition(int moisture, float tempC)
{
  const bool soilLow  = (moisture < SOIL_LOW_ON_PERCENT);
  const bool soilHigh = (moisture >= SOIL_HIGH_OFF_PERCENT);

  const bool tempHigh = (tempC > TEMP_HIGH_C);
  const bool tempLow  = (tempC < TEMP_LOW_C);

  if (soilLow && tempHigh) return COND_A1;
  if (soilLow && tempLow)  return COND_A2;

  if (soilHigh && tempHigh) return COND_B1;
  if (soilHigh && tempLow)  return COND_B2;

  return COND_MID;
}

// Apply pump control based on YOUR RULES + hysteresis middle zone
static void applyPumpLogic(int moisture, float tempC, float humidityPct)
{
  // RULE: If soil moisture is high (~80%) => OFF always
  if (moisture >= SOIL_HIGH_OFF_PERCENT)
  {
    motorStop();
    pumpOn = false;
    pumpMode = PUMP_OFF;
    return;
  }

  // Soil LOW (<50): pump ON, choose HIGH/LOW flow based on temp+humidity rules
  if (moisture < SOIL_LOW_ON_PERCENT)
  {
    const bool tempHigh = (tempC > TEMP_HIGH_C);
    const bool tempLow  = (tempC < TEMP_LOW_C);

    // A1: HIGH flow if soil low + temp high + low humidity
    if (tempHigh && humidityPct <= HUMIDITY_A1_MAX)
    {
      motorStartHigh();
      pumpOn = true;
      pumpMode = PUMP_HIGH;
      return;
    }

    // A2: LOW flow if soil low + temp low + low humidity
    if (tempLow && humidityPct <= HUMIDITY_A2_MAX)
    {
      motorStartLow();
      pumpOn = true;
      pumpMode = PUMP_LOW;
      return;
    }

    // If soil low but doesn't match A1/A2 rules:
    // keep ON but default to LOW flow (safer)
    motorStartLow();
    pumpOn = true;
    pumpMode = PUMP_LOW;
    return;
  }

  // Soil is in middle zone (50–80): keep previous state (hysteresis)
  if (pumpOn)
  {
    motorStartLow();
    pumpMode = PUMP_LOW;
  }
  else
  {
    motorStop();
    pumpMode = PUMP_OFF;
  }
}

static String buildSensorReportSMS(float temperatureC, float humidityPct, int moisturePct,
                                  float flowRateLpm, bool pumpState,
                                  PumpMode mode, ConditionCode cond)
{
  String msg;
  msg.reserve(260);

  msg += "------ SENSOR READINGS ------\n";
  msg += "Condition: " + String(conditionToString(cond)) + "\n";
  msg += "Temperature (C): " + String(temperatureC, 2) + "\n";
  msg += "Humidity (%): " + String(humidityPct, 2) + "\n";
  msg += "Soil Moisture (%): " + String(moisturePct) + "\n";
  msg += "Water Flow Rate (L/min): " + String(flowRateLpm, 2) + "\n";

  msg += "Pump Status: ";
  msg += (pumpState ? "ON\n" : "OFF\n");

  msg += "Pump Mode: ";
  msg += String(pumpModeToString(mode));

  return msg;
}

static void printReadings(float temperatureC, float humidityPct, int moisturePct,
                          float flowRateLpm, bool pumpState,
                          PumpMode mode, ConditionCode cond)
{
  Serial.println();
  Serial.println("------ SENSOR READINGS ------");
  Serial.print("Condition: ");                Serial.println(conditionToString(cond));
  Serial.print("Temperature (C): ");          Serial.println(temperatureC, 2);
  Serial.print("Humidity (%): ");             Serial.println(humidityPct, 2);
  Serial.print("Soil Moisture (%): ");        Serial.println(moisturePct);
  Serial.print("Water Flow Rate (L/min): ");  Serial.println(flowRateLpm, 2);

  Serial.print("Pump Status: ");              Serial.println(pumpState ? "ON" : "OFF");
  Serial.print("Pump Mode: ");                Serial.println(pumpModeToString(mode));

  Serial.println("-----------------------------");
}

// -------------------- Arduino --------------------
void setup()
{
  Serial.begin(9600);
  delay(200);

  Serial.println("Booting system...");

  motorSetup();
  gsmSetup();

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
  if (now - lastSensorReadMs < SENSOR_INTERVAL_MS) return;
  lastSensorReadMs = now;

  // Read sensors
  dhtLoop();
  const float temperature = getTemperatureC();
  const float humidity    = getHumidity();
  const int moisture      = soilMoisturePercent();
  const float flowrate    = getFlowRate();

  // Apply new pump rules
  applyPumpLogic(moisture, temperature, humidity);

  // Determine condition label
  const ConditionCode cond = determineCondition(moisture, temperature);

  // Serial output
  printReadings(temperature, humidity, moisture, flowrate, pumpOn, pumpMode, cond);

  // SMS: send only on pump ON/OFF change (or first time) + cooldown
  const bool stateChanged   = (!hasNotifiedYet) || (pumpOn != lastNotifiedPumpState);
  const bool cooldownPassed = (now - lastSmsSentMs >= SMS_COOLDOWN_MS);

  if (stateChanged && cooldownPassed)
  {
    const String sms = buildSensorReportSMS(
      temperature, humidity, moisture, flowrate,
      pumpOn, pumpMode, cond
    );

    Serial.println("Sending SMS report...");
    if (sendSMS(PHONE_NUMBER, sms))
    {
      Serial.println("✅ SMS sent.");
      lastSmsSentMs = now;
      lastNotifiedPumpState = pumpOn;
      hasNotifiedYet = true;
    }
    else
    {
      Serial.println("❌ SMS failed.");
    }
  }
}