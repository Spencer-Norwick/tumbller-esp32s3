#pragma once

namespace control_mix {

inline float applySpeedCorrection(float balanceOutput, float speedLoopOutput, float speedMixScale) {
  return balanceOutput + (speedLoopOutput * speedMixScale);
}

}  // namespace control_mix
