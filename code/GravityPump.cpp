#include "GravityPump.h"
#include <EEPROM.h>

#define FLOWRATEADDRESS 0x24    //EEPROM address for flowrate, for more pump need to add more address. 
#define PUMPSPEEDADDRESS 0x28   //EEPROM address for speed, for more pump need to add more address.
#define CALIBRATIONTIME 15      //when Calibration pump running time, unit secend
#define ReceivedBufferLength 20

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


GravityPump::GravityPump()
{
}

GravityPump::~GravityPump()
{

}


void GravityPump::setPin(int pin)   //pump pin setting
{
    EEPROM.begin(512);
    this->_pin = pin;
    this->_pumpServo.attach(this->_pin);
}

void GravityPump::getFlowRateAndSpeed()      //flowrate and speed reading from EEPROM
{
    EEPROM_read(FLOWRATEADDRESS, this->_flowRate);
    if(isnan(this->_flowRate)  || this->_pumpSpeed < 0.05) {
      this->_flowRate = 0.6;
    }
    delay(5);
    EEPROM_read(PUMPSPEEDADDRESS, this->_pumpSpeed);
    if(isnan(this->_pumpSpeed) || this->_pumpSpeed < 0) {
      this->_pumpSpeed = 160;
    }
}

void GravityPump::update()      //get the state from system, need to be put in the loop.
{
    pumpDriver(this->_pumpSpeed,this->_intervalTime);
}

void GravityPump::pumpDriver(int speed, unsigned long runTime)      //the basic pump function, have to given speed in number(0 to 180. 90 for stop, 
                                                                    //0 and 180 is max speed in each direction.)and runing time in milliseced.
{
    if(this->_stopFlag || millis() - this->_startTime >= runTime)
    {
        this->_runFlag = false;
        this->_stopFlag = false;
        this->_pumpServo.write(this->_servoStop);
        this->_startTime = millis() + runTime;
    }
    else
    {
        this->_pumpServo.write(speed);
    }    
}

float GravityPump::flowPump(float quantitation)     //quantification setting pump function,base on the basic function.the function need to given a quantification. Then the pump will dosing the quantification
                                                    //in given number. if you have Calibration, the number will be close to result.
{
    unsigned long runTime = 0;
    if(!this->_runFlag)
    {
        this->_runFlag = true;
        this->_intervalTime = 1000*(quantitation / this->_flowRate);
        this->_startTime = millis();
        return this->_intervalTime; 
    }
    return 0;
}

float GravityPump::timerPump(unsigned long runTime) //timer pump function,base on the basic function.the function need to  given the running time then the pump will dosing as long as your have given.
                                                    //and return the quantitation. if you have Calibration,  the number will be close to result.
{
    if(!this->_runFlag)
    {
        this->_runFlag = true;
        this->_intervalTime = runTime*1000;
        this->_startTime = millis();
        return (this->_flowRate*runTime);
    }
    return 0;
}

void GravityPump::stop()    //stop function. whenever you use this function the pump will stop immediately.
{
    this->_stopFlag = true;
    this->_runFlag = false;
}

void GravityPump::calFlowRate(int speed) //Calibration function.the speed parameter is running speed what you needed.
{
    //please input the "STARTCAL" in serial to start cal
    //Pump some liquid in some secs
    //please input the actual mumber in serial by "SETCAL:XX"
    //cal end
    this->_pumpSpeed = speed;
    if(serialDataAvailable() > 0)
    {
        pumpCalibration(uartParse());
    }
}

bool GravityPump::serialDataAvailable()
{
  char receivedChar;
  static unsigned long receivedTimeOut = millis();
  while (Serial.available()>0) 
  {   
    if (millis() - receivedTimeOut > 1000U) 
    {
      this->_receivedBufferIndex = 0;
      memset(this->_receivedBuffer,0,RECEIVEDBUFFERLENGTH);
    }
    receivedTimeOut = millis();
    receivedChar = Serial.read();
    if (receivedChar == '\n' || this->_receivedBufferIndex==ReceivedBufferLength-1){
		this->_receivedBufferIndex = 0;
		strupr(this->_receivedBuffer);
		return true;
    }
    else{
      this->_receivedBuffer[this->_receivedBufferIndex] = receivedChar;
      this->_receivedBufferIndex++;
    }
  }
  return false;
}

byte GravityPump::uartParse()
{
  byte modeIndex = 0;
  if(strstr(this->_receivedBuffer, "STARTCAL") != NULL) 
      modeIndex = 1;
  else if(strstr(this->_receivedBuffer, "SETCAL:") != NULL) 
      modeIndex = 2;
  return modeIndex;
}

void GravityPump::pumpCalibration(byte mode)
{
    char *receivedBufferPtr;
    float quantification = 0;
    
    switch(mode)
    {
      case 1:
      {
        this->_stopFlag = false;
        this->_startTime = millis();
        this->_intervalTime = (CALIBRATIONTIME*1000);
        Serial.println(F("Calibration starting..."));
      }
      break;
      case 2: 
      {
        receivedBufferPtr=strstr(this->_receivedBuffer, "SETCAL:");
        receivedBufferPtr+=strlen("SETCAL:");
        quantification = strtod(receivedBufferPtr,NULL);
        Serial.print(F("Quantification:"));
        Serial.println(quantification);
        this->_flowRate = quantification/float(CALIBRATIONTIME);
        EEPROM_write(FLOWRATEADDRESS, this->_flowRate);
        delay(10);
        EEPROM_write(PUMPSPEEDADDRESS, this->_pumpSpeed);
        Serial.print(F("PumpSpeed:"));
        Serial.println(this->_pumpSpeed);
        Serial.print(F("FlowRate:"));
        Serial.print(this->_flowRate);
        Serial.println(F("ml/s,\r\nCalibration Finish!"));
      }
      break;
      case 3: 
      {
        EEPROM_read(PHVALUEADDR+24, quantification);
        Serial.print(F("Quantification:"));
        Serial.println(quantification);
        this->_flowRate = quantification/float(CALIBRATIONTIME);
        EEPROM_write(FLOWRATEADDRESS, this->_flowRate);
        delay(10);
        EEPROM_write(PUMPSPEEDADDRESS, this->_pumpSpeed);
        Serial.print(F("PumpSpeed:"));
        Serial.println(this->_pumpSpeed);
        Serial.print(F("FlowRate:"));
        Serial.print(this->_flowRate);
        Serial.println(F("ml/s,\r\nCalibration Finish!"));
      }
      break;
    }
}