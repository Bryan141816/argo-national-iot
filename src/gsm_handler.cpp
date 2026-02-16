#include <gsm_handler.h>
#include <SoftwareSerial.h>

SoftwareSerial sim800(10, 11); // SIM800 TX -> D10, SIM800 RX -> D11


void bridgeSerial()
{
  while (Serial.available()) sim800.write(Serial.read());
  while (sim800.available()) Serial.write(sim800.read());
}

bool waitForResponse(const char* expected, unsigned long timeoutMs)
{
  unsigned long start = millis();
  String buf;

  while (millis() - start < timeoutMs)
  {
    while (sim800.available())
    {
      char c = sim800.read();
      buf += c;
      Serial.write(c);

      if (buf.indexOf(expected) != -1) return true;
      if (buf.indexOf("ERROR") != -1) return false;
    }
  }
  return false;
}

bool sendSMS(const String& number, const String& text)
{
  if (number.length() < 8 || text.length() == 0) return false;

  Serial.println("\n--- Sending SMS ---");

  sim800.println("AT");
  if (!waitForResponse("OK", 2000)) return false;

  sim800.println("AT+CMGF=1");
  if (!waitForResponse("OK", 2000)) return false;

  sim800.print("AT+CMGS=\"");
  sim800.print(number);
  sim800.println("\"");
  if (!waitForResponse(">", 5000)) return false;

  sim800.print(text);
  sim800.write(26); // Ctrl+Z

  if (!waitForResponse("+CMGS", 15000)) return false;
  if (!waitForResponse("OK", 15000)) return false;

  Serial.println("\n--- SMS Sent ---");
  return true;
}

void gsmSetup()
{
  Serial.begin(9600);
  sim800.begin(9600);
  delay(1500);

  Serial.println("SIM800L startup check...");

  sim800.println("AT");
  waitForResponse("OK", 2000);

  sim800.println("AT+CSQ");
  waitForResponse("OK", 2000);

  sim800.println("AT+CREG?");
  waitForResponse("OK", 2000);

  // if (!sendSMS(phoneNumber, smsMessage))
  // {
  //   Serial.println("\nSMS FAILED (power/signal/SIM/wiring).");
  // }

  Serial.println("\nSerial bridge active.");
}

void gsmLoop()
{
  bridgeSerial();
}
