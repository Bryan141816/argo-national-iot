#include <flow_handler.h>

volatile int NumPulses = 0;
const int PinSensor = 3;
const float PULSES_PER_LITER = 7.5;   // Sensor calibration factor

float flow_L_m = 0;
float volume = 0;

unsigned long lastMeasureTime = 0;
const unsigned long MEASURE_INTERVAL = 1000; // 1 second

// Interrupt Service Routine
void PulseCount()
{
    NumPulses++;
}

// Setup
void waterFlowSetup()
{
    pinMode(PinSensor, INPUT);
    attachInterrupt(digitalPinToInterrupt(PinSensor), PulseCount, RISING);
    Serial.println("Water Flow Sensor Initialized");
}

// Non-blocking measurement loop (call frequently)
void waterFlowLoop()
{
    unsigned long now = millis();
    if (now - lastMeasureTime >= MEASURE_INTERVAL)
    {
        lastMeasureTime = now;

        // Safely read and reset pulse counter
        noInterrupts();
        int pulses = NumPulses;
        NumPulses = 0;
        interrupts();

        // Calculate flow rate in L/min
        // pulses per second ÷ (pulses per liter) × 60 = L/min
        flow_L_m = (pulses / PULSES_PER_LITER) * 60.0;

        // Accumulate total volume (liters)
        volume += flow_L_m / 60.0;   // because measurement interval is 1 second

        // Optional debug output
        // Serial.print("Pulses: ");
        // Serial.print(pulses);
        // Serial.print(" -> Flow: ");
        // Serial.print(flow_L_m);
        // Serial.println(" L/min");
    }
}

// Getters
float getFlowRate()
{
    return flow_L_m;
}

float getTotalVolume()
{
    return volume;
}

void resetWaterVolume()
{
    volume = 0;
}