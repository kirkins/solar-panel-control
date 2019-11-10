#include <OneWire.h>
#include <DallasTemperature.h>
#include <PID_v1.h>

#define readVoltsGreen A0 // if above 1V true (read as 204)
#define readVoltsYellow A1 // if above 1V true (read as 204)
#define batteryVoltage A2 // check
#define inverter A3
#define inverterFault A4
#define inverterButton A5 // if above 2v being push (208, but will check for 204)

#define greenDrainGround 13  // external LED
#define yellowDrainGround 12 // external LED
#define fanControl 11
#define voltLoadDump 10 // 24 volts to water heating element
#define lightingLoad 9 // 12 volt loads

#define greenLED 7
#define blueLED 6
#define redLED 5
#define inverterSwitch 4
#define bmsActiveSignal 3
#define tempSensors 2

float fanOnTemp       = 32.00;
float waterOffTemp       = 80.00;
double currentTemp, pidOutput, targetTemp;

PID heatingPID(&currentTemp, &pidOutput, &targetTemp, 0.5, 7, 1, DIRECT);

OneWire oneWire(tempSensors);
DallasTemperature sensors(&oneWire);

// temp1 = water
// temp2 = case
// temp3 = battery
DeviceAddress temp1, temp2, temp3;

void setup() {
  Serial.begin(9600);
  sensors.begin();
  heatingPID.SetMode(AUTOMATIC);
  pinMode(fanControl, OUTPUT);
  pinMode(voltLoadDump, OUTPUT);
  pinMode(greenDrainGround, OUTPUT);
  pinMode(yellowDrainGround, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(redLED, OUTPUT);
}

void controlFan() {
  if(sensors.getTempCByIndex(1) > fanOnTemp || sensors.getTempCByIndex(2) > fanOnTemp) {
    digitalWrite(fanControl,HIGH);
  } else {
    digitalWrite(fanControl, LOW);
  }
}

void controlWaterHeat() {
  heatingPID.SetOutputLimits(0, 255);
  heatingPID.Compute();
  if(sensors.getTempCByIndex(0) > waterOffTemp) {
    digitalWrite(voltLoadDump,LOW);
  } else {
    digitalWrite(voltLoadDump, HIGH);
  }
}

void redLightStatus() {
  int batteryLevel = analogRead(batteryVoltage);
  Serial.print("Batter level = ");
  Serial.println(batteryLevel);
}

void externalGreenLight() {
  if(analogRead(readVoltsGreen) > 204) {
    digitalWrite(greenDrainGround, HIGH);
  } else {
    digitalWrite(greenDrainGround, LOW);
  }
}

void externalYellowLight() {
  if(analogRead(readVoltsYellow) > 204) {
    digitalWrite(yellowDrainGround, HIGH);
  } else {
    digitalWrite(yellowDrainGround, LOW);
  }
}

void checkButton() {
  if(analogRead(inverterButton) > 204) {
    digitalWrite(inverterSwitch, HIGH);
  } else {
    digitalWrite(inverterSwitch, LOW);
  }
}

void loop() {

  analogWrite(inverter, 255);
  analogWrite(inverterFault, 255);

  sensors.requestTemperatures();
  controlFan();
  controlWaterHeat();
  redLightStatus();
  externalGreenLight();
  externalYellowLight();
}
