#include <Arduino.h>

// Setup function
void waterFlowSetup();

// Loop function (call inside main loop)
void waterFlowLoop();

// Optional getter functions
float getFlowRate();
float getTotalVolume();

// Reset volume manually
void resetWaterVolume();