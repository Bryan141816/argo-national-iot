//#include <Arduino.h>
//#include <SoftwareSerial.h>
//
//void gsmSetup();
//void gsmLoop();
//bool sendSMS(const String& number, const String& text);
//

#ifndef GSM_HANDLER_H
#define GSM_HANDLER_H

#include <Arduino.h>

void gsmSetup(unsigned long baud = 9600);
void gsmLoop();

// main.cpp will call this
bool sendSMS(const String& number, const String& message);

#endif