#include <OneWire.h>
#include <DallasTemperature.h>
#include <PID_v1.h>

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

float fanOnTemp       = 25.00;
float waterOffTemp       = 80.00;
double currentTemp, pidOutput, targetTemp;

PID heatingPID(&currentTemp, &pidOutput, &targetTemp, 0.5, 7, 1, DIRECT);

OneWire oneWire(tempSensors);
DallasTemperature sensors(&oneWire);

// temp1 = case
// temp2 = battery
// temp3 = water
DeviceAddress temp1, temp2, temp3;

void setup() {
  Serial.begin(9600);
  sensors.begin();
  heatingPID.SetMode(AUTOMATIC);
}

void controlFan() {
  if(sensors.getTempCByIndex(0) > fanOnTemp || sensors.getTempCByIndex(1) > fanOnTemp) {
    digitalWrite(fanControl,HIGH);
  } else {
    digitalWrite(fanControl, LOW);
  }
}

void controlWaterHeat() {
  heatingPID.SetOutputLimits(0, 255);
  heatingPID.Compute();
  if(sensors.getTempCByIndex(0) > waterOffTemp) {
    digitalWrite(voltLoadDump,HIGH);
  } else {
    digitalWrite(voltLoadDump, LOW);
  }
}

void loop() {
  sensors.requestTemperatures();
  Serial.print("temp 1 = ");
  Serial.println(sensors.getTempCByIndex(0));
  Serial.print("temp 2 = ");
  Serial.println(sensors.getTempCByIndex(1));
  Serial.print("temp 3 = ");
  Serial.println(sensors.getTempCByIndex(2));
  controlFan();
  constrolWaterHeat();
}
