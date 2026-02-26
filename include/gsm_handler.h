#ifndef GSM_HANDLER_H
#define GSM_HANDLER_H

#include <Arduino.h>

void gsmSetup(unsigned long baud = 9600);
void gsmLoop();

// Sends SMS with automatic retry on weak signal/network.
// Returns true if sent successfully.
bool sendSMS(const String& number, const String& message);

#endif