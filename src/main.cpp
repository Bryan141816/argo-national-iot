#include <Arduino.h>
#include <gsm_handler.h>
#include <dht_handler.h>
#include <soil_moisture_handler.h>
#include <pressure_handler.h>
#include <flow_handler.h>
#include <motor_handler.h>

// ================= USER SETTINGS =================
static const String PHONE_NUMBER = "09672285291";   // Recommended: +639xxxxxxxxx

static const unsigned long SENSOR_INTERVAL_MS = 1000;
static const unsigned long SMS_COOLDOWN_MS    = 5000;

// Soil moisture thresholds (percent)
static const int SOIL_LOW_ON_PERCENT   = 50;  // Soil < 50%  → LOW
static const int SOIL_HIGH_OFF_PERCENT = 80;  // Soil > 80%  → HIGH (OFF)

// Temperature thresholds (°C)
static const float TEMP_HIGH_C = 30.0;   // Above this is considered HIGH
static const float TEMP_LOW_C  = 20.0;   // Below this is considered LOW
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
static unsigned long lastSmsSentMs    = 0;          // will be initialised to allow first send

static bool pumpOn = false;
static PumpMode pumpMode = PUMP_OFF;

// No longer need lastNotifiedPumpState / hasNotifiedYet for SMS logic,
// but we keep them for potential future use (optional). They are not used here.

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
    case PUMP_HIGH: return "HIGH FLOW";
    case PUMP_LOW:  return "LOW FLOW";
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

// Pump control based on condition (A1, A2, M1, M2, B1, B2)
static void applyPumpLogic(ConditionCode cond)
{
  switch(cond)
  {
    case COND_A1:  // low soil, high temp → HIGH flow
      motorStartHigh();
      pumpOn = true;
      pumpMode = PUMP_HIGH;
      break;
      
    case COND_A2:  // low soil, low temp → LOW flow
    case COND_M1:  // mid soil, high temp → LOW flow
      motorStartLow();
      pumpOn = true;
      pumpMode = PUMP_LOW;
      break;
      
    case COND_M2:  // mid soil, low temp → OFF
    case COND_B1:  // high soil, high temp → OFF
    case COND_B2:  // high soil, low temp → OFF
      motorStop();
      pumpOn = false;
      pumpMode = PUMP_OFF;
      break;
      
    default:
      motorStop();
      pumpOn = false;
      pumpMode = PUMP_OFF;
      break;
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

  // Allow first SMS to be sent immediately (cooldown passes right away)
  lastSmsSentMs = -SMS_COOLDOWN_MS;

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

  // Determine condition label (based on soil + temperature)
  // This MUST come BEFORE applyPumpLogic since we need cond
  const ConditionCode cond = determineCondition(moisture, temperature);

  // Keep previous pump state to detect ON transition
  static bool prevPumpOn = false;

  // Apply pump rules (now based on condition, not just moisture)
  applyPumpLogic(cond);  // Now cond is defined!

  // Print to Serial (humidity omitted)
  printReadings(temperature, moisture, pressure, flowrate, pumpOn, pumpMode, cond);

  // SMS: send only when pump turns ON (transition from OFF to ON)
  bool justTurnedOn = (pumpOn && !prevPumpOn);
  bool cooldownPassed = (now - lastSmsSentMs >= SMS_COOLDOWN_MS);

  if (justTurnedOn && cooldownPassed)
  {
    const String sms = buildSensorReportSMS(
      temperature, moisture, pressure, flowrate,
      pumpOn, pumpMode, cond
    );

    Serial.println("Sending SMS report (pump ON)...");
    if (sendSMS(PHONE_NUMBER, sms))
    {
      Serial.println("✅ SMS sent.");
      lastSmsSentMs = now;
    }
    else
    {
      Serial.println("❌ SMS failed.");
    }
  }

  // Update previous state for next loop
  prevPumpOn = pumpOn;
}