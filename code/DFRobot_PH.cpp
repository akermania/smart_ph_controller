/*!
 * @file DFRobot_PH.cpp
 * @brief Arduino library for Gravity: Analog pH Sensor / Meter Kit V2, SKU: SEN0161-V2
 *
 * @copyright   Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author [Jiawei Zhang](jiawei.zhang@dfrobot.com)
 * @version  V1.0
 * @date  2018-11-06
 * @url https://github.com/DFRobot/DFRobot_PH
 */


#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "GravityPump.h"
#include "DFRobot_PH.h"
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define PH_8_VOLTAGE 1122
#define PH_6_VOLTAGE 1478
#define PH_5_VOLTAGE 1654
#define PH_3_VOLTAGE 2010

//#define EEPROM_write(address, p) {int i = 0; byte *pp = (byte*)&(p);for(; i < sizeof(p); i++) EEPROM.write(address+i, pp[i]);}
//#define EEPROM_read(address, p)  {int i = 0; byte *pp = (byte*)&(p);for(; i < sizeof(p); i++) pp[i]=EEPROM.read(address+i);}

template <typename T>
void EEPROM_read(int address, T &p) {
    byte *pp = (byte*)&p;
    for (int i = 0; i < sizeof(p); i++) {
        pp[i] = EEPROM.read(address + i);
    }
}

template <typename T>
void EEPROM_write(int address, const T &p) {
    const byte *pp = (const byte*)&p;
    for (int i = 0; i < sizeof(p); i++) {
        EEPROM.write(address + i, pp[i]);
    }
    EEPROM.commit(); // Ensure changes are saved
}

#define PHVALUEADDR 0x00    //the start address of the pH calibration parameters stored in the EEPROM
#define FLOWRATEADDRESS 0x24    //EEPROM address for flowrate, for more pump need to add more address. 
#define PUMPSPEEDADDRESS 0x28   //EEPROM address for speed, for more pump need to add more address.
#define CALIBRATIONTIME 15      //when Calibration pump running time, unit secend


char* DFRobot_PH::strupr(char* str) {
    if (str == NULL) return NULL;
    char *ptr = str;
    while (*ptr != ' ') {
        *ptr = toupper((unsigned char)*ptr);
        ptr++;
    }
    return str;
}

boolean phCalibrationFinish  = 0;
boolean enterCalibrationFlag = 0;
char buffer[10];

uint8_t col[2]; // Columns for ADC values.
uint8_t rows;   // Rows per line.
bool clearDisplay = true;

DFRobot_PH::DFRobot_PH()
{
    this->_temperature    = 25.0;
    this->_phValue        = 7.0;
    this->_acidVoltage    = 2032.44;    //buffer solution 4.0 at 25C
    this->_neutralVoltage = 1500.0;     //buffer solution 7.0 at 25C
    this->_voltage        = 1500.0;
    this->_targetPh       = 6.3;
    this->_isF            = 0;
    this->_flowRate       = 0.6;
    this->_pumpSpeed      = 160;
    this->_pumpAmount     = 1.0;
    this->_pumpWait       = 60.0;
    this->_flowMl         = 6.0;
    this->_phBuff         = 0.1;
}

DFRobot_PH::~DFRobot_PH()
{

}

void DFRobot_PH::begin()
{
    
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    delay(500);
    display.clearDisplay();
    display.setCursor(25, 15);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.println(F(" pH Controller"));
    display.setCursor(25, 35);
    display.setTextSize(1);
    display.print(F("Initializing"));
    display.display();
    EEPROM.begin(512);
    EEPROM_read(PHVALUEADDR, this->_neutralVoltage);  //load the neutral (pH = 7.0)voltage of the pH board from the EEPROM
    if((EEPROM.read(PHVALUEADDR)==0xFF && EEPROM.read(PHVALUEADDR+1)==0xFF && EEPROM.read(PHVALUEADDR+2)==0xFF && EEPROM.read(PHVALUEADDR+3)==0xFF) || isnan(this->_neutralVoltage)){
        this->_neutralVoltage = 1500.0;  // new EEPROM, write typical voltage
        EEPROM_write(PHVALUEADDR, this->_neutralVoltage);
    }
    EEPROM_read(PHVALUEADDR+4, this->_acidVoltage);//load the acid (pH = 4.0) voltage of the pH board from the EEPROM
    ////Serial.print("_acidVoltage:");
    ////Serial.println(this->_acidVoltage);
    if((EEPROM.read(PHVALUEADDR+4)==0xFF && EEPROM.read(PHVALUEADDR+5)==0xFF && EEPROM.read(PHVALUEADDR+6)==0xFF && EEPROM.read(PHVALUEADDR+7)==0xFF) || isnan(this->_acidVoltage)){
        this->_acidVoltage = 2032.44;  // new EEPROM, write typical voltage
        EEPROM_write(PHVALUEADDR+4, this->_acidVoltage);
    }

    //Serial.println(this->_neutralVoltage);
    //Serial.println(this->_acidVoltage);

    EEPROM_read(PHVALUEADDR+8, this->_targetPh);//load the acid (pH = 4.0) voltage of the pH board from the EEPROM
    ////Serial.print("_acidVoltage:");
    ////Serial.println(this->_acidVoltage);
    if((EEPROM.read(PHVALUEADDR+8)==0xFF && EEPROM.read(PHVALUEADDR+9)==0xFF && EEPROM.read(PHVALUEADDR+10)==0xFF && EEPROM.read(PHVALUEADDR+11)==0xFF) || isnan(this->_targetPh)){
        this->_targetPh = 6.3;  // new EEPROM, write typical voltage
        EEPROM_write(PHVALUEADDR+8, this->_targetPh);
    }

    EEPROM_read(PHVALUEADDR+12, this->_isF);//load the acid (pH = 4.0) voltage of the pH board from the EEPROM
    ////Serial.print("_acidVoltage:");
    ////Serial.println(this->_acidVoltage);
    if(EEPROM.read(PHVALUEADDR+12)==0xFF  || isnan(this->_isF)){
        this->_isF = 0;  // new EEPROM, write typical voltage
        EEPROM_write(PHVALUEADDR+12, this->_isF);
    }

    EEPROM_read(PHVALUEADDR+16, this->_pumpAmount);//load the acid (pH = 4.0) voltage of the pH board from the EEPROM
    ////Serial.println(this->_acidVoltage);
    if(EEPROM.read(PHVALUEADDR+16)==0xFF  || isnan(this->_pumpAmount)){
        this->_pumpAmount = 1.0;  // new EEPROM, write typical voltage
        EEPROM_write(PHVALUEADDR+16, this->_pumpAmount);
    }


    EEPROM_read(PHVALUEADDR+20, this->_pumpWait);//load the acid (pH = 4.0) voltage of the pH board from the EEPROM
    ////Serial.println(this->_acidVoltage);
    if(EEPROM.read(PHVALUEADDR+20)==0xFF || isnan(this->_pumpWait)){
        this->_pumpWait = 60.0;  // new EEPROM, write typical voltage
        EEPROM_write(PHVALUEADDR+20, this->_pumpWait);
    }

    EEPROM_read(PHVALUEADDR+24, this->_flowMl);//load the acid (pH = 4.0) voltage of the pH board from the EEPROM
    ////Serial.println(this->_acidVoltage);
    if(EEPROM.read(PHVALUEADDR+24)==0xFF  || isnan(this->_flowMl)){
        this->_flowMl = 6.0;  // new EEPROM, write typical voltage
        EEPROM_write(PHVALUEADDR+24, this->_flowMl);
    }
   
    EEPROM_read(PHVALUEADDR+40, this->_phBuff);//load the acid (pH = 4.0) voltage of the pH board from the EEPROM
    ////Serial.println(this->_acidVoltage);
    if(EEPROM.read(PHVALUEADDR+40)==0xFF  || isnan(this->_phBuff)){
        this->_phBuff = 0.1;  // new EEPROM, write typical voltage
        EEPROM_write(PHVALUEADDR+40, this->_phBuff);
    }

    EEPROM_read(FLOWRATEADDRESS, this->_flowRate);
    if(isnan(this->_flowRate)) {
      this->_flowRate = 0.6;
    }
    delay(5);
    EEPROM_read(PUMPSPEEDADDRESS, this->_pumpSpeed);
    if(isnan(this->_pumpSpeed)) {
      this->_pumpSpeed = 160;
    }
} 

float DFRobot_PH::readPH(float voltage, float temperature, bool isDosing)
{
    //Serial.println(voltage);
    //Serial.println(this->_neutralVoltage);
    //Serial.println(this->_acidVoltage);
    float slope = (7.0-4.0)/((this->_neutralVoltage-1500.0)/3.0 - (this->_acidVoltage-1500.0)/3.0);  // two point: (_neutralVoltage,7.0),(_acidVoltage,4.0)
    //Serial.println(slope);
    float intercept =  7.0 - slope*(this->_neutralVoltage-1500.0)/3.0;
    //Serial.println(intercept);
    ////Serial.print("slope:");
    ////Serial.print(slope);
    ////Serial.print(",intercept:");
    ////Serial.println(intercept);
    float uncompensatedPhValue = slope*(voltage-1500.0)/3.0+intercept;  //y = k*x + b

    float standardTemperature = 25.0;
    float temperatureCoefficient = -0.003;
    float temperatureC = temperature;
    if(this->_isF == 1){
      float temperatureC = (temperature - 32) / 1.8;
    }
    this->_phValue = uncompensatedPhValue + (temperatureC - standardTemperature) * temperatureCoefficient;

    //Serial.println(this->_phValue);
    if(enterCalibrationFlag == 0) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 5);
        display.print(F("Temperature: "));
        display.print(temperature,1);
        if(this->_isF == 1) {
            display.print(F(" F"));
        } else {
            display.print(F(" C"));
        }
        display.setTextSize(2);
        display.println();
        display.print(F("pH: "));
        display.print(_phValue,2);
        if(isDosing) {
          display.setTextSize(1);
          display.println(F("..v.."));
        } else {
          display.println();
        }
        display.setTextSize(1);
        display.println();
        display.println();
        display.print(F("Target: "));
        display.print(_targetPh,2);
        display.display();
    }
    return _phValue;
}


void DFRobot_PH::calibration(float voltage, float temperature,char* cmd)
{
    this->_voltage = voltage;
    this->_temperature = temperature;
    String sCmd = String(cmd);
    sCmd.toUpperCase();
    phCalibration(cmdParse(sCmd.c_str()));  // if received Serial CMD from the serial monitor, enter into the calibration mode
}

void DFRobot_PH::calibration(float voltage, float temperature)
{
    this->_voltage = voltage;
    this->_temperature = temperature;
    if(cmdSerialDataAvailable() > 0){
        phCalibration(cmdParse());  // if received Serial CMD from the serial monitor, enter into the calibration mode
    }

}

boolean DFRobot_PH::cmdSerialDataAvailable()
{
    char cmdReceivedChar;
    static unsigned long cmdReceivedTimeOut = millis();
    while(Serial.available()>0){
        if(millis() - cmdReceivedTimeOut > 500U){
            this->_cmdReceivedBufferIndex = 0;
            memset(this->_cmdReceivedBuffer,0,(ReceivedBufferLength));
        }
        cmdReceivedTimeOut = millis();
        cmdReceivedChar = Serial.read();
        if (cmdReceivedChar == '\n' || this->_cmdReceivedBufferIndex==ReceivedBufferLength-1){
            this->_cmdReceivedBufferIndex = 0;
            strupr(this->_cmdReceivedBuffer);
            return true;
        }else{
            this->_cmdReceivedBuffer[this->_cmdReceivedBufferIndex] = cmdReceivedChar;
            this->_cmdReceivedBufferIndex++;
        }
    }
    return false;
}

byte DFRobot_PH::cmdParse(const char* cmd)
{
    int modeIndex = 0;
    if(strstr(cmd, "ENTERPH")      != NULL){
        modeIndex = 1;
    }else if(strstr(cmd, "EXITPH") != NULL){
        modeIndex = 3;
    }else if(strstr(cmd, "CALPH")  != NULL){
        modeIndex = 2;
    }
    else if(strstr(cmd, "TARGET")  != NULL){
        modeIndex = 4;
    }
    else if(strstr(cmd, "PT")  != NULL){
        modeIndex = 5;
    }
    else if(strstr(cmd, "MT")  != NULL){
        modeIndex = 6;
    }
    else if(String(cmd).equals("ST")){
        modeIndex = 7;
    }
    else if(strstr(cmd, "TT")  != NULL){
        modeIndex = 8;
    }
    else if(strstr(cmd, "1GP")  != NULL){
        modeIndex = 9;
    }
    else if(strstr(cmd, "2GP")  != NULL){
        modeIndex = 10;
    }
    else if(strstr(cmd, "3GP")  != NULL){
        modeIndex = 11;
    } 
    else if(strstr(cmd, "4GP")  != NULL){
        modeIndex = 12;
    } 
    else if(String(cmd).equals("5GP")){
        modeIndex = 122;
    }
    else if(String(cmd).equals("6GP")){
        modeIndex = 123;
    } 
    else if(strstr(cmd, "LDOSE")  != NULL){
        modeIndex = 13;
    }
    else if(strstr(cmd, "BACK")  != NULL){
        modeIndex = 14;
    }
    else if(String(cmd).equals("FRATE")){
        modeIndex = 15;
    }
    else if(String(cmd).equals("PFRATE")){
        modeIndex = 16;
    }
    else if(String(cmd).equals("MFRATE")){
        modeIndex = 17;
    }
    else if(String(cmd).equals("SFRATE")){
        modeIndex = 18;
    }
    else if(String(cmd).equals("AMNT")){
        modeIndex = 19;
    }
    else if(String(cmd).equals("PAMNT")){
        modeIndex = 20;
    }
    else if(String(cmd).equals("MAMNT")){
        modeIndex = 21;
    }
    else if(String(cmd).equals("SAMNT")){
        modeIndex = 22;
    }
    else if(String(cmd).equals("WTIME")){
        modeIndex = 23;
    }
    else if(String(cmd).equals("PWTIME")){
        modeIndex = 24;
    }
    else if(String(cmd).equals("MWTIME")){
        modeIndex = 25;
    } 
    else if(String(cmd).equals("SWTIME")){
        modeIndex = 26;
    }
    else if(String(cmd).equals("PCAL")){
        modeIndex = 27;
    } 
    else if(String(cmd).equals("PCAL2")){
        modeIndex = 28;
    }
    else if(String(cmd).equals("PSTART")){
        modeIndex = 29;
    } 
    else if(String(cmd).equals("PCALW")){
        modeIndex = 30;
    }
    else if(String(cmd).equals("PCALP")){
        modeIndex = 31;
    }
    else if(String(cmd).equals("PCALM")){
        modeIndex = 32;
    }
    else if(String(cmd).equals("PCALS")){
        modeIndex = 33;
    } 
    else if(String(cmd).equals("S5GP")){
        modeIndex = 34;
    }
    else if(String(cmd).equals("BUFF")){
        modeIndex = 35;
    }  
    else if(String(cmd).equals("PBUFF")){
        modeIndex = 36;
    }
    else if(String(cmd).equals("MBUFF")){
        modeIndex = 37;
    }
    else if(String(cmd).equals("SBUFF")){
        modeIndex = 38;
    }
    return modeIndex;
}

byte DFRobot_PH::cmdParse()
{
    int modeIndex = 0;
    if(strstr(this->_cmdReceivedBuffer, "ENTERPH")      != NULL){
        modeIndex = 1;
    }else if(strstr(this->_cmdReceivedBuffer, "EXITPH") != NULL){
        modeIndex = 3;
    }else if(strstr(this->_cmdReceivedBuffer, "CALPH")  != NULL){
        modeIndex = 2;
    }else if(strstr(this->_cmdReceivedBuffer, "TARGET")  != NULL){
        modeIndex = 4;
    }else if(strstr(this->_cmdReceivedBuffer, "PT")  != NULL){
        modeIndex = 5;
    }else if(strstr(this->_cmdReceivedBuffer, "MT")  != NULL){
        modeIndex = 6;
    }else if(strstr(this->_cmdReceivedBuffer, "ST")  != NULL){
        modeIndex = 7;
    }else if(strstr(this->_cmdReceivedBuffer, "TT")  != NULL){
        modeIndex = 8;
    }else if(strstr(this->_cmdReceivedBuffer, "1GP")  != NULL){
        modeIndex = 9;
    }else if(strstr(this->_cmdReceivedBuffer, "2GP")  != NULL){
        modeIndex = 10;
    }else if(strstr(this->_cmdReceivedBuffer, "3GP")  != NULL){
        modeIndex = 11;
    }else if(strstr(this->_cmdReceivedBuffer, "4GP")  != NULL){
        modeIndex = 12;
    } else if(strstr(this->_cmdReceivedBuffer, "LDOSE")  != NULL){
        modeIndex = 13;
    }else if(strstr(this->_cmdReceivedBuffer, "BACK")  != NULL){
        modeIndex = 14;
    }else if(strstr(this->_cmdReceivedBuffer, "FRATE")  != NULL){
        modeIndex = 15;
    }else if(strstr(this->_cmdReceivedBuffer, "PFRATE")  != NULL){
        modeIndex = 16;
    }else if(strstr(this->_cmdReceivedBuffer, "MFRATE")  != NULL){
        modeIndex = 17;
    }else if(strstr(this->_cmdReceivedBuffer, "SFRATE")  != NULL){
        modeIndex = 18;
    }
    return modeIndex;
}

void DFRobot_PH::phCalibration(int mode)
{
    const float epsilon = 0.0001;
    char *receivedBufferPtr;
    if(mode == 0) {
        if(enterCalibrationFlag){
            //Serial.println(F(">>>Command Error<<<"));
        }
    } else if(mode == 1) {
        enterCalibrationFlag = 1;
        phCalibrationFinish  = 0;
        // //Serial.println();
        // //Serial.println(F(">>>Enter PH Calibration Mode<<<"));
        // //Serial.println(F(">>>Please put the probe into the 4.0 or 7.0 standard buffer solution<<<"));
        // //Serial.println();
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 5);
        display.print(F("Calibration Mode"));
        display.setTextSize(1);
        display.setCursor(0, 20);
        display.print(F("Please insert the probe to the 4.0 or 7.0 standard buffer solution, and press 'SET'"));
        display.display();
   } else if(mode == 2) {
        if(enterCalibrationFlag){
            display.clearDisplay();
            if((this->_voltage>1322)&&(this->_voltage<1678)){        // buffer solution:7.0{
                // //Serial.println();
                // //Serial.print(F(">>>Buffer Solution:7.0"));
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.print(F("Buffer Solution"));
                display.setTextSize(2);
                display.setCursor(0, 20);
                display.print(F("7.0"));
                display.setTextSize(1);
                display.setCursor(0, 40);
                display.print(F("Move to the next solution, or save and exit"));
                display.display();
                this->_neutralVoltage =  this->_voltage;
                // //Serial.println(F(",Send EXITPH to Save and Exit<<<"));
                // //Serial.println();
                phCalibrationFinish = 1;
            }else if((this->_voltage>1854)&&(this->_voltage<2210)){  //buffer solution:4.0
                // //Serial.println();
                // //Serial.print(F(">>>Buffer Solution:4.0"));
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.print(F("Buffer Solution"));
                display.setTextSize(2);
                display.setCursor(0, 20);
                display.print(F("4.0"));
                display.setTextSize(1);
                display.setCursor(0, 40);
                display.print(F("Move to the next solution, or save and exit"));
                display.display();
                this->_acidVoltage =  this->_voltage;
                // //Serial.println(F(",Send EXITPH to Save and Exit<<<")); 
                // //Serial.println();
                phCalibrationFinish = 1;
            }else{
                //Serial.println();
                //Serial.print(F(">>>Buffer Solution Error Try Again<<<"));
                //Serial.println();                                    // not buffer solution or faulty operation
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.print(F("Not a Buffer Solution"));
                display.setTextSize(1);
                display.setCursor(0, 25);
                display.print(F("Try Again"));
                display.display();
                phCalibrationFinish = 0;
            }
          }
        } else if(mode == 3) {
        if(enterCalibrationFlag){
            //Serial.println();
            if(phCalibrationFinish){
                if((this->_voltage>1322)&&(this->_voltage<1678)){
                    EEPROM_write(PHVALUEADDR, this->_neutralVoltage);
                }else if((this->_voltage>1854)&&(this->_voltage<2210)){
                    EEPROM_write(PHVALUEADDR+4, this->_acidVoltage);
                }
                //Serial.print(F(">>>Calibration Successful"));
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 25);
                display.print(F("Calibration "));
                display.setCursor(10, 35);
                display.print(F("Successful"));
                display.display();
            }else{
                //Serial.print(F(">>>Exit"));
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 25);
                display.print(F("Exit"));
                display.display();
            }
            //Serial.println(F(",Exit PH Calibration Mode<<<"));
            //Serial.println();
            delay(2000);
            phCalibrationFinish  = 0;
            enterCalibrationFlag = 0;
          }
        } else if(mode == 4) {
            if(enterCalibrationFlag == 0){
                //Serial.println(F(">>>Set PH Target"));
                dtostrf(_targetPh, 6, 2, buffer);
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.print(F("Set pH Target: "));
                display.setTextSize(2);
                display.setCursor(0, 35);
                display.print(buffer);
                display.display();
                enterCalibrationFlag = 1;
            }
        } else if(mode == 5) {
            if(enterCalibrationFlag){
                this->_targetPh = _targetPh + 0.1;
                dtostrf(_targetPh, 6, 2, buffer);
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.print(F("Set pH Target: "));
                display.setTextSize(2);
                display.setCursor(0, 35);
                display.print(buffer);
                display.display();
            }
        } else if(mode == 6) {
            if(enterCalibrationFlag){
                this->_targetPh = _targetPh - 0.1;
                dtostrf(_targetPh, 6, 2, buffer);
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.print(F("Set pH Target: "));
                display.setTextSize(2);
                display.setCursor(0, 35);
                display.print(buffer);
                display.display();
            }
        } else if(mode == 7) {
            if(enterCalibrationFlag) {
                EEPROM_write(PHVALUEADDR+8, this->_targetPh);
                enterCalibrationFlag = 0;
                //Serial.println(F(">>>Set Target Successful"));
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 25);
                display.print(F("Set Target "));
                display.setCursor(10, 35);
                display.print(F("Successful"));
                display.display();
                delay(2000);
            }       
        } else if(mode == 8) {
            if(enterCalibrationFlag == 0 && phCalibrationFinish == 0) {
                display.clearDisplay();
                display.setTextSize(2);
                display.setCursor(0, 15);
                if(this->_isF == 0) {
                    this->_isF = 1;
                    EEPROM_write(PHVALUEADDR+12, this->_isF);
                    //Serial.println(F(">>>Set Temp to F"));
                    display.print(F("Fahrenheit"));
                } else {
                    this->_isF = 0;
                    EEPROM_write(PHVALUEADDR+12, this->_isF);
                    //Serial.println(F(">>>Set Temp to C"));
                    display.print(F("Celsius"));
                } 
                display.display();
            }       
        } else if(mode == 9) {
            enterCalibrationFlag = 1;
            //Serial.println(F(">>>Pump Settings - Flow Rate"));
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 5);
            display.println(F("Pump Settings"));
            display.setCursor(0, 15);
            display.println(F("> Flow Rate"));
            display.println(F("  Amount"));
            display.println(F("  Wait time"));
            display.println(F("  Calibrate"));
            display.println(F("  Test 2 ml"));
            display.println(F("  Buffer"));
            display.display();
      } else if(mode == 10) {
            enterCalibrationFlag = 1;
            //Serial.println(F(">>>Pump Settings - Amount"));
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 5);
            display.println(F("Pump Settings"));
            display.setCursor(0, 15);
            display.println(F("  Flow Rate"));
            display.println(F("> Amount"));
            display.println(F("  Wait time"));
            display.println(F("  Calibrate"));
            display.println(F("  Test 2 ml"));
            display.println(F("  Buffer"));
            display.display();
       } else if(mode == 11) {
            enterCalibrationFlag = 1;
            //Serial.println(F(">>>Pump Settings - Wait time"));
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 5);
            display.println(F("Pump Settings"));
            display.setCursor(0, 15);
            display.println(F("  Flow Rate"));
            display.println(F("  Amount"));
            display.println(F("> Wait time"));
            display.println(F("  Calibrate"));
            display.println(F("  Test 2 ml"));
            display.println(F("  Buffer"));
            display.display();
       } else if(mode == 12) {
            enterCalibrationFlag = 1;
            //Serial.println(F(">>>Pump Settings - Calibrate"));
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 5);
            display.println(F("Pump Settings"));
            display.setCursor(0, 15);
            display.println(F("  Flow Rate"));
            display.println(F("  Amount"));
            display.println(F("  Wait time"));
            display.println(F("> Calibrate"));
            display.println(F("  Test 2 ml"));
            display.println(F("  Buffer"));
            display.display();
       } else if(mode == 122) {
            enterCalibrationFlag = 1;
            //Serial.println(F(">>>Pump Settings - Calibrate"));
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 5);
            display.println(F("Pump Settings"));
            display.setCursor(0, 15);
            display.println(F("  Flow Rate"));
            display.println(F("  Amount"));
            display.println(F("  Wait time"));
            display.println(F("  Calibrate"));
            display.println(F("> Test 2 ml"));
            display.println(F("  Buffer"));
            display.display();
       } else if(mode == 123) {
            enterCalibrationFlag = 1;
            //Serial.println(F(">>>Pump Settings - Calibrate"));
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 5);
            display.println(F("Pump Settings"));
            display.setCursor(0, 15);
            display.println(F("  Flow Rate"));
            display.println(F("  Amount"));
            display.println(F("  Wait time"));
            display.println(F("  Calibrate"));
            display.println(F("  Test 2 ml"));
            display.println(F("> Buffer"));
            display.display();
       } else if(mode == 13) {
          //Serial.println(F(">>>Dosing..."));
          display.clearDisplay();
          display.setTextSize(2);
          display.setCursor(0, 5);
          display.println();
          display.println(F("Dosing..."));
          display.println();
          display.display();
       } else if(mode == 14) {
          enterCalibrationFlag = 1;
          display.clearDisplay();
          display.setTextSize(1);
          display.println();
          display.println();
          display.setTextSize(2);
          display.print(F("pH: "));
          display.println(_phValue,2);
          display.setTextSize(1);
          display.println();
          display.print(F("Target: "));
          display.println(_targetPh,2);
          display.display();
          //display.display();
       } else if(mode == 15) {
          //Serial.println(F(">>>Set Flow Rate"));
          dtostrf(_flowRate, 6, 2, buffer);
          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(0, 5);
          display.println(F("Set Flow Rate: "));
          display.println();
          display.setTextSize(2);
          display.println(buffer);
          display.display();
          enterCalibrationFlag = 1;
       } else if(mode == 16) {
            if(enterCalibrationFlag){
                this->_flowRate = _flowRate + 0.05;
                dtostrf(_flowRate, 6, 2, buffer);
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.println(F("Set Flow Rate: "));
                display.println();
                display.setTextSize(2);
                display.println(buffer);
                display.display();
            }
        } else if(mode == 17) {
            if(enterCalibrationFlag){
                if (abs(_flowRate - 0.05) < epsilon) {
                  return;
                }
                this->_flowRate = _flowRate - 0.05;
                dtostrf(_flowRate, 6, 2, buffer);
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.println(F("Set Flow Rate: "));
                display.println();
                display.setTextSize(2);
                display.println(buffer);
                display.display();
            }
        } else if(mode == 18) {
            if(enterCalibrationFlag) {
                EEPROM_write(FLOWRATEADDRESS, this->_flowRate);
                enterCalibrationFlag = 0;
                //Serial.println(F(">>>Set flow Rate Successful"));
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.println();
                display.println(F("Set Flow Rate "));
                display.println(F("Successful"));
                display.display();
                delay(1000);
            }       
        } else if(mode == 19) {
          dtostrf(_pumpAmount, 6, 2, buffer);
          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(0, 5);
          display.println(F("Set Amount: "));
          display.println();
          display.setTextSize(2);
          display.println(buffer);
          display.display();
          enterCalibrationFlag = 1;
       } else if(mode == 20) {
            if(enterCalibrationFlag){
                if(_pumpAmount <= 0.2 ) {
                  this->_pumpAmount = _pumpAmount + 0.01;
                } else if(_pumpAmount <= 1.0 ) {
                  this->_pumpAmount = _pumpAmount + 0.1;
                } else {
                  this->_pumpAmount = _pumpAmount + 0.5;
                }
                dtostrf(_pumpAmount, 6, 2, buffer);
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.println(F("Set Amount: "));
                display.println();
                display.setTextSize(2);
                display.println(buffer);
                display.display();
            }
        } else if(mode == 21) {
            if(enterCalibrationFlag){
                if (abs(_pumpAmount - 0.1) < epsilon) {
                  return;
                }
                if(_pumpAmount <= 0.2 ) {
                  this->_pumpAmount = _pumpAmount - 0.01;
                } else if(_pumpAmount <= 1.0 ) {
                  this->_pumpAmount = _pumpAmount - 0.1;
                } else {
                  this->_pumpAmount = _pumpAmount - 0.5;
                }
                dtostrf(_pumpAmount, 6, 2, buffer);
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.println(F("Set Amount: "));
                display.println();
                display.setTextSize(2);
                display.println(buffer);
                display.display();
            }
        } else if(mode == 22) {
            if(enterCalibrationFlag) {
                EEPROM_write(PHVALUEADDR+16, this->_pumpAmount);
                enterCalibrationFlag = 0;
                //Serial.println(F(">>>Set Amount Successful"));
                display.clearDisplay();
                display.setTextSize(1);
                //display.setCursor(0, 25);
                display.println();
                display.println(F("Set Amount "));
                //display.setCursor(10, 35);
                display.println(F("Successful"));
                display.display();
                delay(1000);
            }       
        } else if(mode == 23) {
          dtostrf(_pumpWait, 6, 2, buffer);
          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(0, 5);
          display.println(F("Set Wait Time: "));
          display.println();
          display.setTextSize(2);
          display.println(buffer);
          display.display();
          enterCalibrationFlag = 1;
       } else if(mode == 24) {
            if(enterCalibrationFlag){
                if(_pumpWait >= 1.0){
                  this->_pumpWait = _pumpWait + 1.0;
                } else {
                  this->_pumpWait = _pumpWait + 0.1;
                }
                dtostrf(_pumpWait, 6, 2, buffer);
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.println(F("Set Wait Time: "));
                display.println();
                display.setTextSize(2);
                display.println(buffer);
                display.display();
            }
        } else if(mode == 25) {
            if(enterCalibrationFlag){
                if (abs(_pumpWait - 0.1) < epsilon) {
                  return;
                }
                if(_pumpWait <= 1.0){
                  this->_pumpWait = _pumpWait - 0.1;
                } else {
                  this->_pumpWait = _pumpWait - 1.0;
                }
                dtostrf(_pumpWait, 6, 2, buffer);
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.println(F("Set Wait Time: "));
                display.println();
                display.setTextSize(2);
                display.println(buffer);
                display.display();
            }
        } else if(mode == 26) {
            if(enterCalibrationFlag) {
                EEPROM_write(PHVALUEADDR+20, this->_pumpWait);
                enterCalibrationFlag = 0;
                //Serial.println(F(">>>Set Wait Time Successful"));
                display.clearDisplay();
                display.setTextSize(1);
                display.println();
                display.println(F("Set Wait Time "));
                display.println(F("Successful"));
                display.display();
                delay(1000);
            }       
        } else if(mode == 27) {
            enterCalibrationFlag = 1;
            //Serial.println(F(">>>Enter Pump Calibration Mode<<<"));
            //Serial.println(F(">>>Please set one end of the pump in a liquid and the other end in a measuring cup<<<"));
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 5);
            display.println(F("Pump Calibration"));
            display.setTextSize(1);
            display.println();
            display.println(F("Please set one end of the pump in a liquid and the other end in a measuring cup and press 'SET'"));
            display.display();
        } else if(mode == 28) {
            enterCalibrationFlag = 1;
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 5);
            display.println(F("Pump Calibration"));
            display.println();
            display.println(F("Make sure that the tube is full of liquid. Press 'DOWN' to fill it and press 'SET' to start calibration"));
            display.display();
        } else if(mode == 29) {
            enterCalibrationFlag = 1;
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 5);
            display.println(F("Calibrating..."));
            display.display();
        } else if(mode == 30) {
          dtostrf(_flowMl, 6, 2, buffer);
          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(0, 5);
          display.println(F("Amount in ml: "));
          display.println();
          display.setTextSize(2);
          display.println(buffer);
          display.display();
          enterCalibrationFlag = 1;
       } else if(mode == 31) {
            if(enterCalibrationFlag){
                this->_flowMl = _flowMl + 0.1;
                dtostrf(_flowMl, 6, 2, buffer);
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.println(F("Amount in ml: "));
                display.println();
                display.setTextSize(2);
                display.println(buffer);
                display.display();
            }
        } else if(mode == 32) {
            if(enterCalibrationFlag){
                if(_flowMl == 0.1) {
                  return;
                }
                this->_flowMl = _flowMl - 0.1;
                dtostrf(_flowMl, 6, 2, buffer);
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.println(F("Amount in ml: "));
                display.println();
                display.setTextSize(2);
                display.println(buffer);
                display.display();
            }
        } else if(mode == 33) {
            if(enterCalibrationFlag) {
                EEPROM_write(PHVALUEADDR+24, this->_flowMl);
                enterCalibrationFlag = 0;
                //Serial.println(F(">>>Set Wait Time Successful"));
                display.clearDisplay();
                display.setTextSize(1);
                display.println();
                display.println(F("Calibration "));
                display.println(F("Successful"));
                display.display();
                delay(1000);
            }       
        } else if(mode == 34) {
            enterCalibrationFlag = 1;
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 5);
            display.println(F("This will test 2ml"));
            display.println();
            display.println(F("Continute?"));
            display.display();
        } else if(mode == 35) {
          dtostrf(_phBuff, 6, 2, buffer);
          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(0, 5);
          display.println(F("Set Buffer: "));
          display.println();
          display.setTextSize(2);
          display.println(buffer);
          display.display();
          enterCalibrationFlag = 1;
       } else if(mode == 36) {
            if(enterCalibrationFlag){
                if(_phBuff < 0.2 ) {
                  this->_phBuff = _phBuff + 0.01;
                } else {
                  this->_phBuff = _phBuff + 0.1;
                }
                dtostrf(_phBuff, 6, 2, buffer);
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.println(F("Set Buffer: "));
                display.println();
                display.setTextSize(2);
                display.println(buffer);
                display.display();
            }
        } else if(mode == 37) {
            if(enterCalibrationFlag){
                if(_phBuff == 0.01) {
                  return;
                }
                if(_phBuff <= 0.2 ) {
                  this->_phBuff = _phBuff - 0.01;
                }  else {
                  this->_phBuff = _phBuff - 0.1;
                }
                dtostrf(_phBuff, 6, 2, buffer);
                display.clearDisplay();
                display.setTextSize(1);
                display.setCursor(0, 5);
                display.println(F("Set Buffer: "));
                display.println();
                display.setTextSize(2);
                display.println(buffer);
                display.display();
            }
        } else if(mode == 38) {
            if(enterCalibrationFlag) {
                EEPROM_write(PHVALUEADDR+40, this->_phBuff);
                enterCalibrationFlag = 0;
                //Serial.println(F(">>>Set Amount Successful"));
                display.clearDisplay();
                display.setTextSize(1);
                //display.setCursor(0, 25);
                display.println();
                display.println(F("Set Buffer "));
                //display.setCursor(10, 35);
                display.println(F("Successful"));
                display.display();
                delay(1000);
            }       
        }
    clearDisplay = true;

}

