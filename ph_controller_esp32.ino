/*

 * Serial Commands:
 *   0    - enterph          -> enter the calibration mode  (long click on SET)
 *   1      - calph          -> calibrate with the standard buffer solution, two buffer solutions(4.0 and 7.0) will be automaticlly recognized  (one click on SET)
 *   1/2    - exitph         -> save the calibrated parameters and exit from calibration mode  (long click on SET)
 *   1      - 1gp            -> Pump settings view (one click on DOWN) 
 *   9         - frate       -> enter pump flow rate window
 *   13            - pfrate  -> Increase pump flow rate (one click on UP)
 *   13            - mfrate  -> Decrease pump flow rate (one click on DOWN)
 *   13            - sfrate  -> Save pump flow rate (one click on SET)
 *   9      - 2gp
 *   10         - amnt       -> enter pump amount window
 *   14            - pamnt   -> Increase pump amount (one click on UP)
 *   14            - mamnt   -> Decrease pump amount (one click on DOWN)
 *   14            - samnt   -> Save pump amount (one click on SET)
 *   10     - 3gp
 *   11         - wtime      -> enter pump wait time window
 *   15            - pwtime  -> Increase pump  wait time (one click on UP)
 *   15            - mwtime  -> Decrease pump  wait time (one click on DOWN)
 *   15            - swtime  -> Save pump  wait time (one click on SET)
 *   11     - 4gp
 *   12         - pcal       -> enter pump calibration window
 *   16         - pcal2      -> continue pump calibration window
 *   17            - pstart  -> Start pump calibration (one click on SET)
 *   18            - pcalw   -> enter select flow rate window (one click on SET)
 *   19            - pcalp   -> Increase cal pump flow rate  (one click on UP)
 *   19            - pcalm   -> Decrease cal pump flow rate (one click on DOWN)
 *   19            - pcals   -> Save pump flow rate (one click on SET)
 *   12     - 5gp
 *   20         - s5pg       -> Confirm pure 1 ml test
 *   0    - target      -> Open target pH window (one click on SET)
 *   4      - mt        -> Decrease pH target (one click on DOWN)
 *   4      - pt        -> Increase pH target (one click on UP)
 *   4      - st        -> Save pH target (long click on SET)
 *   0    - tt          -> Change Temp C/F (one click on UP) 
 * 
 */

#include "DFRobot_PH.h"
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ezButton.h>
#include <string.h>
#include "GravityPump.h"
#include <Adafruit_ADS1X15.h>

#define ONE_WIRE_BUS 4
#define PH_PIN 34
#define UP_PIN 19
#define SET_PIN 5
#define DOWN_PIN 18
#define PUMP_PIN 16
#define PUMP_MOMENTARY 0.1
#define ESPADC 4095.0   //the esp Analog Digital Convertion value
#define ESPVOLTAGE 3300 //the esp voltage supply value
#define WAIT_BETWEEN_DOSE 0.17

float voltage,phValue,temperature = 25;
DFRobot_PH ph;
GravityPump pump;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
 Adafruit_ADS1115 ads;


const int SHORT_PRESS_TIME = 1000; // 1000 milliseconds
const int LONG_PRESS_TIME  = 2000; // 1000 milliseconds
ezButton setButton(SET_PIN); 
ezButton upButton(UP_PIN); 
ezButton downButton(DOWN_PIN); 
unsigned long pressedTimeSet  = 0;
unsigned long releasedTimeSet = 0;
unsigned long pressedTimeUp  = 0;
unsigned long releasedTimeUp = 0;
unsigned long pressedTimeDown  = 0;
unsigned long releasedTimeDown = 0;
bool isPressingSet = false;
bool isPressingUp = false;
bool isPressingDown = false;
bool isLongDetected = false;
int cmdType = 0; 
char cmd[10];
float pump_amount = 1.0;
float pump_wait = 60.0;
float target_ph = 6.3;
float phBuff = 0.1;
int isF = 0; 
bool first_run = true;
bool isDosing = false;

#define PHVALUEADDR 0x00

float readFloatFromEEPROM(int address) {
    union {
        byte b[4];
        float f;
    } u;

    // Read 4 bytes from EEPROM
    for (int i = 0; i < 4; i++) {
        u.b[i] = EEPROM.read(PHVALUEADDR+ address + i);
    }
    return u.f;
}


void setup()
{
    Serial.begin(115200); 
    ads.setGain(GAIN_TWOTHIRDS); 
    ads.begin();
    ads.startComparator_SingleEnded(0, 1000);
    EEPROM.begin(512);
    pump.setPin(PUMP_PIN);
    setButton.setDebounceTime(50);
    upButton.setDebounceTime(50);
    downButton.setDebounceTime(20);
    ph.begin();
    pump.getFlowRateAndSpeed();
    target_ph = readFloatFromEEPROM(8);
    isF = readFloatFromEEPROM(12);
    pump_amount = readFloatFromEEPROM(16);
    pump_wait = readFloatFromEEPROM(20);
    phBuff = readFloatFromEEPROM(40);
    
    
}

void loop()
{
    pump.update();
    setButton.loop(); // MUST call the loop() function first
    upButton.loop();
    downButton.loop();
    if(setButton.isPressed()){
      pressedTimeSet = millis();
      isPressingSet = true;
      isLongDetected = false;
    } else if(upButton.isPressed()){
      pressedTimeUp = millis();
      isPressingUp = true;
      isLongDetected = false;
    } else if(downButton.isPressed()){
      pressedTimeDown = millis();
      isPressingDown = true;
      isLongDetected = false;
    }

    if(setButton.isReleased()) {
      isPressingSet = false;
      releasedTimeSet = millis();

      long pressDuration = releasedTimeSet - pressedTimeSet;

      if( pressDuration < SHORT_PRESS_TIME ) {
        //Serial.println(F("A short press is detected"));
        if(cmdType == 1) {
          char cmd[] = "calph";
          cmdType=2;
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 0) {
          char cmd[] = "target";
          cmdType=4;
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 9) {
          char cmd[] = "frate";
          cmdType=13;
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 10) {
          char cmd[] = "amnt";
          cmdType=14;
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 13) {
          char cmd[] = "sfrate";
          cmdType=9;
          ph.calibration(voltage,temperature,cmd);
          pump.getFlowRateAndSpeed();
          strcpy(cmd, "1gp");
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 14) {
          char cmd[] = "samnt";
          cmdType=10;
          ph.calibration(voltage,temperature,cmd);
          pump_amount = readFloatFromEEPROM(16);
          strcpy(cmd, "2gp");
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 11) {
          char cmd[] = "wtime";
          cmdType=15;
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 15) {
          char cmd[] = "swtime";
          cmdType=11;
          ph.calibration(voltage,temperature,cmd);
          pump_wait = readFloatFromEEPROM(20);
          strcpy(cmd, "3gp");
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 12) {
          char cmd[] = "pcal";
          cmdType=16;
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 16) {
          char cmd[] = "pcal2";
          cmdType=17;
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 17) {
          char cmd[] = "pstart";
          cmdType=18;
          ph.calibration(voltage,temperature,cmd);
          //pump.pumpCalibration(1);
          pump.timerPump(15);
          //delay(16000);
          char cmd2[] = "pcalw";
          cmdType=19;
          ph.calibration(voltage,temperature,cmd2);
        } else if(cmdType == 19) {
          char cmd[] = "pcals";
          cmdType=9;
          ph.calibration(voltage,temperature,cmd);
          pump.pumpCalibration(3);
          pump.getFlowRateAndSpeed();
          char cmd2[] = "4gp";
          ph.calibration(voltage,temperature,cmd2);
        } else if(cmdType == 20) {
           char cmd[] = "s5gp";
           cmdType=21;
           ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 21) {
           pump.flowPump(2.0);
           cmdType=20;
           char cmd2[] = "5gp";
           ph.calibration(voltage,temperature,cmd2);
        } else if(cmdType == 20) {
          char cmd[] = "6gp";
          cmdType=23;
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 23) {
          char cmd[] = "buff";
          cmdType=24;
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 24) {
            char cmd[] = "sbuff";
            cmdType = 23;
            ph.calibration(voltage,temperature,cmd);
            char cmd2[] = "6gp";
            ph.calibration(voltage,temperature,cmd2);
            phBuff = readFloatFromEEPROM(40);
         }
      }
    }

    if(upButton.isReleased()) {
      isPressingUp = false;
      releasedTimeUp = millis();

      long pressDuration = releasedTimeUp - pressedTimeUp;

      if( pressDuration < SHORT_PRESS_TIME ) {
          if(cmdType == 0){
            if(isF == 1) {
              isF=0;
            } else {
              isF=1;
            }
            strcpy(cmd, "tt");
            ph.calibration(voltage,temperature,cmd);
            first_run = true;
          } else if(cmdType == 4){
            strcpy(cmd, "pt");
            ph.calibration(voltage,temperature,cmd);
          } else if(cmdType == 9){
            strcpy(cmd, "enterph");
            cmdType=1;
            ph.calibration(voltage,temperature,cmd);
          } else if(cmdType == 10){
            strcpy(cmd, "1gp");
            cmdType=9;
            ph.calibration(voltage,temperature,cmd);
          } else if(cmdType == 11){
            cmdType=10;
            strcpy(cmd, "2gp");
            ph.calibration(voltage,temperature,cmd);
          } else if(cmdType == 12){
            cmdType=11;
            strcpy(cmd, "3gp");
            ph.calibration(voltage,temperature,cmd);
          } else if(cmdType == 20){
            cmdType=12;
            strcpy(cmd, "4gp");
            ph.calibration(voltage,temperature,cmd);
          }  else if(cmdType == 13) {
            strcpy(cmd, "pfrate");
            ph.calibration(voltage,temperature,cmd);
          } else if(cmdType == 14) {
            strcpy(cmd, "pamnt");
            ph.calibration(voltage,temperature,cmd);
          } else if(cmdType == 15) {
            strcpy(cmd, "pwtime");
            ph.calibration(voltage,temperature,cmd);
          } else if(cmdType == 19) {
            strcpy(cmd, "pcalp");
            ph.calibration(voltage,temperature,cmd);
          } else if(cmdType == 23) {
            strcpy(cmd, "5gp");
            cmdType=20;
            ph.calibration(voltage,temperature,cmd);
          } else if(cmdType == 24) {
            strcpy(cmd, "pbuff");
            ph.calibration(voltage,temperature,cmd);
          }
        }
      }


    if(downButton.isReleased()) {
      isPressingDown = false;
      releasedTimeDown = millis();

      long pressDuration = releasedTimeDown - pressedTimeDown;

      if( pressDuration < SHORT_PRESS_TIME ) {
         if(cmdType == 4){
              strcpy(cmd, "mt");
              ph.calibration(voltage,temperature,cmd);
         } else if(cmdType == 1){
              strcpy(cmd, "1gp");
              cmdType=9;
              ph.calibration(voltage,temperature,cmd);
         } else if(cmdType == 9){ 
              strcpy(cmd, "2gp");
              cmdType=10;
              ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 10){
              strcpy(cmd, "3gp");
              cmdType=11;
              ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 11){
              strcpy(cmd, "4gp");
              cmdType=12;
              ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 12){
              strcpy(cmd, "5gp");
              cmdType=20;
              ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 0) {
            pump.stop();
        } else if(cmdType == 13) {
          strcpy(cmd, "mfrate");
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 14) {
          strcpy(cmd, "mamnt");
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 15) {
          strcpy(cmd, "mwtime");
          ph.calibration(voltage,temperature,cmd);
        }  else if(cmdType == 17) {
          pump.stop();
        } else if(cmdType == 19) {
          strcpy(cmd, "pcalm");
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 20) {
          cmdType=23;
          strcpy(cmd, "6gp");
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 24) {
            strcpy(cmd, "mbuff");
            ph.calibration(voltage,temperature,cmd);
        }
      }
    }

    if(isPressingSet == true && isLongDetected == false) {
      long pressDuration = millis() - pressedTimeSet;
      if( pressDuration > LONG_PRESS_TIME ) {
        isDosing = false;
        pump.stop();
        pump_wait = readFloatFromEEPROM(20);
        if(cmdType == 0) {
          strcpy(cmd, "enterph");
          cmdType=1;
          ph.calibration(voltage,temperature,cmd);
        } else if(cmdType == 1 || cmdType == 2 || cmdType >= 9) {
          strcpy(cmd, "exitph");
          cmdType=0;
          ph.calibration(voltage,temperature,cmd);
          first_run = true;
        } else if(cmdType == 4) {
          strcpy(cmd, "st");
          cmdType=0;
          ph.calibration(voltage,temperature,cmd);
          target_ph = readFloatFromEEPROM(8);
          first_run = true;
        } 
        isLongDetected = true;
        
      }
    }

    if(isPressingDown == true) {
      if(cmdType == 0 || cmdType == 17) {
        pump.flowPump(PUMP_MOMENTARY);
      } 
    }



    static unsigned long timepoint = millis();
    if ((isPressingSet == false && isPressingDown == false && isPressingUp == false && cmdType == 0) || first_run == true || cmdType == 2) {
      if (millis()-timepoint> pump_wait * 60000UL  || first_run == true || cmdType == 2 ) {                  //time interval: 1s
          first_run = false;
          timepoint = millis();
          temperature = readTemperature();         // read your temperature sensor to execute temperature compensation
          //voltage = analogRead(PH_PIN)/4096.0*5000;  // read the voltage
          //voltage = analogRead(PH_PIN) / ESPADC * ESPVOLTAGE;
          voltage = ads_read(); // / ESPADC * ESPVOLTAGE;
          if(cmdType == 2) {
            strcpy(cmd, "calph");
            ph.calibration(voltage,temperature,cmd);
          } else {
            phValue = ph.readPH(voltage,temperature, isDosing);  // convert voltage to pH with temperature compensation
          }
          if(phValue - phBuff > target_ph && cmdType != 2) {
            isDosing = true;
            pump_wait = WAIT_BETWEEN_DOSE;
            pump.flowPump(pump_amount);
          } else {
            if(isDosing == true) {
              isDosing = false;
              pump.stop();
              Serial.println(F("Reached Target"));
              pump_wait = readFloatFromEEPROM(20);
              first_run = true;
            }
          }
          // Serial.print(F("temperature:"));
          // Serial.print(temperature,1);
          // if(isF == 1) {
          //   Serial.print(F("^F  pH:"));
          // } else {
          //   Serial.print(F("^C  pH:"));
          // }
          //Serial.println(phValue,2);
      }
    }
    ph.calibration(voltage,temperature);           // calibration process by Serail CMD
}


int16_t ads_read(){ 
  int16_t adc0;
  int16_t mv=0;
  adc0 = ads.getLastConversionResults();
  mv = adc0 * 0.1875;
  //Serial.print(mv); Serial.println(" mV");
  return mv;
}


float readTemperature()
{
  sensors.requestTemperatures(); 
  // Serial.print("Celsius temperature: ");
  // Serial.print(sensors.getTempCByIndex(0)); 
  // Serial.print(" - Fahrenheit temperature: ");
  // Serial.println(sensors.getTempFByIndex(0));
  // delay(1000);
  if(isF == 1) {
    return sensors.getTempFByIndex(0);
  } else {
    return sensors.getTempCByIndex(0);
  }
  
}