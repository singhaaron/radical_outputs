//Preprocessor Directives
#include <stdio.h>    //Input&Output
#include <stdlib.h>   //Malloc,Atoi(converting string commandLine argv[2] into a integer)
#include <pthread.h>  //Threading
#include <string.h>   //MemSet,Strcopy,STRMP
#include <time.h>     //Time
#include <unistd.h>   // Close(),Read()
#include <fcntl.h>    //Open(),O_Rdly(flags)
#include <stdbool.h>  //Bool
#include <signal.h>   //Interupt
#include <wiringPi.h> //Pins
#include <softPwm.h>  //Power Output
#include <ncurses.h> //KeyPress Library
#include <math.h>

//white vcc
//purple gnd

//yellow vcc
//
//Motor Macros
//MOTOR-1
#define F_MOT_A 3
#define R_MOT_A 2
#define VOLT_MOT_A 0
//MOTOR-2
#define F_MOT_B 4
#define R_MOT_B 5
#define VOLT_MOT_B 6
//MOTOR-3
#define F_MOT_C 13
#define R_MOT_C 14
#define VOLT_MOT_C 12
//MOTOR-4
#define F_MOT_D 11
#define R_MOT_D 10
#define VOLT_MOT_D 26

// Sensors
#define ECHO_SENSOR_TRIGGER 21
#define ECHO_SENSOR_RECEIVER 22
#define CLOSE_RANGE_SENSOR 7
#define SERVO_TRIGGER 15
#define DISTANCE_THRESHOLD 15.0

//Direction
#define LINE_LEFT_PIN 7
#define LINE_MIDDLE_PIN 16
#define LINE_RIGHT_PIN 1
#define LINE_BOTTOM_PIN 8
enum direction
{
    None,
    Forward,
    Backward,
    Left,
    LeftLeft,
    LeftLeftLeft,
    Right,
    RightRight,
    RightRightRight,
    Repeat,
    Offline
};
//Matrix constants
#define _ 1
#define L 0
#define M 0
#define R 0
#define B 0
#define LEFT 2
#define MIDDLE 2
#define RIGHT 2
#define BOTTOM 2

enum direction lineMatrix[LEFT][MIDDLE][RIGHT][BOTTOM] = {
    [_][_][_][_] = Offline,            // 0
    [L][_][R][_] = Forward,            // 6
    [L][M][R][_] = Repeat,            //11
    [L][_][R][B] = Repeat,            //13
    [_][M][_][_] = Forward,         // 2
    [_][M][_][B] = Forward,         // 9
    [L][M][R][B] = Forward,         //15
    [_][_][_][B] = Backward,        // 4
    [L][_][_][_] = Left,            // 1
    [L][_][_][B] = LeftLeft,        // 7
    [_][M][R][_] = RightRight,        // 8
    [L][M][_][B] = LeftLeftLeft,    //12
    [_][_][R][_] = Right,           // 3
    [L][M][_][_] = LeftLeft,      // 5
    [_][_][R][B] = RightRight,      //10
    [_][M][R][B] = RightRightRight, //14
};

//Global Variables
bool haltProgram = false;                                  //Terminate All
enum direction lineDirection;                             //Drive Direction
enum direction obstacleDirection;
enum direction driveDirection;
bool isOffline = false;
bool ninetyDegLeft, ninetyDegRight, threesixtyDeg = false; //Static Turns
bool isBlockedByObstacle = false;                          //Obstacle Blocking
bool haveWaitedForObstacle = false;                        //Obstacle Sensing
bool lineControl = true;                                   //Line Sensing

//Wiring Pi Starter
static void setup()
{
    printf("setting up the program!\n");
    pinMode(F_MOT_A, SOFT_PWM_OUTPUT);
    pinMode(R_MOT_A, SOFT_PWM_OUTPUT);
    pinMode(VOLT_MOT_A, SOFT_PWM_OUTPUT);
    softPwmCreate(F_MOT_A, 0, 100);
    softPwmCreate(R_MOT_A, 0, 100);
    softPwmCreate(VOLT_MOT_A, 0, 100);
    //B
    pinMode(F_MOT_B, SOFT_PWM_OUTPUT);
    pinMode(R_MOT_B, SOFT_PWM_OUTPUT);
    pinMode(VOLT_MOT_B, SOFT_PWM_OUTPUT);
    softPwmCreate(F_MOT_B, 0, 100);
    softPwmCreate(R_MOT_B, 0, 100);
    softPwmCreate(VOLT_MOT_B, 0, 100);
    //C
    pinMode(F_MOT_C, SOFT_PWM_OUTPUT);
    pinMode(R_MOT_C, SOFT_PWM_OUTPUT);
    pinMode(VOLT_MOT_C, SOFT_PWM_OUTPUT);
    softPwmCreate(F_MOT_C, 0, 100);
    softPwmCreate(R_MOT_C, 0, 100);
    softPwmCreate(VOLT_MOT_C, 0, 100);
    //D
    pinMode(F_MOT_D, SOFT_PWM_OUTPUT);
    pinMode(R_MOT_D, SOFT_PWM_OUTPUT);
    pinMode(VOLT_MOT_D, SOFT_PWM_OUTPUT);
    softPwmCreate(F_MOT_D, 0, 100);
    softPwmCreate(R_MOT_D, 0, 100);
    softPwmCreate(VOLT_MOT_D, 0, 100);

    // Sensors
    pinMode(ECHO_SENSOR_TRIGGER, OUTPUT);
    pinMode(ECHO_SENSOR_RECEIVER, INPUT);
    pinMode(CLOSE_RANGE_SENSOR, INPUT);

    // Servo
    pinMode(SERVO_TRIGGER, SOFT_PWM_OUTPUT);
    pullUpDnControl(SERVO_TRIGGER, PUD_OFF);
    softPwmCreate(SERVO_TRIGGER, 0, 50);
    softPwmWrite(SERVO_TRIGGER, 13);

    // Line sensor
    pinMode(LINE_LEFT_PIN, INPUT);
    pinMode(LINE_MIDDLE_PIN, INPUT);
    pinMode(LINE_RIGHT_PIN, INPUT);
    pinMode(LINE_BOTTOM_PIN, INPUT);
}

static void allOff()
{
    digitalWrite(F_MOT_A, LOW);
    digitalWrite(R_MOT_A, LOW);
    digitalWrite(VOLT_MOT_A, LOW);
    digitalWrite(F_MOT_B, LOW);
    digitalWrite(R_MOT_B, LOW);
    digitalWrite(VOLT_MOT_B, LOW);
    digitalWrite(F_MOT_C, LOW);
    digitalWrite(R_MOT_C, LOW);
    digitalWrite(VOLT_MOT_C, LOW);
    digitalWrite(F_MOT_D, LOW);
    digitalWrite(R_MOT_D, LOW);
    digitalWrite(VOLT_MOT_D, LOW);
}
static void interruptHandlers(const int signals)
{
    printf("turning everything off!\n");
    allOff();
    exit(0);
}
//Motor Function
void *runMotor(void *u)
{   
    do
    {
        //Motor Directions
        if (driveDirection == Left)
        {
            // printf("Should be driving left!\n");
            softPwmWrite(VOLT_MOT_A, 50);
            softPwmWrite(VOLT_MOT_B, 50);
            softPwmWrite(VOLT_MOT_C, 50);
            softPwmWrite(VOLT_MOT_D, 50);
            softPwmWrite(F_MOT_A, 0);
            softPwmWrite(R_MOT_A, 100);
            softPwmWrite(F_MOT_B, 100);
            softPwmWrite(R_MOT_B, 0);
            softPwmWrite(F_MOT_C, 0);
            softPwmWrite(R_MOT_C, 100);
            softPwmWrite(F_MOT_D, 100);
            softPwmWrite(R_MOT_D, 0);
        }
        else if (driveDirection == LeftLeft)
        {
            // printf("Should be driving leftleft!\n");
            softPwmWrite(VOLT_MOT_A, 60);
            softPwmWrite(VOLT_MOT_B, 60);
            softPwmWrite(VOLT_MOT_C, 60);
            softPwmWrite(VOLT_MOT_D, 60);
            softPwmWrite(F_MOT_A, 0);
            softPwmWrite(R_MOT_A, 100);
            softPwmWrite(F_MOT_B, 100);
            softPwmWrite(R_MOT_B, 0);
            softPwmWrite(F_MOT_C, 0);
            softPwmWrite(R_MOT_C, 100);
            softPwmWrite(F_MOT_D, 100);
            softPwmWrite(R_MOT_D, 0);
        }
        else if (driveDirection == None){
            // printf("Should be driving... NOT!\n");
            softPwmWrite(VOLT_MOT_A, 0);
            softPwmWrite(VOLT_MOT_B, 0);
            softPwmWrite(VOLT_MOT_C, 0);
            softPwmWrite(VOLT_MOT_D, 0);
            softPwmWrite(F_MOT_A, 0);
            softPwmWrite(R_MOT_A, 0);
            softPwmWrite(F_MOT_B, 0);
            softPwmWrite(R_MOT_B, 0);
            softPwmWrite(F_MOT_C, 0);
            softPwmWrite(R_MOT_C, 0);
            softPwmWrite(F_MOT_D, 0);
            softPwmWrite(R_MOT_D, 0);
        }
        else if (driveDirection == Right)
        {
            // printf("Should be driving Right!\n");
            softPwmWrite(VOLT_MOT_A, 50);
            softPwmWrite(VOLT_MOT_B, 50);
            softPwmWrite(VOLT_MOT_C, 50);
            softPwmWrite(VOLT_MOT_D, 50);
            softPwmWrite(F_MOT_A, 100);
            softPwmWrite(R_MOT_A, 0);
            softPwmWrite(F_MOT_B, 0);
            softPwmWrite(R_MOT_B, 100);
            softPwmWrite(F_MOT_C, 100);
            softPwmWrite(R_MOT_C, 0);
            softPwmWrite(F_MOT_D, 0);
            softPwmWrite(R_MOT_D, 100);
        }
        else if (driveDirection == RightRight)
        {
            // printf("Should be driving Right!\n");
            softPwmWrite(VOLT_MOT_A, 60);
            softPwmWrite(VOLT_MOT_B, 60);
            softPwmWrite(VOLT_MOT_C, 60);
            softPwmWrite(VOLT_MOT_D, 60);
            softPwmWrite(F_MOT_A, 100);
            softPwmWrite(R_MOT_A, 0);
            softPwmWrite(F_MOT_B, 0);
            softPwmWrite(R_MOT_B, 100);
            softPwmWrite(F_MOT_C, 100);
            softPwmWrite(R_MOT_C, 0);
            softPwmWrite(F_MOT_D, 0);
            softPwmWrite(R_MOT_D, 100);
        }
        else if (driveDirection == Forward)
        {
            //All Motors Charge Forward
            // printf("Should be driving Forward!\n");
            softPwmWrite(VOLT_MOT_A, 20);
            softPwmWrite(VOLT_MOT_B, 20);
            softPwmWrite(VOLT_MOT_C, 20);
            softPwmWrite(VOLT_MOT_D, 20);
            softPwmWrite(F_MOT_A, 100);
            softPwmWrite(R_MOT_A, 0);
            softPwmWrite(F_MOT_B, 100);
            softPwmWrite(R_MOT_B, 0);
            softPwmWrite(F_MOT_C, 100);
            softPwmWrite(R_MOT_C, 0);
            softPwmWrite(F_MOT_D, 100);
            softPwmWrite(R_MOT_D, 0);
        }
        else if (driveDirection == Backward)
        {
            //All Motors Charge Backward
            // printf("Should be driving Backwards!\n");
            softPwmWrite(VOLT_MOT_A, 20);
            softPwmWrite(VOLT_MOT_B, 20);
            softPwmWrite(VOLT_MOT_C, 20);
            softPwmWrite(VOLT_MOT_D, 20);
            softPwmWrite(F_MOT_A, 0);
            softPwmWrite(R_MOT_A, 100);
            softPwmWrite(F_MOT_B, 0);
            softPwmWrite(R_MOT_B, 100);
            softPwmWrite(F_MOT_C, 0);
            softPwmWrite(R_MOT_C, 100);
            softPwmWrite(F_MOT_D, 0);
            softPwmWrite(R_MOT_D, 100);
        }
        //Static Turns
        else if (driveDirection == LeftLeftLeft)
        {
            // printf("Should be driving LEEEEFT!\n");
            softPwmWrite(VOLT_MOT_A, 25);
            softPwmWrite(VOLT_MOT_B, 40);
            softPwmWrite(VOLT_MOT_C, 50);
            softPwmWrite(VOLT_MOT_D, 40);
            softPwmWrite(R_MOT_A, 10);
            softPwmWrite(F_MOT_B, 75);
            softPwmWrite(R_MOT_C, 25);
            softPwmWrite(F_MOT_D, 75);

        }

        else if (driveDirection == RightRightRight)
        {
            // printf("Should be driving RIIIIIGHHT!\n");
            softPwmWrite(VOLT_MOT_A, 40);
            softPwmWrite(VOLT_MOT_B, 10);
            softPwmWrite(VOLT_MOT_C, 40);
            softPwmWrite(VOLT_MOT_D, 10);
            softPwmWrite(F_MOT_A, 75);
            softPwmWrite(R_MOT_B, 25);
            softPwmWrite(F_MOT_C, 75);
            softPwmWrite(R_MOT_D, 50);
        }
        else if (threesixtyDeg)
        {
            // printf("AAHHHHH I DON't KNOW WHAT IM DOING!!\n");
            softPwmWrite(VOLT_MOT_A, 100);
            softPwmWrite(VOLT_MOT_B, 100);
            softPwmWrite(VOLT_MOT_C, 100);
            softPwmWrite(VOLT_MOT_D, 100);
            softPwmWrite(F_MOT_A, 100);
            softPwmWrite(F_MOT_C, 100);
            softPwmWrite(R_MOT_B, 100);
            softPwmWrite(R_MOT_D, 100);
            usleep(1000000); //1sec
            softPwmWrite(F_MOT_A, 0);
            softPwmWrite(R_MOT_A, 0);
            softPwmWrite(F_MOT_B, 0);
            softPwmWrite(R_MOT_B, 0);
            softPwmWrite(F_MOT_C, 0);
            softPwmWrite(R_MOT_C, 0);
            softPwmWrite(F_MOT_D, 0);
            softPwmWrite(R_MOT_D, 0);
        }

    } while (!haltProgram);
    return NULL;
}

// Obstacle Sensing
float objectDistance = 0.0;
pthread_mutex_t obstacleMutex = PTHREAD_MUTEX_INITIALIZER;
float echoSensorDistance() {
    digitalWrite(ECHO_SENSOR_TRIGGER, HIGH);
    usleep(10);
    digitalWrite(ECHO_SENSOR_TRIGGER, LOW);

    clock_t start = clock();
    clock_t stop = clock();

    // As long as the echo pin reads 0, keep resetting the start time.
    // Once the echo pin reads 1, we got a hit, so we should start the timer
    // only once the pin switches.
    while (digitalRead(ECHO_SENSOR_RECEIVER) == 0) {
        start = clock();
    }

    while (digitalRead(ECHO_SENSOR_RECEIVER) == 1) {
        stop = clock();
    }

    double timeDifference = ((double) (stop - start)) / CLOCKS_PER_SEC;
    objectDistance = (((float) timeDifference * 340) / 2) * 100;
    printf("\rObject distance = %f", objectDistance);
    fflush(stdout);
    return objectDistance;
}

//returns 0 if delay was successful with no breaks
//returns 1 if delay was broken
int delayWithCheck(int delayAmount, int delayIncrement, bool flag, bool breakIfIm) {
    int steps = delayAmount / delayIncrement;
    for( int i = 0; i <= steps; i++) {
        if(flag == breakIfIm) {
            return 1;
        }
        else {
            delay(delayIncrement);
        }
    }
    return 0;
}

void moveAroundObstacle() {
    // Turn the echo sensor fully to the left.
    // Hard turn to the left, in place about 90 degrees.
    // This will depend on how far the servo allows the sensor to rotate.

    // Turning around the obstacle until we get back to the line
    // pthread_mutex_lock(&obstacleMutex);
    for(int i = 13; i >= 5; i--){
        softPwmWrite(SERVO_TRIGGER, i);
        delay(100);
    }
    obstacleDirection = LeftLeft;
    delay(500);
    obstacleDirection = None;
    delay(1000);
    obstacleDirection = Forward;
    delay(1750);
    obstacleDirection = None;
    delay(1000);
    obstacleDirection = RightRight;
    delay(450);
    obstacleDirection = None;
    delay(1000);
    for(int i = 5; i <= 13; i++){
        softPwmWrite(SERVO_TRIGGER, i);
        delay(100);
    }
    while(true) {
        obstacleDirection = Forward;
        if(delayWithCheck(2000, 50, isOffline, false)){
            break;
        }
        obstacleDirection = None;
        if(delayWithCheck(1000, 50, isOffline, false)){
            break;
        }
        obstacleDirection = RightRight;
        if(delayWithCheck(450, 50, isOffline, false)){
            break;
        }
        obstacleDirection = None;
        if(delayWithCheck(1000, 50, isOffline, false)){
            break;
        }
        obstacleDirection = Forward;
        if(delayWithCheck(1600, 50, isOffline, false)){
            break;
        }
        obstacleDirection = None;
        if(delayWithCheck(1000, 50, isOffline, false)){
            break;
        }
    }
    // pthread_mutex_unlock(&obstacleMutex);
    // Center the echo sensor.
    // softPwmWrite(SERVO_TRIGGER, 13);
    
}

void* checkSensors(void* args) {
    // printf("check sensors");
    // fflush(stdout);
    while (!haltProgram) {
        // pthread_mutex_lock(&obstacleMutex);
        if (echoSensorDistance() <= DISTANCE_THRESHOLD) {
            isBlockedByObstacle = true;
            printf("sensed obstacle\n");
            fflush(stdout);
            lineControl = false;
            obstacleDirection = None;
            // Wait 5 seconds to check if the obstacle has moved.
            if (!haveWaitedForObstacle) {
                printf("waiting on obstacle timer\n");
                fflush(stdout);
                delay(1000);
                haveWaitedForObstacle = true;
            } else {
                printf("moving around obstacle\n");
                fflush(stdout);
                moveAroundObstacle();
                lineControl = true;
                isBlockedByObstacle = false;
                haveWaitedForObstacle = false;
                printf("giving control back to line control\n");
                fflush(stdout);
            }
        } else {
            isBlockedByObstacle = false;
            haveWaitedForObstacle = false;
            lineControl = true;
        }
        // pthread_mutex_unlock(&obstacleMutex);
    }
    pthread_exit(0);
}

bool isRunning = true;
void* lineSensorThread(void* arg) {
  enum direction lineDirectionTemp;
  while( isRunning ) {
    // pthread_mutex_lock(&obstacleMutex);
    lineDirectionTemp = lineMatrix
                          [digitalRead(LINE_LEFT_PIN)]
                          [digitalRead(LINE_MIDDLE_PIN)]
                          [digitalRead(LINE_RIGHT_PIN)]
                          [digitalRead(LINE_BOTTOM_PIN)];
    if (lineDirectionTemp == Repeat){
    }
    else if(lineDirectionTemp == Offline) {
        lineDirection = None;
        isOffline = true;
    }
    else{
        lineDirection = lineDirectionTemp;
        isOffline = false;
    }
    // pthread_mutex_unlock(&obstacleMutex);
    //   printf("\r %d %d %d %d",digitalRead(LINE_LEFT_PIN),
    //                       digitalRead(LINE_MIDDLE_PIN),
    //                       digitalRead(LINE_RIGHT_PIN),
    //                       digitalRead(LINE_BOTTOM_PIN));
    // fflush(stdout);
  }
  return NULL;
}

pthread_t lineSensorPID;
void startLineSensorThread() {
  pthread_create( &lineSensorPID, NULL, lineSensorThread, NULL );
}

void stopLineSensorThread() {
  isRunning = false;
  pthread_join( lineSensorPID,  NULL );
}

void* lineORobstacle(void* args) {
    int flagTemp = 0;
    int flag = 0;
    while(!haltProgram) {
        //debugging flags
        
        if(!lineControl) {
            //start debugging statements
            flag = 0;
            if(flagTemp != flag) {
                flagTemp = flag;
                printf("getting direction from obstacle direction\n");
                fflush(stdout);
            }
            //end debugging statements

            driveDirection = obstacleDirection;
        }
        else {
            //start debugging statements
            flag = 1;
            if(flagTemp != flag) {
                flagTemp = flag;
                printf("getting direction from line direction\n");
                fflush(stdout);
            }
            //end debugging statements

            driveDirection = lineDirection;
        }
    }
    return NULL;
}

int main()
{
    signal(SIGINT, interruptHandlers);
	if(-1 == wiringPiSetup()){                     //check if wiring pi 
							//failed
		printf("Failed to setup Wiring Pi!\n");
		return 1;
	}
    setup();
    pthread_t MotorThread;
    pthread_create(&MotorThread, NULL, &runMotor, NULL);
    startLineSensorThread();
    // // Test Obstacle Sensor
    pthread_t obstacleThread;
    pthread_create(&obstacleThread, NULL, checkSensors, NULL);
    pthread_t lineORobstacleThread;
    pthread_create(&lineORobstacleThread, NULL, lineORobstacle, NULL);
    pthread_join(MotorThread, NULL); //Main Thread waits for the p1 thread to terminate before continuing main exeuction
    pthread_join(obstacleThread, NULL);
    pthread_join(lineORobstacleThread, NULL);
    stopLineSensorThread();
    // haltProgram = false;
    // End Test

    return 0;
}