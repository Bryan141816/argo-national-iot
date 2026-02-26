#include <dht_handler.h>
#include <DHT.h>

#define DHT11_PIN 2
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
    float humidity = dht.readHumidity();
    float tempC = dht.readTemperature();

    if (!isnan(humidity)) {
        lastHumidity = humidity;
    }

    if (!isnan(tempC)) {
        lastTempC = tempC;
    }
}


float getTemperatureC()
{
    return lastTempC;
}

float getHumidity()
{
    return lastHumidity;
}
