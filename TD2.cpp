/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"
#include "QEI.h"
#include <cstdio>
#define PI 3.14159265359
#define GEAR_RATIO 10.9
#define WHEEL_RADIUS 0.04
#define DISTANCE_BETWEEN_WHEELS 0.1566
#define COUNTS_PER_REVOLUTION 256

#define SAMPLE_TIME_S 0.01f
#define SAMPLE_TIME_US 10000

Serial pc(USBTX, USBRX, 9600); //for PC debugging 
Serial hm10(PA_11, PA_12, 9600); //BLE module
DigitalOut led(LED2);
char c;

Ticker controlTicker; 
volatile bool controlFlag = false; 
void controlISR(){controlFlag = true;}

class MotorDriveBoard{
    private:
        PwmOut speed_left;
        PwmOut speed_right;

        DigitalOut mode_left;
        DigitalOut mode_right;

        DigitalOut direction_left;
        DigitalOut direction_right;

        DigitalOut enable;

        float period_left;
        float period_right;


    public:
        MotorDriveBoard(PinName sl, PinName sr, PinName ml, PinName mr, PinName dl, PinName dr, PinName e) : speed_left(sl), speed_right(sr), mode_left(ml), mode_right(mr), direction_left(dl), direction_right(dr), enable(e){
            period_left = period_right = 0;
        }
        // SET FUNCTIONS
        void SetDutyCycle(float l, float r){
            speed_left.write(1-l);
            speed_right.write(1-r);
        }
        void SetPeriod(float l, float r){
            speed_left.period(l);
            speed_right.period(r); //in seconds

            period_left = l;
            period_right = r;
        }
        void SetDirection(float l, float r){
            direction_left.write(l);
            direction_right.write(r);
        }
        void SetMode(int l, int r){
            mode_left.write(l);
            mode_right.write(r);
        }
        void SetEnable(int e){
            enable = e;
        }
        // GET FUNCTIONS
        float GetDutyCycleLeft(){
            return speed_left.read();
        }
        float GetDutyCycleRight(){
            return speed_right.read();
        }
        float GetPeriodLeft(){
            return period_left;
        }
        float GetPeriodRight(){
            return period_right;
        }
        int GetModeLeft(){
            return mode_left.read();
        }
        int GetModeRight(){
            return mode_right.read();
        }
        int GetDirectionLeft(){
            return direction_left.read();
        }
        int GetDirectionRight(){
            return direction_right.read();
        }
};
class SensorArray{
    private:
        DigitalOut uln1, uln2, uln3, uln4, uln5, uln6;
        DigitalOut tr1, tr2, tr3, tr4, tr5, tr6;
    public:
        SensorArray(PinName u1, PinName u2, PinName u3, PinName u4, PinName u5, PinName u6, PinName t1, PinName t2, PinName t3, PinName t4, PinName t5, PinName t6):
        uln1(u1), uln2(u2), uln3(u3), uln4(u4), uln5(u5), uln6(u6), tr1(t1), tr2(t2), tr3(t3), tr4(t4), tr5(t5), tr6(t6){}


        //SET FUNCTIONS
        void SetLEDs(int u1, int u2, int u3, int u4, int u5, int u6){
            uln1.write(u1);
            uln2.write(u2);
            uln3.write(u3);
            uln4.write(u4);
            uln5.write(u5);
            uln6.write(u6);
        }

        //GET FUNCTIONS
        int GetLED1(){
            return uln1;
        }
        //TASK 4 : SHOW MCE CAN OUTPUT WHAT THE SENSORS SEE
        void SensorOutputs(){
            float output1, output2, output3, output4, output5, output6;
            output1 = tr1.read();
            output2 = tr2.read();
            output3 = tr3.read();
            output4 = tr4.read();
            output5 = tr5.read();
            output6 = tr6.read();
            printf("TCRT 1 : %.2f, TCRT 2 : %.2f, TCRT 3 : %.2f, TCRT 4 : %.2f, TCRT 5 : %.2f, TCRT 6 : %.2f\n", output1, output2, output3, output4, output5, output6);
        }              
};
//SET UP HM-10 VIA USB SERIAL PORT
void SerialConfig(){
    //type into the serial monitor to configure
    char w;
    if (pc.readable()){
        w = pc.getc();
        hm10.putc(w);
    }
}
//TASK 2 : SHOW WE CAN MEASURE VELOCITY VIA ENCODERS
void MeasureWheelVelocity(QEI &leftWheel, QEI &rightWheel, MotorDriveBoard &motor){
    //static var to remember value between func calls
    static int prevTickLeft = 0;
    static int prevTickRight = 0;

    static float distance = 0.0f;
    //1 tick corresponds to ~0.09mm
    //therefore expect 5550 ticks for 0.5m
    float const metersPerTick = (2*PI*WHEEL_RADIUS)/(COUNTS_PER_REVOLUTION*GEAR_RATIO);
    if(controlFlag){
        //ticker interrupts at sample time seconds
        //when flag is true, it means we take another sample
        controlFlag = false;

        int currentTickLeft = leftWheel.getPulses();
        int currentTickRight = rightWheel.getPulses();

        int dL = currentTickLeft - prevTickLeft;
        int dR = currentTickRight - prevTickRight;

        prevTickLeft = currentTickLeft;
        prevTickRight = currentTickRight;

        float distanceLeft = dL*metersPerTick;
        float distanceRight = dR*metersPerTick;
        float distanceAvg = 0.5*(distanceLeft + distanceRight);

        float velocityLeft = distanceLeft/SAMPLE_TIME_S;
        float velocityRight = distanceRight/SAMPLE_TIME_S;

        distance += distanceAvg;
        //printf("Distance = %.2f\n", distance);
        printf("Velocity Left = %.2f, Velocity Right = %.2f, Distance = %.2f\n", abs(velocityLeft), abs(velocityRight), abs(distance*4));
        if(abs(distance*4) >= 4.0f){motor.SetDutyCycle(0,0);}
        //essentially code taken from TD1, but changed to calculate velocity, not tick count
    }
}

//TASK 5 : SHOW MCE RESPONDS TO BLE COMMAND FROM PHONE
void DisplayBLESignals(){
    if(hm10.readable()){
        c = hm10.getc();
        if(c == 'A'){
            led = 1;
        }
        else if(c == 'B'){
            led = 0;
        }
    }
}
int main()
{
    //Object Instantiation
    controlTicker.attach(&controlISR, SAMPLE_TIME_S);
    QEI leftWheel(PA_0, PA_1, NC, 256);
    QEI rightWheel(PC_8, PC_6, NC, 256);
    MotorDriveBoard motor(PB_4, PA_10, PA_8, PB_5, PB_10, PB_3, PB_8);

    SensorArray sensorArray(PA_5, PA_6, PA_7, PB_6, PC_7, PA_9, PC_2, PC_3, PA_4, PB_0, PC_1, PC_0);
    //Settings
    motor.SetMode(0,0); //unipolar
    motor.SetDutyCycle(0.3,0.3); // low duty cycle
    motor.SetDirection(0,0); //forward
    motor.SetPeriod(0.001,0.001);
    motor.SetEnable(1);

    sensorArray.SetLEDs(1, 1, 1, 1, 1, 1);
    while(1){
        MeasureWheelVelocity(leftWheel, rightWheel, motor);
        wait_us(1000);
        DisplayBLESignals();
        wait_us(1000);
        sensorArray.SensorOutputs();
        wait_us(1000);
    }

}
