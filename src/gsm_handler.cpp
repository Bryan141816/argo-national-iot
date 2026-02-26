#include "gsm_handler.h"
#include <SoftwareSerial.h>

// SIM800L TX -> Arduino D10 (Arduino RX)
// SIM800L RX -> Arduino D11 (Arduino TX)
static SoftwareSerial sim800(10, 11);

// ============ Tunable Settings ============
static const int MIN_CSQ              = 10;      // 0..31 (99 unknown). 10+ is “OK”
static const int MAX_SEND_RETRIES     = 3;       // retries for whole send operation
static const int MAX_NET_RETRIES      = 3;       // retries for network+signal readiness
static const unsigned long NET_WAIT_MS = 15000;
static const unsigned long SIG_WAIT_MS = 15000;
// =========================================

static void flushSIM800()
{
  while (sim800.available()) sim800.read();
}

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

      // debug mirror to Serial
      if (Serial) Serial.write(c);

      if (buffer.indexOf(expected) != -1) return true;
      if (buffer.indexOf("ERROR") != -1) return false;
    }
  }
  return false;
}

static bool isSIMReady()
{
  flushSIM800();
  sim800.println("AT+CPIN?");
  return waitForResponse("READY", 5000);
}

// home: +CREG: x,1  roaming: +CREG: x,5
static bool waitForNetwork(unsigned long totalTimeoutMs)
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

    if (buf.indexOf(",1") != -1 || buf.indexOf(",5") != -1)
      return true;

    delay(1000);
  }

  return false;
}

// Returns 0..31 or 99 (unknown)
static int readCSQ()
{
  flushSIM800();
  sim800.println("AT+CSQ");

  unsigned long start = millis();
  String buf;

  while (millis() - start < 3000)
  {
    while (sim800.available())
    {
      char c = (char)sim800.read();
      buf += c;
      if (Serial) Serial.write(c);
    }
  }

  int idx = buf.indexOf("+CSQ:");
  if (idx == -1) return 99;

  int comma = buf.indexOf(',', idx);
  if (comma == -1) return 99;

  int startNum = idx + 5;
  while (startNum < (int)buf.length() && buf[startNum] == ' ') startNum++;

  String rssiStr = buf.substring(startNum, comma);
  rssiStr.trim();

  return rssiStr.toInt();
}

static bool waitForSignal(int minCSQ, unsigned long totalTimeoutMs)
{
  unsigned long start = millis();

  while (millis() - start < totalTimeoutMs)
  {
    int csq = readCSQ();

    if (Serial)
    {
      Serial.print("Signal check: CSQ=");
      Serial.println(csq);
    }

    // csq 99 = unknown => treat as weak
    if (csq != 99 && csq >= minCSQ) return true;

    delay(2000);
  }
  return false;
}

void gsmSetup(unsigned long baud)
{
  if (Serial) Serial.println("Initializing SIM800L...");
  sim800.begin(baud);

  delay(3000);

  // Optional: echo off
  sim800.println("ATE0");
  waitForResponse("OK", 2000);

  // Quick AT test
  sim800.println("AT");
  waitForResponse("OK", 2000);
}

void gsmLoop()
{
  // Keep empty for now
}

bool sendSMS(const String& number, const String& message)
{
  for (int attempt = 1; attempt <= MAX_SEND_RETRIES; attempt++)
  {
    if (Serial)
    {
      Serial.println();
      Serial.print("[GSM] SMS send attempt ");
      Serial.print(attempt);
      Serial.print("/");
      Serial.println(MAX_SEND_RETRIES);
    }

    // SIM ready
    if (!isSIMReady())
    {
      if (Serial) Serial.println("[GSM] SIM not ready. Retrying...");
      delay(2000);
      continue;
    }

    // Network + signal checks (retry)
    bool ready = false;
    for (int netTry = 1; netTry <= MAX_NET_RETRIES; netTry++)
    {
      if (Serial)
      {
        Serial.print("[GSM] Network try ");
        Serial.print(netTry);
        Serial.print("/");
        Serial.println(MAX_NET_RETRIES);
      }

      if (!waitForNetwork(NET_WAIT_MS))
      {
        if (Serial) Serial.println("[GSM] Not registered yet. Retrying...");
        delay(2000);
        continue;
      }

      if (!waitForSignal(MIN_CSQ, SIG_WAIT_MS))
      {
        if (Serial) Serial.println("[GSM] Weak/unknown signal. Retrying...");
        delay(2000);
        continue;
      }

      ready = true;
      break;
    }

    if (!ready)
    {
      if (Serial) Serial.println("[GSM] Network/signal not ready. Retrying full send...");
      delay(3000);
      continue;
    }

    // SMS send sequence
    sim800.println("AT");
    if (!waitForResponse("OK", 2000))
    {
      if (Serial) Serial.println("[GSM] AT failed. Retrying...");
      delay(1500);
      continue;
    }

    sim800.println("AT+CMGF=1");
    if (!waitForResponse("OK", 3000))
    {
      if (Serial) Serial.println("[GSM] Failed to set text mode. Retrying...");
      delay(1500);
      continue;
    }

    sim800.print("AT+CMGS=\"");
    sim800.print(number);
    sim800.println("\"");

    if (!waitForResponse(">", 5000))
    {
      if (Serial) Serial.println("[GSM] No '>' prompt. Retrying...");
      delay(1500);
      continue;
    }

    sim800.print(message);
    sim800.write((uint8_t)26); // CTRL+Z

    // Many modules reply with +CMGS then OK
    bool ok = waitForResponse("+CMGS", 20000);
    if (!ok)
    {
      // fallback: some show only OK
      ok = waitForResponse("OK", 20000);
    }
    else
    {
      ok = waitForResponse("OK", 20000);
    }

    if (ok)
    {
      if (Serial) Serial.println("[GSM] SMS sent successfully.");
      return true;
    }

    if (Serial) Serial.println("[GSM] Send failed. Retrying...");
    delay(3000);
  }

  if (Serial) Serial.println("[GSM] SMS failed after all retries.");
  return false;
}