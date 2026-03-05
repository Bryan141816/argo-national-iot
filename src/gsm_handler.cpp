#include "gsm_handler.h"
#include <SoftwareSerial.h>

static SoftwareSerial sim800(10, 11);

// ================= SETTINGS =================
static const int MIN_CSQ = 10;
static const int MAX_SEND_RETRIES = 3;
static const int MAX_NET_RETRIES = 3;

static const unsigned long CMD_TIMEOUT = 5000;
static const unsigned long NET_TIMEOUT = 15000;
static const unsigned long SIG_TIMEOUT = 15000;
// ============================================


// ================= STATE MACHINE =================
enum GSMState
{
  GSM_IDLE,

  GSM_CHECK_SIM,
  GSM_WAIT_SIM,

  GSM_CHECK_NETWORK,
  GSM_WAIT_NETWORK,

  GSM_CHECK_SIGNAL,
  GSM_WAIT_SIGNAL,

  GSM_SEND_AT,
  GSM_WAIT_AT,

  GSM_SET_TEXT,
  GSM_WAIT_TEXT,

  GSM_SEND_NUMBER,
  GSM_WAIT_PROMPT,

  GSM_SEND_MESSAGE,
  GSM_WAIT_SEND,

  GSM_SUCCESS,
  GSM_FAILED
};

static GSMState state = GSM_IDLE;
// =================================================


// ================ INTERNAL DATA ==================
static String targetNumber;
static String targetMessage;

static int sendRetry = 0;
static int netRetry = 0;

static unsigned long stateTimer = 0;

static String responseBuffer;
// =================================================


// ================= UTILITIES =====================
static void flushSIM800()
{
  while (sim800.available())
    sim800.read();
}

static void sendCommand(const String &cmd)
{
  flushSIM800();
  sim800.println(cmd);
  responseBuffer = "";
  stateTimer = millis();
}

static bool readSerial()
{
  bool updated = false;

  while (sim800.available())
  {
    char c = sim800.read();
    responseBuffer += c;

    if (Serial)
      Serial.write(c);

    updated = true;
  }

  return updated;
}

static bool responseContains(const char *txt)
{
  return responseBuffer.indexOf(txt) != -1;
}

static bool timeout(unsigned long ms)
{
  return millis() - stateTimer > ms;
}
// =================================================


// ================= PUBLIC API ====================

void gsmSetup(unsigned long baud)
{
  if (Serial)
    Serial.println("Initializing SIM800L...");

  sim800.begin(baud);

  delay(3000);

  sendCommand("ATE0");
}

void gsmLoop()
{
  readSerial();

  switch (state)
  {

  case GSM_IDLE:
    break;

  case GSM_CHECK_SIM:
    if (Serial) Serial.println("[GSM] Checking SIM...");
    sendCommand("AT+CPIN?");
    state = GSM_WAIT_SIM;
    break;

  case GSM_WAIT_SIM:

    if (responseContains("READY"))
    {
      state = GSM_CHECK_NETWORK;
    }
    else if (timeout(CMD_TIMEOUT))
    {
      state = GSM_FAILED;
    }

    break;

  case GSM_CHECK_NETWORK:

    if (Serial) Serial.println("[GSM] Checking network...");

    sendCommand("AT+CREG?");
    state = GSM_WAIT_NETWORK;
    break;

  case GSM_WAIT_NETWORK:

    if (responseContains(",1") || responseContains(",5"))
    {
      netRetry = 0;
      state = GSM_CHECK_SIGNAL;
    }
    else if (timeout(CMD_TIMEOUT))
    {

      netRetry++;

      if (netRetry >= MAX_NET_RETRIES)
      {
        state = GSM_FAILED;
      }
      else
      {
        state = GSM_CHECK_NETWORK;
      }
    }

    break;

  case GSM_CHECK_SIGNAL:

    if (Serial) Serial.println("[GSM] Checking signal...");

    sendCommand("AT+CSQ");
    state = GSM_WAIT_SIGNAL;
    break;

  case GSM_WAIT_SIGNAL:

    if (responseContains("+CSQ:"))
    {
      int idx = responseBuffer.indexOf("+CSQ:");
      int comma = responseBuffer.indexOf(",", idx);

      if (comma > idx)
      {
        int rssi = responseBuffer.substring(idx + 5, comma).toInt();

        if (Serial)
        {
          Serial.print("Signal: ");
          Serial.println(rssi);
        }

        if (rssi >= MIN_CSQ)
        {
          state = GSM_SEND_AT;
        }
        else
        {
          state = GSM_FAILED;
        }
      }
    }
    else if (timeout(SIG_TIMEOUT))
    {
      state = GSM_FAILED;
    }

    break;

  case GSM_SEND_AT:

    sendCommand("AT");
    state = GSM_WAIT_AT;
    break;

  case GSM_WAIT_AT:

    if (responseContains("OK"))
    {
      state = GSM_SET_TEXT;
    }
    else if (timeout(CMD_TIMEOUT))
    {
      state = GSM_FAILED;
    }

    break;

  case GSM_SET_TEXT:

    sendCommand("AT+CMGF=1");
    state = GSM_WAIT_TEXT;
    break;

  case GSM_WAIT_TEXT:

    if (responseContains("OK"))
    {
      flushSIM800();

      sim800.print("AT+CMGS=\"");
      sim800.print(targetNumber);
      sim800.println("\"");

      responseBuffer = "";
      stateTimer = millis();
      state = GSM_WAIT_PROMPT;
    }
    else if (timeout(CMD_TIMEOUT))
    {
      state = GSM_FAILED;
    }

    break;

  case GSM_WAIT_PROMPT:

    if (responseContains(">"))
    {

      sim800.print(targetMessage);
      sim800.write(26);

      responseBuffer = "";
      stateTimer = millis();

      state = GSM_WAIT_SEND;
    }
    else if (timeout(CMD_TIMEOUT))
    {
      state = GSM_FAILED;
    }

    break;

  case GSM_WAIT_SEND:

    if (responseContains("+CMGS") && responseContains("OK"))
    {
      state = GSM_SUCCESS;
    }
    else if (timeout(20000))
    {
      state = GSM_FAILED;
    }

    break;

  case GSM_SUCCESS:

    if (Serial)
      Serial.println("[GSM] SMS SENT");

    state = GSM_IDLE;
    break;

  case GSM_FAILED:

    sendRetry++;

    if (sendRetry >= MAX_SEND_RETRIES)
    {
      if (Serial)
        Serial.println("[GSM] SMS FAILED");

      state = GSM_IDLE;
    }
    else
    {
      if (Serial)
        Serial.println("[GSM] RETRYING...");

      state = GSM_CHECK_SIM;
    }

    break;
  }
}


bool sendSMS(const String &number, const String &message)
{
  if (state != GSM_IDLE)
    return false;

  targetNumber = number;
  targetMessage = message;

  sendRetry = 0;
  netRetry = 0;

  state = GSM_CHECK_SIM;

  return true;
}