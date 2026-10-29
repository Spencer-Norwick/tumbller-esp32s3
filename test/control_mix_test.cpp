#include <cassert>
#include <cmath>

#include "control_mix.hpp"

namespace {
bool nearlyEqual(float left, float right) {
  return std::fabs(left - right) < 0.0001f;
}
}  // namespace

int main() {
  assert(nearlyEqual(control_mix::applySpeedCorrection(60.0f, -100.0f, 0.05f), 55.0f));
  assert(nearlyEqual(control_mix::applySpeedCorrection(-60.0f, 100.0f, 0.05f), -55.0f));
  assert(nearlyEqual(control_mix::applySpeedCorrection(25.0f, 0.0f, 0.05f), 25.0f));
  assert(control_mix::isSaturated(90.1f, 90.0f));
  assert(control_mix::isSaturated(-90.1f, 90.0f));
  assert(!control_mix::isSaturated(90.0f, 90.0f));
  assert(!control_mix::isSaturated(-90.0f, 90.0f));
  return 0;
}
