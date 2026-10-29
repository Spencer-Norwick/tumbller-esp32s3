#include <Arduino.h>
#include "Motor.hpp"
// #include "PinChangeInt.h"



void Motor::Encoder_init()
{
//   attachInterrupt(digitalPinToInterrupt(ENCODER_LEFT_A_PIN), EncoderCountLeftA, CHANGE);
//   attachPinChangeInterrupt(ENCODER_RIGHT_A_PIN, EncoderCountRightA, CHANGE);
    pinMode(ENCODER_LEFT_A_PIN, INPUT_PULLDOWN);
    pinMode(ENCODER_RIGHT_A_PIN, INPUT_PULLDOWN);
    attachInterrupt(ENCODER_LEFT_A_PIN, EncoderCountLeftA, RISING);
    attachInterrupt(ENCODER_RIGHT_A_PIN, EncoderCountRightA, RISING);
}

volatile unsigned long Motor::encoder_count_right_a = 0;
//Getting right wheel speed.
void Motor::EncoderCountRightA()
{
  Motor::encoder_count_right_a++;
}


volatile unsigned long Motor::encoder_count_left_a = 0;
//Getting left wheel speed.
void Motor::EncoderCountLeftA()
{
  Motor::encoder_count_left_a++;
}

void Motor::SnapshotEncoderCounts(unsigned long &leftCount, unsigned long &rightCount)
{
  noInterrupts();
  leftCount = encoder_count_left_a;
  rightCount = encoder_count_right_a;
  interrupts();
}

void Motor::ResetEncoderCounts()
{
  noInterrupts();
  encoder_count_left_a = 0;
  encoder_count_right_a = 0;
  interrupts();
}
