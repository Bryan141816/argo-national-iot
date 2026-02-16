#include <flow_handler.h>

volatile int NumPulses = 0;
const int PinSensor = 3;
const float factor_conversion = 7.5;

float flow_L_m = 0;
float volume = 0;

unsigned long lastMeasureTime = 0;
const unsigned long measureInterval = 1000; // 1 second


// Interrupt function
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


// Loop (non-blocking version)
void waterFlowLoop()
{
    unsigned long currentMillis = millis();

    if (currentMillis - lastMeasureTime >= measureInterval)
    {
        lastMeasureTime = currentMillis;

        noInterrupts();
        int pulses = NumPulses;
        NumPulses = 0;
        interrupts();

        flow_L_m = pulses / factor_conversion;
        volume += (flow_L_m / 60.0);  // since measured per second

        // Serial.print("Flow: ");
        // Serial.print(flow_L_m, 3);
        // Serial.print(" L/min | Volume: ");
        // Serial.print(volume, 3);
        // Serial.println(" L");
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
