#include <OneWire.h>
#include <DallasTemperature.h>
#include <Countimer.h>
#include <PID_v1.h>

//INPUTS
#define readVoltsGreen A0 // if above 1V true (read as 204)
#define readVoltsYellow A1 // if above 1V true (read as 204)
#define batteryLevel A2 // check voltage directly from battery
#define inverterButton A5 // if above 1v being pushed
#define bmsActiveSignal 8  // will read TRUE when BMS active
#define batteryHeater 3  // will read TRUE when BMS active
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

Countimer batteryLowTimer;
Countimer redLightTimer;
Countimer faultTimer;
Countimer inverterChangingTimer;

int batteryState = 3;
float fanOnTemp       = 28.00;
float waterOffTemp       = 90.00;
double safeBatteryTempLow = -2.00;
double safeBatteryTempHigh = 40.00;
double safeCaseTempHigh = 45.00;
double safeWaterTempLow = 2.00;
double currentTemp, targetTemp, averageBatteryVoltage;

double averageBatteryTemp = 10.0;

int errorState = 0;
bool inverterTimerLock = false;
bool inverterTimerBlock = false;
bool inverterChanging = false;

bool inverterFaultTimerLock = false;
bool inverterFaultTimerBlock = false;

bool firstTimeOn = true;

int loopRun = 0;
int voltageHistorySize;
double voltageHistory[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
int batteryTempHistorySize;
double batteryTempHistory[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
double loadOutput;
double batteryVoltage = 3.2;

double batteryCase2Limit = 3.26;  
double targetVoltage = batteryCase2Limit + 0.15;

double targetBatteryTemp = 1.0;
double batteryTempOutput;


OneWire oneWire(tempSensors);
DallasTemperature sensors(&oneWire);

PID loadOutputPID(&batteryVoltage, &loadOutput, &targetVoltage, 4, 1000, 1, REVERSE);
PID batteryTempPID(&averageBatteryTemp, &batteryTempOutput, &targetBatteryTemp, 0.2, 1, 0.2, DIRECT);

// 0 = water
// 1 = case
// 2 = battery

void setup() {
  // Set PWM frequency for pin 10 to 30hz
  TCCR1B = TCCR1B & B11111000 | B00000101;

  // get size of history array
  voltageHistorySize = sizeof(voltageHistory) / sizeof(voltageHistory[0]);
  batteryTempHistorySize = sizeof(batteryTempHistory) / sizeof(batteryTempHistory[0]);

  loadOutputPID.SetMode(AUTOMATIC);
  batteryTempPID.SetMode(AUTOMATIC);
  Serial.begin(9600);
  sensors.begin();
  pinMode(fanControl, OUTPUT);
  pinMode(bmsActiveSignal, OUTPUT);
  pinMode(voltLoadDump, OUTPUT);
  pinMode(batteryHeater, OUTPUT);
  pinMode(greenDrainGround, OUTPUT);
  pinMode(yellowDrainGround, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(inverterSwitch, OUTPUT);
  pinMode(voltOut1, OUTPUT);
  pinMode(voltOut2, OUTPUT);
  pinMode(lightingLoad, OUTPUT);

  analogWrite(voltOut1, 1023);
  analogWrite(voltOut2, 1023);

  redLightTimer.setInterval(controlRedBlinking, 500);
  digitalWrite(greenLED, HIGH);

  batteryLowTimer.setCounter(0, 0, 3, batteryLowTimer.COUNT_DOWN, confirmLowBatteryOrBMS);
  inverterChangingTimer.setCounter(0, 0, 2, inverterChangingTimer.COUNT_DOWN, stopInverterChanging);

  batteryLowTimer.setInterval(refreshClock, 1000);
  inverterChangingTimer.setInterval(refreshClock, 1000);
}

float average (double * array, int len) {
  double sum = 0;
  int total = len;
  for (int i = 0 ; i < len ; i++) {
    if(array[i] != -1) {
      sum += array [i] ;
    } else {
      total--;
    }
  }
  return  ((float) sum) / ((float) total);
}

void refreshClock() {
  //Serial.print("Current count time is: ");
  //Serial.println(batteryLowTimer.getCurrentTime());
}

void controlFan() {
  Serial.print("Water Temp:    ");
  Serial.println(sensors.getTempCByIndex(0));
  Serial.print("Case Temp:    ");
  Serial.println(sensors.getTempCByIndex(1));
  Serial.print("Average Batt Temp:    ");
  Serial.println(averageBatteryTemp);

  if(sensors.getTempCByIndex(1) > fanOnTemp || averageBatteryTemp > fanOnTemp) {
    digitalWrite(fanControl,HIGH);
  } else {
    digitalWrite(fanControl, LOW);
  }
}

void controlWaterHeat() {
  loadOutputPID.SetOutputLimits(0, 255);
  loadOutputPID.Compute();
  if(sensors.getTempCByIndex(0) > waterOffTemp || batteryState < 3) {
    digitalWrite(voltLoadDump,LOW);
    digitalWrite(blueLED, LOW);
  } else {
    analogWrite(voltLoadDump, loadOutput);
    analogWrite(blueLED, 30);
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
  if(batteryState < 2 || errorState == 3 || errorState == 4) {
    if(!inverterTimerLock) {
      inverterTimerLock = true;
      Serial.println("Setting confirm battery low timer");
    }
  } else if(inverterTimerLock) {
    Serial.print("CANCELLING TURN OFF INVERTER");
    Serial.println();
    inverterTimerBlock = true;
  }

  if(analogRead(readVoltsYellow) < 204) {
    if(!inverterFaultTimerLock) {
      inverterFaultTimerLock = true;
      faultTimer.setCounter(0, 0, 5, faultTimer.COUNT_DOWN, confirmInverterFault);
    }
  } else if(inverterFaultTimerLock) {
    inverterFaultTimerBlock = true;
  }
}

void confirmLowBatteryOrBMS() {
  Serial.println("I ran 1");
  if(batteryState < 2) {
    Serial.println("I ran 2");
    delay(5000);
    if(!inverterTimerBlock && (analogRead(readVoltsGreen) < 204 || analogRead(readVoltsYellow) < 204)) {
      Serial.println("I ran 3");
      inverterChanging = true;
    }
  }
  inverterTimerBlock = false;
  inverterTimerLock = false;
}

void confirmInverterFault() {
  if(analogRead(readVoltsYellow) < 204 && !inverterFaultTimerBlock) {
    inverterChanging = true;
  }
  inverterFaultTimerBlock = false;
  inverterFaultTimerLock = false;
}

void setBatteryState() {

  analogRead(batteryLevel);
  delayMicroseconds(100);
  batteryVoltage = (5.000*(analogRead(batteryLevel)/1023.00))*1.3810;

  // Battery history
  for (int i = voltageHistorySize; i > 0; i--){
    voltageHistory[i]=voltageHistory[i-1];
  }
  voltageHistory[0] = batteryVoltage;

  // Get average battery voltage
  averageBatteryVoltage = average(voltageHistory, voltageHistorySize);

  if(loopRun == 19) {
    Serial.print("Average Battery Voltage:    ");
    Serial.println(averageBatteryVoltage);
    Serial.print("Average Battery Voltage:    ");
    Serial.println(averageBatteryVoltage, 4);

    for(int i = 0; i < voltageHistorySize; i++) {
      Serial.println(voltageHistory[i]);
    }
  }


  if(averageBatteryVoltage < 1.5) {
    batteryState = 0; // battErrorLOW
  } else if(averageBatteryVoltage <2.75) {
    batteryState = 1; // battLOW
  } else if(averageBatteryVoltage < batteryCase2Limit) {
    batteryState = 2; // battNORMAL
  } else if(averageBatteryVoltage < 4.5) {
    batteryState = 3; // battHIGH
  } else {
    batteryState = 4; // battErrorHIGH
  }
}

void changeInverterState() {
  if(inverterChanging) {
    Serial.println("I ran 4");
    digitalWrite(inverterSwitch, HIGH);
  } else {
    digitalWrite(inverterSwitch, LOW);
  }
}

void stopInverterChanging() {
  Serial.println("I ran 5");
  inverterChanging = false;
}

void controlLightingLoad(){

  if(averageBatteryVoltage < 2.6){
    digitalWrite(lightingLoad, LOW);
  }else if(averageBatteryVoltage > 3.00){
    digitalWrite(lightingLoad, HIGH);
  }else if(firstTimeOn){
    digitalWrite(lightingLoad,HIGH);
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

  if(averageBatteryTemp < safeBatteryTempLow) {
    errorState = 1;
  } else if(sensors.getTempCByIndex(0) < safeWaterTempLow) {
    errorState = 2;
  } else if(averageBatteryTemp > safeBatteryTempHigh || sensors.getTempCByIndex(1) > safeCaseTempHigh) {
    errorState = 3;
  } else if(!batteryState) {
    errorState = 4;
  } else if(batteryState == 4) {
    errorState = 5;
  } else {
    errorState = 0;
  }

}

void controlRedBlinking() {
  bool redOn;
  bool greenOn = true;

  if(errorState==0) {
    redOn=false;
    greenOn=true;
  } else {
    redOn=true;
    greenOn=false;
  }

  if(redOn) {
    digitalWrite(greenLED, LOW);
    digitalWrite(redLED, HIGH);
  } else if(greenOn) {
    digitalWrite(greenLED, HIGH);
    digitalWrite(redLED, LOW);
  }

}

void controlBatteryTemp() {

  double tempBatteryTemp = sensors.getTempCByIndex(2);

  if(tempBatteryTemp > -50) {

    // Battery Temp history
    for (int i = batteryTempHistorySize; i > 0; i--){
      batteryTempHistory[i]=batteryTempHistory[i-1];
    }

    batteryTempHistory[0] = tempBatteryTemp;

    // Get average Battery Temp
    averageBatteryTemp = average(batteryTempHistory, batteryTempHistorySize);

  }

  Serial.print("Battery Temp PID Output:    ");
  Serial.println(batteryTempOutput);


  if(batteryState > 1) {
    batteryTempPID.SetOutputLimits(0, 150);
    batteryTempPID.Compute();
    analogWrite(batteryHeater, batteryTempOutput);
  } else {
    analogWrite(batteryHeater, LOW);
  }
}

void printTests(){

  //INPUTS//

  Serial.print("Battery State:    ");
  Serial.println(batteryState);

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
  //Serial.print("bmsActiveSignal:  ");
  //Serial.println(digitalRead(bmsActiveSignal));

  // Error State
  Serial.print("Error State:  ");
  Serial.println(errorState);

  //tempSensors //  2
  //tempSensors prints in controlFan function//
}

void loop() {

  redLightTimer.run();
  if(!redLightTimer.isCounterCompleted()) {
    redLightTimer.start();
  }

  faultTimer.run();
  if(!faultTimer.isCounterCompleted()) {
    faultTimer.start();
  }

  inverterChangingTimer.run();
  if(inverterChanging) {
    inverterChangingTimer.start();
  }


  batteryLowTimer.run();
  if(inverterTimerLock) {
    batteryLowTimer.start();
  }

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


  if(!inverterChanging) {
    checkButton();
  }

  // Print logs only every 20 runs
  loopRun++;
  if(loopRun > 19) {
    printTests();
    loopRun = 0;
  }
  firstTimeOn = false;
}
