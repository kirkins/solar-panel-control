#include <OneWire.h>
#include <DallasTemperature.h>

#define readVoltsGreen A0
#define readVoltsYellow A1
#define inverter A3
#define inverterFault A4
#define batteryVoltage A5

#define greenDrainGround 13
#define yellowDrainGround 12
#define fanControl 11
#define voltLoadDump 10
#define voltLoads 9

#define greenLED 7
#define blueLED 6
#define redLED 5
#define inverterSwitch 4
#define bmsActiveSignal 3
#define tempSensors 2

OneWire oneWire(tempSensors);
DallasTemperature sensors(&oneWire);

DeviceAddress temp1, temp2, temp3;

void setup() {
  Serial.begin(9600);
  sensors.begin();

  // locate devices on the bus
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // search for devices on the bus and assign based on an index.
  if (!sensors.getAddress(temp1, 0)) Serial.println("Can't find temp1"); 
  if (!sensors.getAddress(temp2, 1)) Serial.println("Can't find temp2"); 
  if (!sensors.getAddress(temp3, 1)) Serial.println("Can't find temp3"); 

  sensors.setHighAlarmTemp(insideThermometer, 25);

}

void checkAlarm(DeviceAddress deviceAddress)
{
  if (sensors.hasAlarm(deviceAddress))
  {
    Serial.print("ALARM: ");
    printData(deviceAddress);
  }
}

void loop() {
  checkAlarm(temp1);
  checkAlarm(temp2);
  checkAlarm(temp3);
}
