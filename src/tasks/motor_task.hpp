#pragma once

#include "task_common.hpp"

struct MotorAppliedTelemetry {
  bool initialized = false;
  unsigned long updatedAtMs = 0;
  uint32_t sequence = 0;
  int leftPwm = 0;
  int rightPwm = 0;
  unsigned long timeoutMs = 0;
  bool running = false;
};

// Start motor control task pinned to core 1
void motor_task_start();

void motor_get_encoder_status(EncoderTelemetry &out);
void motor_reset_encoder_counts();
void motor_snapshot_encoder_counts(unsigned long &left, unsigned long &right);
void motor_get_applied_status(MotorAppliedTelemetry &out);
