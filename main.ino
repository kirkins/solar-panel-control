#include <OneWire.h>
#include <DallasTemperature.h>
#include <PID_v1.h>
#include <Countimer.h>

//INPUTS
#define readVoltsGreen A0 // if above 1V true (read as 204)
#define readVoltsYellow A1 // if above 1V true (read as 204)
#define batteryLevel A2 // check voltage directly from battery
#define inverterButton A5 // if above 1v being pushed
#define bmsActiveSignal 3  // will read TRUE when BMS active
#define tempSensors 2 // serial digital inputs

//OUTPUTS
#define voltOut1 A3  //sends 5v continuously
#define voltOut2 A4  //sends 5v continuously
#define greenDrainGround 13  // external LED
#define yellowDrainGround 12 // external LED
#define fanControl 11
#define voltLoadDump 10 // 24 volts to water heating element
#define lightingLoad 9 // 12 volt loads
#define greenLED 7
#define blueLED 6
#define redLED 5
#define inverterSwitch 4

Countimer timer;

bool batteryState = 3;
float fanOnTemp       = 32.00;
float waterOffTemp       = 80.00;
double currentTemp, pidOutput, targetTemp;
bool inverterTimerLock = false;
bool inverterTimerBlock = false;
bool inverterChanging = false;

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
  pinMode(inverterSwitch, OUTPUT);
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

void externalGreenLight() {
  if(analogRead(readVoltsGreen) > 204.00) {
    digitalWrite(greenDrainGround, HIGH);
  } else {
    digitalWrite(greenDrainGround, LOW);
  }
}

void externalYellowLight() {
  if(analogRead(readVoltsYellow) > 204.00) {
    digitalWrite(yellowDrainGround, HIGH);
  } else {
    digitalWrite(yellowDrainGround, LOW);
  }
}

void checkButton() {
  if(analogRead(inverterButton) > 204.00) {
    digitalWrite(inverterSwitch, HIGH);
  } else {
    digitalWrite(inverterSwitch, LOW);
  }
}

void turnOffInverter() {
  // turn off inverter if BMS not True
  // turn off inverter if battLevel Low
  // Turn off inverter if fault for 5 seconds
  // Turn on inverter if inverter is off and battlevel goes from Low to Normal
  if(!digitalRead(bmsActiveSignal) || batteryState < 2) {
    if(!inverterTimerLock) {
      inverterTimerLock = true;
      timer.setCounter(0, 0, 3, timer.COUNT_DOWN, confirmInverterShutdown);
    }
  } else if(inverterTimerLock) {
    inverterTimerBlock = true;
  }
}

void confirmInverterShutdown() {
  if(!digitalRead(bmsActiveSignal) || batteryState < 2) {
    if(!inverterTimerBlock && analogRead(readVoltsGreen) > 204) {
      inverterChanging = true;
      timer.setCounter(0, 0, 2, timer.COUNT_DOWN, stopInverterChanging);
    }
  }
  inverterTimerBlock = false;
  inverterTimerLock = false;
}

void setBatteryState() {
  double batteryVoltage = 5*(analogRead(batteryLevel)/1023)
  if(batteryVoltage < 2.6) {
    batteryState = 0; // battErrorLOW
  } else if(batteryVoltage < 2.9) {
    batteryState = 1; // battLOW
  } else if(batteryVoltage < 3.5) {
    batteryState = 2; // battNORMAL
  } else if(batteryVoltage <3.65) {
    batteryState = 3; // battHIGH
  } else if(batteryVoltage > 3.65) {
    batteryState = 4; // battErrorHIGH
  }
}

void changeInverterState() {
  if(inverterChanging) {
    digitalWrite(inverterSwitch, HIGH);
  }
}

void stopInverterChanging() {
  inverterChanging = false;
}

void loop() {
  sensors.requestTemperatures();
  setBatteryState();
  turnOffInverter(); // only if needed
  controlFan();
  controlWaterHeat();
  externalGreenLight();
  externalYellowLight();
  changeInverterState();
}
