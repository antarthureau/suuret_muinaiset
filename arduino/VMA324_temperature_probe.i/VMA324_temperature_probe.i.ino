#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 2

const unsigned long DELAY_TIME = 60000;
const int STATIC_REF = 0;
unsigned long prev = 0;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
 
void setup(void){
  Serial.begin(9600);
  sensors.begin();
}
 
 
void loop(void){
  if (millis() - prev >= DELAY_TIME){
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);

    Serial.print("temperature:");
    Serial.print(temp);
    Serial.print(",");
    Serial.print("ref:");
    Serial.println(STATIC_REF);
    prev = millis();
  }
}

