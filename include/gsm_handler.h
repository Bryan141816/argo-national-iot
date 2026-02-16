#include <Arduino.h>
#include <SoftwareSerial.h>

void gsmSetup();
void gsmLoop();
bool sendSMS(const String& number, const String& text);