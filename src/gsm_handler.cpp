#include "gsm_handler.h"
#include <SoftwareSerial.h>

// If wired: SIM800L TX -> D10, RX -> D11
static SoftwareSerial sim800(10, 11);

// ---------- Helper: flush SIM800 buffer ----------
static void flushSIM800()
{
  while (sim800.available()) sim800.read();
}

// ---------- Helper: wait for response ----------
static bool waitForResponse(const char* expected, unsigned long timeoutMs)
{
  unsigned long start = millis();
  String buffer;

  while (millis() - start < timeoutMs)
  {
    while (sim800.available())
    {
      char c = (char)sim800.read();
      buffer += c;

      // Mirror to Serial (debug)
      if (Serial) Serial.write(c);

      if (buffer.indexOf(expected) != -1) return true;
      if (buffer.indexOf("ERROR") != -1) return false;
    }
  }
  return false;
}

// ---------- Check SIM ready ----------
static bool isSIMReady()
{
  flushSIM800();
  sim800.println("AT+CPIN?");
  return waitForResponse("READY", 5000);
}

// ---------- Wait for network registration ----------
static bool waitForNetwork(unsigned long totalTimeoutMs = 30000)
{
  unsigned long start = millis();

  while (millis() - start < totalTimeoutMs)
  {
    flushSIM800();
    sim800.println("AT+CREG?");

    unsigned long t0 = millis();
    String buf;

    while (millis() - t0 < 3000)
    {
      while (sim800.available())
      {
        char c = (char)sim800.read();
        buf += c;
        if (Serial) Serial.write(c);
      }
    }

    // home = ,1  roaming = ,5
    if (buf.indexOf(",1") != -1 || buf.indexOf(",5") != -1)
      return true;

    delay(1000);
  }
  return false;
}

// ===== Public API expected by main.cpp =====

void gsmSetup(unsigned long baud)
{
  if (Serial) Serial.println("Initializing SIM800L...");
  sim800.begin(baud);

  delay(3000);

  // Echo off (optional)
  sim800.println("ATE0");
  waitForResponse("OK", 2000);

  // Basic AT test (optional)
  sim800.println("AT");
  waitForResponse("OK", 2000);
}

void gsmLoop()
{
  // Keep empty unless you later add SMS receive, etc.
}

// ✅ This is what main.cpp should call
bool sendSMS(const String& number, const String& message)
{
  if (Serial) Serial.println("Sending SMS...");

  if (!isSIMReady())
  {
    if (Serial) Serial.println("SIM not ready.");
    return false;
  }

  if (Serial) Serial.println("Waiting for network...");
  if (!waitForNetwork(30000))
  {
    if (Serial) Serial.println("Network not registered.");
    return false;
  }

  // Basic attention
  sim800.println("AT");
  if (!waitForResponse("OK", 2000)) return false;

  // Text mode
  sim800.println("AT+CMGF=1");
  if (!waitForResponse("OK", 3000)) return false;

  // Recipient
  sim800.print("AT+CMGS=\"");
  sim800.print(number);
  sim800.println("\"");

  // Wait for prompt
  if (!waitForResponse(">", 5000)) return false;

  // Body + Ctrl+Z
  sim800.print(message);
  sim800.write((uint8_t)26);

  // Accept +CMGS then OK (some modules may only return OK)
  if (!waitForResponse("+CMGS", 20000))
  {
    if (!waitForResponse("OK", 20000)) return false;
    return true;
  }

  if (!waitForResponse("OK", 20000)) return false;

  if (Serial) Serial.println("SMS Sent Successfully!");
  return true;
}