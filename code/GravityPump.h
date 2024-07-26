#ifndef _GRAVITYPUMP_H_
#define _GRAVITYPUMP_H_
//#include <Servo.h>
#include <ESP32Servo.h>
#include <Arduino.h>

#define RECEIVEDBUFFERLENGTH 20

class GravityPump
{
public:
    GravityPump();
    ~GravityPump();

    void update();                          //get the state from system, need to be put in the loop.
    void setPin(int pin);                   //set the pin for GravityPump.
    void calFlowRate(int speed = 180);      //Calibration function.the speed parameter is running speed what you needed.
                                            //please input the "STARTCAL" in serial to start cal
                                            //Pump some liquid in some secs
                                            //please input the actual mumber in serial by "SETCAL:XX"
                                            //cal end
    void pumpDriver(int speed, unsigned long runTime); //the basic pump function, have to given speed in number(0 to 180. 90 for stop,
                                                       //0 and 180 is max speed in each direction.)and runing time in milliseced.
    float timerPump(unsigned long runTime);            //timer pump function,base on the basic function.the function need to  given the running time then the pump will dosing as long as your have given.
                                                       //and return the quantitation. if you have Calibration,  the number will be close to result.
    float flowPump(float quantitation);                //quantification setting pump function,base on the basic function.the function need to given a quantification. Then the pump will dosing the quantification
                                                       //in given number. if you have Calibration, the number will be close to result.
    void getFlowRateAndSpeed();                        //flowrate and speed reading from EEPROM
    void stop();                                       //stop function. whenever you use this function the pump will stop immediately.
    void pumpCalibration(byte mode);
    
  private:
    Servo _pumpServo;
    int _pin;
    bool _runFlag = false;
    bool _stopFlag = false;
    int _pumpSpeed = 160;
    float _flowRate = 0.6; //default flow rate
    unsigned long _startTime = 0;
    unsigned long _intervalTime = 0;
    const int _servoStop = 90;
    char _receivedBuffer[RECEIVEDBUFFERLENGTH]; // store the serial command
    byte _receivedBufferIndex = 0;

  private:
    byte uartParse();
    bool serialDataAvailable();
    //void pumpCalibration(byte mode);
};


#endif
