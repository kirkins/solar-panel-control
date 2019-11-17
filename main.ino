#include <OneWire.h>
#include <DallasTemperature.h>
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
Countimer redLightTimer;

int batteryState = 3;
float fanOnTemp       = 28.00;
float waterOffTemp       = 80.00;
double safeBatteryTempLow = 0.00;
double safeBatteryTempHigh = 40.00;
double safeCaseTempHigh = 45.00;
double safeWaterTempLow = 2.00;
double currentTemp, targetTemp;

int errorState = 0;
bool inverterTimerLock = false;
bool inverterTimerBlock = false;
bool inverterChanging = false;

bool inverterFaultTimerLock = false;
bool inverterFaultTimerBlock = false;

OneWire oneWire(tempSensors);
DallasTemperature sensors(&oneWire);

// 0 = water
// 1 = case
// 2 = battery

void setup() {
  Serial.begin(9600);
  sensors.begin();
  pinMode(fanControl, OUTPUT);
  pinMode(voltLoadDump, OUTPUT);
  pinMode(greenDrainGround, OUTPUT);
  pinMode(yellowDrainGround, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(inverterSwitch, OUTPUT);
  pinMode(voltOut1, OUTPUT);
  pinMode(voltOut2, OUTPUT);

  analogWrite(voltOut1, 1023);
  analogWrite(voltOut2, 1023);

  redLightTimer.setCounter(controlRedBlinking, 500);
}

void controlFan() {
  if(sensors.getTempCByIndex(1) > fanOnTemp || sensors.getTempCByIndex(2) > fanOnTemp) {
    digitalWrite(fanControl,HIGH);
  } else {
    digitalWrite(fanControl, LOW);
  }
  // Debug messages
  Serial.print("Water Temp Sensor: "); 
  Serial.println(sensors.getTempCByIndex(0));
  Serial.print("Case Temp Sensor: ");
  Serial.println(sensors.getTempCByIndex(1));
  Serial.print("Batt Temp Sensor: ");
  Serial.println(sensors.getTempCByIndex(2));
}

void controlWaterHeat() {
  if(sensors.getTempCByIndex(0) > waterOffTemp || batteryState < 3) {
    digitalWrite(voltLoadDump,LOW);
    digitalWrite(blueLED, LOW);
  } else {
    digitalWrite(voltLoadDump, HIGH);
    analogWrite(blueLED, 50);
  }
}

void externalGreenLight() {
  if(analogRead(readVoltsGreen) < 204.00) {
    digitalWrite(greenDrainGround, HIGH);
  } else {
    digitalWrite(greenDrainGround, LOW);
  }
}

void externalYellowLight() {
  if(analogRead(readVoltsYellow) < 204.00) {
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
  // Turn off inverter if error state is greater than 2 (meaning not cold water or cold battery error)
  // Turn on inverter if inverter is off and battlevel goes from Low to Normal
  if(!digitalRead(bmsActiveSignal) || batteryState < 2 || errorState > 2) {
    if(!inverterTimerLock) {
      inverterTimerLock = true;
      timer.setCounter(0, 0, 3, timer.COUNT_DOWN, confirmLowBatteryOrBMS);
    }
  } else if(inverterTimerLock) {
    inverterTimerBlock = true;
  }

  if(analogRead(readVoltsYellow) < 204) {
    if(!inverterFaultTimerLock) {
      inverterFaultTimerLock = true;
      timer.setCounter(0, 0, 5, timer.COUNT_DOWN, confirmInverterFault);
    }
  } else if(inverterFaultTimerBlock) {
    inverterFaultTimerBlock = true;
  }
}

void confirmLowBatteryOrBMS() {
  Serial.println("I ran 1");
  if(!digitalRead(bmsActiveSignal) || batteryState < 2) {
    Serial.println("I ran 2");
    if(!inverterTimerBlock && analogRead(readVoltsGreen) > 204) {
      Serial.println("I ran 3");
      inverterChanging = true;
      timer.setCounter(0, 0, 2, timer.COUNT_DOWN, stopInverterChanging);
    }
  }
  inverterTimerBlock = false;
  inverterTimerLock = false;
}

void confirmInverterFault() {
  if(analogRead(readVoltsYellow) < 204 && !inverterFaultTimerBlock) {
    inverterChanging = true;
    timer.setCounter(0, 0, 2, timer.COUNT_DOWN, stopInverterChanging);
  }
  inverterFaultTimerBlock = false;
  inverterFaultTimerLock = false;
}

void setBatteryState() {
  double batteryVoltage = (5*(analogRead(batteryLevel)/1023.00))*1.016;
  // Serial.print("Battery Level:    ");
  // Serial.println(analogRead(batteryLevel));
  Serial.print("Battery Voltage:    ");
  Serial.println(batteryVoltage);
  if(batteryVoltage < 2.6) {
    batteryState = 0; // battErrorLOW
  } else if(batteryVoltage <2.9) {
    batteryState = 1; // battLOW
  } else if(batteryVoltage < 3.5) {
    batteryState = 2; // battNORMAL
  } else if(batteryVoltage < 3.65) {
    batteryState = 3; // battHIGH
  } else {
    batteryState = 4; // battErrorHIGH
  }
  Serial.print("Battery State:    ");
  Serial.println(batteryState);
}

void changeInverterState() {
  if(inverterChanging) {
    digitalWrite(inverterSwitch, HIGH);
  }
}

void stopInverterChanging() {
  inverterChanging = false;
}

void controlLightingLoad(){
  if(batteryState < 2) {
    digitalWrite(lightingLoad, LOW);
  } else {
    digitalWrite(lightingLoad, HIGH);
  }
}

void getErrorState(){
  // Error Types:
  // 0 - No error
  // 1 - Battery too cold
  // 2 - Water too cold
  // 3 - Battery or case too hot
  // 4 - Battery low error
  // 5 - Battery high error

  if(sensors.getTempCByIndex(2) < safeBatteryTempLow) {
    errorState = 1;
  } else if(sensors.getTempCByIndex(0) < safeWaterTempLow) {
    errorState = 2;
  } else if(sensors.getTempCByIndex(2) > safeBatteryTempHigh || sensors.getTempCByIndex(1) > safeCaseTempHigh) {
    errorState = 3;
  } else if(batteryState == 0) {
    errorState = 4;
  } else if(batteryState == 4) {
    errorState = 5;
  } else {
    errorState = 0;
  }

}

void controlRedBlinking() {
  if(errorState==0) {
    digitalWrite(greenLED, HIGH);
    digitalWrite(redLED, LOW);
  } else if(errorState==1) {
    digitalWrite(greenLED, LOW);
    digitalWrite(redLED, HIGH);
  }
}

void printTests(){

  //INPUTS//

  //readVoltsGreen  //  A0
  //inverter is off when above 1V
  Serial.print("readVoltsGreen:  ");
  Serial.println(5.00*(analogRead(readVoltsGreen)/1023.00));

  //readVoltsYellow  // A1
  //inverter is in FAULT when below 1V
  Serial.print("readVoltsYellow:  ");
  Serial.println(5.00*(analogRead(readVoltsYellow)/1023.00));

  //batteryLevel  //  A2
  //Battery Voltage prints in setBatteryState function//

  //inverterButton // A5
  //button is being pushed when above 1V
  Serial.print("inverterButton:  ");
  Serial.println(5.00*(analogRead(inverterButton)/1023.00));

  //bmsActiveSignal //  3
  //true when BMS is OK
  Serial.print("bmsActiveSignal:  ");
  Serial.println(digitalRead(bmsActiveSignal));

  // Error State
  Serial.print("Error State:  ");
  Serial.println(errorState);

  //tempSensors //  2
  //tempSensors prints in controlFan function//
}

void loop() {
  sensors.requestTemperatures();
  setBatteryState();
  getErrorState();
  turnOffInverter(); // only if needed
  controlFan();
  controlWaterHeat();
  externalGreenLight();
  externalYellowLight();
  changeInverterState();
  controlLightingLoad();
  checkButton();
  printTests();
}
