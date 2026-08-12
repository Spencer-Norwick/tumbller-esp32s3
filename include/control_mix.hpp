#pragma once

namespace control_mix {

inline float applySpeedCorrection(float balanceOutput, float speedLoopOutput, float speedMixScale) {
  return balanceOutput + (speedLoopOutput * speedMixScale);
}

inline bool isSaturated(float output, float limit) {
  return output > limit || output < -limit;
}

}  // namespace control_mix
