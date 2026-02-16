#include <dht_handler.h>
#include <DHT.h>

#define DHT11_PIN 7
#define DHTTYPE DHT11

static DHT dht(DHT11_PIN, DHTTYPE);

static float lastTempC = 0.0;
static float lastHumidity = 0.0;

void dhtSetup()
{
    dht.begin();
}

void dhtLoop()
{
    lastHumidity = dht.readHumidity();
    lastTempC = dht.readTemperature();
}

float getTemperatureC()
{
    return lastTempC;
}

float getHumidity()
{
    return lastHumidity;
}
