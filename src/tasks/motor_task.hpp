#pragma once

#include "task_common.hpp"

// Start motor control task pinned to core 1
void motor_task_start();

void motor_get_encoder_status(EncoderTelemetry &out);
void motor_reset_encoder_counts();
