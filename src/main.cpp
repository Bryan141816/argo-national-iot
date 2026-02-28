#include <Arduino.h>
#include <gsm_handler.h>
#include <dht_handler.h>
#include <soil_moisture_handler.h>
#include <pressure_handler.h>
#include <flow_handler.h>
#include <motor_handler.h>

// ================= USER SETTINGS =================
static const String PHONE_NUMBER = "09956229639";   // Recommended: +639xxxxxxxxx

static const unsigned long SENSOR_INTERVAL_MS = 1000;
static const unsigned long SMS_COOLDOWN_MS    = 5000;

// Soil moisture thresholds (percent)
static const int SOIL_LOW_ON_PERCENT   = 50;  // Soil < 50%  → LOW
static const int SOIL_HIGH_OFF_PERCENT = 80;  // Soil > 80%  → HIGH (OFF)

// Temperature thresholds (°C)
static const float TEMP_HIGH_C = 22.0;   // Above this is considered HIGH
static const float TEMP_LOW_C  = 16.0;   // Below this is considered LOW
// =================================================

enum ConditionCode
{
  COND_A1,   // low soil, high temp
  COND_A2,   // low soil, low temp
  COND_M1,   // mid soil, high temp
  COND_M2,   // mid soil, low temp
  COND_B1,   // high soil, high temp
  COND_B2    // high soil, low temp
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
    case COND_M1: return "M1";
    case COND_M2: return "M2";
    case COND_B1: return "B1";
    case COND_B2: return "B2";
    default:      return "UNKNOWN";
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
  bool lowSoil = (moisture < SOIL_LOW_ON_PERCENT);
  bool midSoil = (moisture >= SOIL_LOW_ON_PERCENT && moisture <= SOIL_HIGH_OFF_PERCENT);
  // highSoil is the remaining case (moisture > SOIL_HIGH_OFF_PERCENT)

  bool highTemp = (tempC > TEMP_HIGH_C);
  bool lowTemp  = (tempC < TEMP_LOW_C);

  if (lowSoil)
  {
    if (highTemp) return COND_A1;
    if (lowTemp)  return COND_A2;
    return COND_A1; // default if temperature is in between
  }
  else if (midSoil)
  {
    if (highTemp) return COND_M1;
    if (lowTemp)  return COND_M2;
    return COND_M1; // default
  }
  else // highSoil
  {
    if (highTemp) return COND_B1;
    if (lowTemp)  return COND_B2;
    return COND_B1; // default
  }
}

// Pump control based ONLY on soil moisture (table rules)
static void applyPumpLogic(int moisture)
{
  if (moisture < SOIL_LOW_ON_PERCENT)          // Low soil → HIGH flow
  {
    motorStartHigh();
    pumpOn = true;
    pumpMode = PUMP_HIGH;
  }
  else if (moisture <= SOIL_HIGH_OFF_PERCENT)  // Mid soil → LOW flow
  {
    motorStartLow();
    pumpOn = true;
    pumpMode = PUMP_LOW;
  }
  else                                          // High soil → OFF
  {
    motorStop();
    pumpOn = false;
    pumpMode = PUMP_OFF;
  }
}

static String buildSensorReportSMS(float temperatureC, int moisturePct,
                                   float pressureBar, float flowRateLpm, bool pumpState,
                                   PumpMode mode, ConditionCode cond)
{
  String msg;
  msg.reserve(250);

  msg += "------ SENSOR READINGS ------\n";
  msg += "Condition: " + String(conditionToString(cond)) + "\n";
  msg += "Temperature (C): " + String(temperatureC, 2) + "\n";
  msg += "Soil Moisture (%): " + String(moisturePct) + "\n";
  msg += "Pressure (bar): " + String(pressureBar, 2) + "\n";
  msg += "Water Flow Rate (L/min): " + String(flowRateLpm, 2) + "\n";

  msg += "Pump Status: ";
  msg += (pumpState ? "ON\n" : "OFF\n");

  msg += "Pump Mode: ";
  msg += String(pumpModeToString(mode));

  return msg;
}

static void printReadings(float temperatureC, int moisturePct,
                          float pressureBar, float flowRateLpm, bool pumpState,
                          PumpMode mode, ConditionCode cond)
{
  Serial.println();
  Serial.println("------ SENSOR READINGS ------");
  Serial.print("Condition: ");                Serial.println(conditionToString(cond));
  Serial.print("Temperature (C): ");          Serial.println(temperatureC, 2);
  Serial.print("Soil Moisture (%): ");        Serial.println(moisturePct);
  Serial.print("Pressure (bar): ");           Serial.println(pressureBar, 2);
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

  dhtSetup();                    // still needed for temperature
  pressureSetup();
  (void)soilMoisturePercent();   // warm‑up read
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

  // Read sensors (humidity is ignored)
  dhtLoop();
  const float temperature = getTemperatureC();   // DHT provides temperature
  const int moisture      = soilMoisturePercent();
  const float pressure    = getPressure();
  const float flowrate    = getFlowRate();

  // Apply pump rules (based only on soil moisture)
  applyPumpLogic(moisture);

  // Determine condition label (based on soil + temperature)
  const ConditionCode cond = determineCondition(moisture, temperature);

  // Print to Serial (humidity omitted)
  printReadings(temperature, moisture, pressure, flowrate, pumpOn, pumpMode, cond);

  // SMS: send only on pump ON/OFF change (or first time) + cooldown
  const bool stateChanged   = (!hasNotifiedYet) || (pumpOn != lastNotifiedPumpState);
  const bool cooldownPassed = (now - lastSmsSentMs >= SMS_COOLDOWN_MS);

  if (stateChanged && cooldownPassed)
  {
    const String sms = buildSensorReportSMS(
      temperature, moisture, pressure, flowrate,
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