#pragma once

class KalmanPitch {
 public:
  void reset(float initialAngleDeg = 0.0f);
  void update(float measuredAngleDeg, float gyroRateDps, float dtSeconds);

  float angleDeg() const { return _angle; }
  float rateDps() const { return _angleRate; }
  float gyroBiasDps() const { return _gyroBias; }

 private:
  float _angle = 0.0f;
  float _angleRate = 0.0f;
  float _gyroBias = 0.0f;
  float _p[2][2] = {{1.0f, 0.0f}, {0.0f, 1.0f}};
};
