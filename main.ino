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

void setup() {
  Serial.begin(9600);
}

void loop() {

}
