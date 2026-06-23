#include "KalmanPitch.hpp"

namespace {
constexpr float Q_ANGLE = 0.001f;
constexpr float Q_GYRO = 0.005f;
constexpr float R_ANGLE = 0.5f;
constexpr float C_0 = 1.0f;
}  // namespace

void KalmanPitch::reset(float initialAngleDeg) {
  _angle = initialAngleDeg;
  _angleRate = 0.0f;
  _gyroBias = 0.0f;
  _p[0][0] = 1.0f;
  _p[0][1] = 0.0f;
  _p[1][0] = 0.0f;
  _p[1][1] = 1.0f;
}

void KalmanPitch::update(float measuredAngleDeg, float gyroRateDps, float dtSeconds) {
  _angle += (gyroRateDps - _gyroBias) * dtSeconds;
  const float angleError = measuredAngleDeg - _angle;

  const float pdot0 = Q_ANGLE - _p[0][1] - _p[1][0];
  const float pdot1 = -_p[1][1];
  const float pdot2 = -_p[1][1];
  const float pdot3 = Q_GYRO;

  _p[0][0] += pdot0 * dtSeconds;
  _p[0][1] += pdot1 * dtSeconds;
  _p[1][0] += pdot2 * dtSeconds;
  _p[1][1] += pdot3 * dtSeconds;

  const float pct0 = C_0 * _p[0][0];
  const float pct1 = C_0 * _p[1][0];
  const float e = R_ANGLE + C_0 * pct0;
  const float k0 = pct0 / e;
  const float k1 = pct1 / e;

  const float t0 = pct0;
  const float t1 = C_0 * _p[0][1];
  _p[0][0] -= k0 * t0;
  _p[0][1] -= k0 * t1;
  _p[1][0] -= k1 * t0;
  _p[1][1] -= k1 * t1;

  _angle += k0 * angleError;
  _gyroBias += k1 * angleError;
  _angleRate = gyroRateDps - _gyroBias;
}
