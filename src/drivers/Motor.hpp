#ifndef _MOTOR_H
#define _MOTOR_H

#include "../config.hpp"

class Motor
{
  public:
          Motor();
          
          void Pin_init();
          /*Measuring_speed*/
          void Encoder_init();
          static void EncoderCountRightA();
          static void EncoderCountLeftA();
          static void SnapshotEncoderCounts(unsigned long &leftCount, unsigned long &rightCount);
          static void ResetEncoderCounts();
          
          void (Motor::*MOVE[5])(int speed);
          void Stop(int speed);
          void Forward(int speed);
          void Back(int speed);
          void Left(int speed);
          void Right(int speed);
          void LeftOnlyHigh(int speed);
          void LeftOnlyLow(int speed);
          void RightOnlyHigh(int speed);
          void RightOnlyLow(int speed);
          void DriveSigned(int leftPwm, int rightPwm);

  public:
          static volatile unsigned long encoder_count_right_a;
          static volatile unsigned long encoder_count_left_a;

  };







#endif
