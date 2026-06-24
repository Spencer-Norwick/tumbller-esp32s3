// HTTP server task: listens on port 80 and dispatches /info, /sensor/ht, and /motor routes.
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <SensirionI2cSht3x.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "task_common.hpp"
#include "balance_task.hpp"
#include "server_task.hpp"

static WiFiServer server(80);
static constexpr unsigned long CLIENT_HEADER_TIMEOUT_MS = 250;
static constexpr size_t MAX_HEADER_BYTES = 1536;
static void serverTask(void *pvParameters);
static void sendJson(WiFiClient &client, const char *status, const String &body);
static const char *jsonBool(bool value);
static String hexByte(uint8_t value);
static bool handleInfoRequest(WiFiClient &client, const String &header);
static bool handleI2cScanRequest(WiFiClient &client, const String &header);
static bool handleImuRawRequest(WiFiClient &client, const String &header);
static bool handleBalanceConfigRequest(WiFiClient &client, const String &header);
static bool handleBalanceArmRequest(WiFiClient &client, const String &header);
static bool handleBalanceDisarmRequest(WiFiClient &client, const String &header);
static bool handleBalanceStatusRequest(WiFiClient &client, const String &header);
static bool handleBalanceCalibrateRequest(WiFiClient &client, const String &header);
static bool handleSensorRequest(WiFiClient &client, const String &header);
static bool handleMotorRequest(WiFiClient &client, const String &header);
static bool queryFloatParam(const String &header, const char *name, float &value);
static float clampConfigValue(float value, float minValue, float maxValue);

void server_task_start() {
  // Launch a dedicated server task pinned to core 0
  server.begin();
  xTaskCreatePinnedToCore(serverTask, "serverTask", 8192, nullptr, 1, nullptr, 0);
}

static void serverTask(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    // Blink heartbeat LED once per second to show the task is alive
    {
      static unsigned long lastBlinkMs = 0;
      unsigned long now = millis();
      if (now - lastBlinkMs >= 1000) {
        lastBlinkMs = now;
        digitalWrite(LED_BLUE, !digitalRead(LED_BLUE));
      }
    }

    WiFiClient client = server.available();   // Listen for incoming clients
    if (!client) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    client.setTimeout(CLIENT_HEADER_TIMEOUT_MS);
    unsigned long currentTime = millis();
    unsigned long previousTime = currentTime;
#if defined(USE_SERIAL) && defined(HTTP_SERIAL_DEBUG)
    Serial.println("New Client.");
#endif
        String currentLine = ""; // collect the current header line
        String header;            // full HTTP header buffer
    while (client.connected() && currentTime - previousTime <= CLIENT_HEADER_TIMEOUT_MS) {
      currentTime = millis();
      if (client.available()) {
        char c = client.read();
#if defined(USE_SERIAL) && defined(HTTP_SERIAL_DEBUG)
        Serial.write(c);
#endif
        header += c;
        if (header.length() > MAX_HEADER_BYTES) {
          client.println("HTTP/1.1 431 Request Header Fields Too Large");
          client.println("Connection: close");
          client.println();
          break;
        }
        if (c == '\n') {
          if (currentLine.length() == 0) {
            // End of headers: dispatch by path (info → JSON, sensor → JSON, motor → HTML)
            if (handleInfoRequest(client, header)) break;
            if (handleI2cScanRequest(client, header)) break;
            if (handleImuRawRequest(client, header)) break;
            if (handleBalanceConfigRequest(client, header)) break;
            if (handleBalanceArmRequest(client, header)) break;
            if (handleBalanceDisarmRequest(client, header)) break;
            if (handleBalanceCalibrateRequest(client, header)) break;
            if (handleBalanceStatusRequest(client, header)) break;
            if (handleSensorRequest(client, header)) break;
            if (handleMotorRequest(client, header)) break;

            // Unknown route
            client.println("HTTP/1.1 404 Not Found");
            client.println("Connection: close");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      } else {
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
    header = "";
    client.stop();
#if defined(USE_SERIAL) && defined(HTTP_SERIAL_DEBUG)
    Serial.println("Client disconnected.");
    Serial.println("");
#endif
  }
}

static void sendJson(WiFiClient &client, const char *status, const String &body) {
  // Minimal helper to emit JSON responses with a given HTTP status line
  client.println(status);
  client.println("Content-type: application/json");
  client.println("Connection: close");
  client.println();
  client.println(body);
}

static const char *jsonBool(bool value) {
  return value ? "true" : "false";
}

static String hexByte(uint8_t value) {
  String result = "0x";
  if (value < 16) {
    result += "0";
  }
  result += String(value, HEX);
  return result;
}

static bool handleInfoRequest(WiFiClient &client, const String &header) {
  // /info → report hostname and current IP as JSON
  if (header.indexOf("GET /info") < 0) return false;
  String ipStr = WiFi.localIP().toString();
  String jsonResponse = "{\"hostname\":\"" + String(WIFI_HOSTNAME) + "\",\"ip\":\"" + ipStr + "\"}";
  sendJson(client, "HTTP/1.1 200 OK", jsonResponse);
  return true;
}

static bool handleI2cScanRequest(WiFiClient &client, const String &header) {
  // /i2c/scan -> list responding 7-bit I2C addresses for board bring-up.
  if (header.indexOf("GET /i2c/scan") < 0) return false;

  if (!i2c_lock(pdMS_TO_TICKS(250))) {
    sendJson(client, "HTTP/1.1 503 Service Unavailable", "{\"error\":\"I2C bus busy\"}");
    return true;
  }

  String jsonResponse = "{\"addresses\":[";
  bool first = true;
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      if (!first) {
        jsonResponse += ",";
      }
      jsonResponse += "\"0x";
      if (address < 16) {
        jsonResponse += "0";
      }
      jsonResponse += String(address, HEX);
      jsonResponse += "\"";
      first = false;
    }
  }
  i2c_unlock();
  jsonResponse += "]}";
  sendJson(client, "HTTP/1.1 200 OK", jsonResponse);
  return true;
}

static bool handleImuRawRequest(WiFiClient &client, const String &header) {
  if (header.indexOf("GET /imu/raw") < 0) return false;

  BalanceTelemetry telemetry;
  balance_get_raw(telemetry);
  String jsonResponse = "{";
  jsonResponse += "\"imuReady\":" + String(jsonBool(telemetry.imuReady));
  jsonResponse += ",\"lastReadOk\":" + String(jsonBool(telemetry.lastReadOk));
  jsonResponse += ",\"imuAddress\":\"" + hexByte(telemetry.imuAddress) + "\"";
  jsonResponse += ",\"whoAmI\":\"" + hexByte(telemetry.whoAmI) + "\"";
  jsonResponse += ",\"whoAmICompatible\":" + String(jsonBool(telemetry.whoAmICompatible));
  jsonResponse += ",\"updatedAtMs\":" + String(telemetry.updatedAtMs);
  jsonResponse += ",\"lastError\":\"" + String(telemetry.lastError) + "\"";
  jsonResponse += ",\"raw\":{";
  jsonResponse += "\"ax\":" + String(telemetry.raw.ax);
  jsonResponse += ",\"ay\":" + String(telemetry.raw.ay);
  jsonResponse += ",\"az\":" + String(telemetry.raw.az);
  jsonResponse += ",\"gx\":" + String(telemetry.raw.gx);
  jsonResponse += ",\"gy\":" + String(telemetry.raw.gy);
  jsonResponse += ",\"gz\":" + String(telemetry.raw.gz);
  jsonResponse += "}}";
  sendJson(client, "HTTP/1.1 200 OK", jsonResponse);
  return true;
}

static bool handleBalanceStatusRequest(WiFiClient &client, const String &header) {
  if (header.indexOf("GET /balance/status") < 0) return false;

  BalanceTelemetry telemetry;
  balance_get_status(telemetry);
  String jsonResponse = "{";
  jsonResponse += "\"imuReady\":" + String(jsonBool(telemetry.imuReady));
  jsonResponse += ",\"lastReadOk\":" + String(jsonBool(telemetry.lastReadOk));
  jsonResponse += ",\"calibrated\":" + String(jsonBool(telemetry.calibrated));
  jsonResponse += ",\"calibrationInProgress\":" + String(jsonBool(telemetry.calibrationInProgress));
  jsonResponse += ",\"balanceControllerEnabled\":" + String(jsonBool(telemetry.balanceControllerEnabled));
  jsonResponse += ",\"balanceControlSafetyOk\":" + String(jsonBool(telemetry.balanceControlSafetyOk));
  jsonResponse += ",\"balanceMotorOutputAvailable\":" + String(jsonBool(telemetry.balanceMotorOutputAvailable));
  jsonResponse += ",\"balanceMotorOutputArmed\":" + String(jsonBool(telemetry.balanceMotorOutputArmed));
  jsonResponse += ",\"balanceMotorOutputEnabled\":" + String(jsonBool(telemetry.balanceMotorOutputEnabled));
  jsonResponse += ",\"balanceDriveCommandSent\":" + String(jsonBool(telemetry.balanceDriveCommandSent));
  jsonResponse += ",\"imuAddress\":\"" + hexByte(telemetry.imuAddress) + "\"";
  jsonResponse += ",\"whoAmI\":\"" + hexByte(telemetry.whoAmI) + "\"";
  jsonResponse += ",\"whoAmICompatible\":" + String(jsonBool(telemetry.whoAmICompatible));
  jsonResponse += ",\"updatedAtMs\":" + String(telemetry.updatedAtMs);
  jsonResponse += ",\"loopCount\":" + String(telemetry.loopCount);
  jsonResponse += ",\"failedReadCount\":" + String(telemetry.failedReadCount);
  jsonResponse += ",\"loopDtMs\":" + String(telemetry.loopDtMs, 3);
  jsonResponse += ",\"accelPitchDeg\":" + String(telemetry.accelPitchDeg, 3);
  jsonResponse += ",\"accelPitchSource\":\"accelTiltXDeg\"";
  jsonResponse += ",\"accelAngleAyAzDeg\":" + String(telemetry.accelAngleAyAzDeg, 3);
  jsonResponse += ",\"accelAngleAxAzDeg\":" + String(telemetry.accelAngleAxAzDeg, 3);
  jsonResponse += ",\"accelAngleAxAyDeg\":" + String(telemetry.accelAngleAxAyDeg, 3);
  jsonResponse += ",\"accelTiltXDeg\":" + String(telemetry.accelTiltXDeg, 3);
  jsonResponse += ",\"axisFilterReady\":" + String(jsonBool(telemetry.axisFilterReady));
  jsonResponse += ",\"accelAngleAyAzSmoothedDeg\":" + String(telemetry.accelAngleAyAzSmoothedDeg, 3);
  jsonResponse += ",\"accelAngleAxAzSmoothedDeg\":" + String(telemetry.accelAngleAxAzSmoothedDeg, 3);
  jsonResponse += ",\"accelAngleAxAySmoothedDeg\":" + String(telemetry.accelAngleAxAySmoothedDeg, 3);
  jsonResponse += ",\"accelTiltXSmoothedDeg\":" + String(telemetry.accelTiltXSmoothedDeg, 3);
  jsonResponse += ",\"pitchDeg\":" + String(telemetry.pitchDeg, 3);
  jsonResponse += ",\"gyroRateDps\":" + String(telemetry.gyroRateDps, 3);
  jsonResponse += ",\"gyroRateSource\":\"gyroYRateDps\"";
  jsonResponse += ",\"gyroXRateDps\":" + String(telemetry.gyroXRateDps, 3);
  jsonResponse += ",\"gyroYRateDps\":" + String(telemetry.gyroYRateDps, 3);
  jsonResponse += ",\"gyroZRateDps\":" + String(telemetry.gyroZRateDps, 3);
  jsonResponse += ",\"gyroBiasRaw\":" + String(telemetry.gyroBiasRaw, 3);
  jsonResponse += ",\"balanceSetpointDeg\":" + String(telemetry.balanceSetpointDeg, 3);
  jsonResponse += ",\"balanceKp\":" + String(telemetry.balanceKp, 3);
  jsonResponse += ",\"balanceKi\":" + String(telemetry.balanceKi, 3);
  jsonResponse += ",\"balanceKd\":" + String(telemetry.balanceKd, 3);
  jsonResponse += ",\"balanceOutputLimit\":" + String(telemetry.balanceOutputLimit, 3);
  jsonResponse += ",\"balanceMaxAbsAngleDeg\":" + String(telemetry.balanceMaxAbsAngleDeg, 3);
  jsonResponse += ",\"balanceMotorSign\":" + String(telemetry.balanceMotorSign, 1);
  jsonResponse += ",\"balanceAngleErrorDeg\":" + String(telemetry.balanceAngleErrorDeg, 3);
  jsonResponse += ",\"balanceIntegralError\":" + String(telemetry.balanceIntegralError, 3);
  jsonResponse += ",\"balancePTerm\":" + String(telemetry.balancePTerm, 3);
  jsonResponse += ",\"balanceITerm\":" + String(telemetry.balanceITerm, 3);
  jsonResponse += ",\"balanceDTerm\":" + String(telemetry.balanceDTerm, 3);
  jsonResponse += ",\"balanceOutputRaw\":" + String(telemetry.balanceOutputRaw, 3);
  jsonResponse += ",\"balanceOutputClamped\":" + String(telemetry.balanceOutputClamped, 3);
  jsonResponse += ",\"balanceLeftPwm\":" + String(telemetry.balanceLeftPwm);
  jsonResponse += ",\"balanceRightPwm\":" + String(telemetry.balanceRightPwm);
  jsonResponse += ",\"balanceSafetyReason\":\"" + String(telemetry.balanceSafetyReason) + "\"";
  jsonResponse += ",\"lastError\":\"" + String(telemetry.lastError) + "\"";
  jsonResponse += "}";
  sendJson(client, "HTTP/1.1 200 OK", jsonResponse);
  return true;
}

static bool handleBalanceConfigRequest(WiFiClient &client, const String &header) {
  if (header.indexOf("GET /balance/config") < 0) return false;

  BalanceControlConfig config;
  balance_get_config(config);

  float value = 0.0f;
  bool updated = false;
  if (queryFloatParam(header, "setpoint", value)) {
    config.setpointDeg = clampConfigValue(value, -10.0f, 10.0f);
    updated = true;
  }
  if (queryFloatParam(header, "kp", value)) {
    config.kp = clampConfigValue(value, 0.0f, 200.0f);
    updated = true;
  }
  if (queryFloatParam(header, "ki", value)) {
    config.ki = clampConfigValue(value, 0.0f, 10.0f);
    updated = true;
  }
  if (queryFloatParam(header, "kd", value)) {
    config.kd = clampConfigValue(value, 0.0f, 20.0f);
    updated = true;
  }
  if (queryFloatParam(header, "limit", value)) {
    config.outputLimit = clampConfigValue(value, 0.0f, 255.0f);
    updated = true;
  }
  if (queryFloatParam(header, "maxAngle", value)) {
    config.maxAbsAngleDeg = clampConfigValue(value, 1.0f, 45.0f);
    updated = true;
  }
  if (queryFloatParam(header, "motorSign", value)) {
    config.motorSign = value < 0.0f ? -1.0f : 1.0f;
    updated = true;
  }

  if (updated) {
    balance_set_config(config);
  }

  String jsonResponse = "{";
  jsonResponse += "\"updated\":" + String(jsonBool(updated));
  jsonResponse += ",\"volatile\":true";
  jsonResponse += ",\"motorOutputAvailable\":" + String(jsonBool(BALANCE_MOTOR_OUTPUT_AVAILABLE != 0));
  jsonResponse += ",\"armMaxOutputLimit\":" + String(BALANCE_ARM_MAX_OUTPUT_LIMIT, 3);
  jsonResponse += ",\"setpointDeg\":" + String(config.setpointDeg, 3);
  jsonResponse += ",\"kp\":" + String(config.kp, 3);
  jsonResponse += ",\"ki\":" + String(config.ki, 3);
  jsonResponse += ",\"kd\":" + String(config.kd, 3);
  jsonResponse += ",\"outputLimit\":" + String(config.outputLimit, 3);
  jsonResponse += ",\"maxAbsAngleDeg\":" + String(config.maxAbsAngleDeg, 3);
  jsonResponse += ",\"motorSign\":" + String(config.motorSign, 1);
  jsonResponse += "}";
  sendJson(client, "HTTP/1.1 200 OK", jsonResponse);
  return true;
}

static bool handleBalanceArmRequest(WiFiClient &client, const String &header) {
  if (header.indexOf("GET /balance/arm") < 0) return false;

  char reason[64] = "";
  const bool armed = balance_request_motor_arm(reason, sizeof(reason));
  String jsonResponse = "{";
  jsonResponse += "\"armed\":" + String(jsonBool(armed));
  jsonResponse += ",\"motorOutputAvailable\":" + String(jsonBool(BALANCE_MOTOR_OUTPUT_AVAILABLE != 0));
  jsonResponse += ",\"armMaxOutputLimit\":" + String(BALANCE_ARM_MAX_OUTPUT_LIMIT, 3);
  jsonResponse += ",\"reason\":\"" + String(reason) + "\"";
  jsonResponse += "}";
  sendJson(client, armed ? "HTTP/1.1 202 Accepted" : "HTTP/1.1 409 Conflict", jsonResponse);
  return true;
}

static bool handleBalanceDisarmRequest(WiFiClient &client, const String &header) {
  if (header.indexOf("GET /balance/disarm") < 0) return false;

  balance_request_motor_disarm();
  sendJson(client, "HTTP/1.1 200 OK", "{\"armed\":false,\"reason\":\"disarmed\"}");
  return true;
}

static bool handleBalanceCalibrateRequest(WiFiClient &client, const String &header) {
  if (header.indexOf("GET /balance/calibrate") < 0) return false;

  const bool queued = balance_request_calibration();
  String jsonResponse = "{";
  jsonResponse += "\"calibrationQueued\":" + String(jsonBool(queued));
  jsonResponse += ",\"requiresStationaryRobot\":true";
  jsonResponse += ",\"message\":\"Keep the robot still while calibrationInProgress is true.\"";
  jsonResponse += "}";
  sendJson(client, queued ? "HTTP/1.1 202 Accepted" : "HTTP/1.1 503 Service Unavailable", jsonResponse);
  return true;
}

static bool queryFloatParam(const String &header, const char *name, float &value) {
  const int pathStart = header.indexOf("GET /balance/config");
  if (pathStart < 0) return false;
  const int queryStart = header.indexOf('?', pathStart);
  const int httpStart = header.indexOf(" HTTP", pathStart);
  if (queryStart < 0 || httpStart < 0 || queryStart > httpStart) return false;

  String token = String(name) + "=";
  const int tokenStart = header.indexOf(token, queryStart + 1);
  if (tokenStart < 0 || tokenStart > httpStart) return false;
  const char previous = header.charAt(tokenStart - 1);
  if (previous != '?' && previous != '&') return false;

  const int valueStart = tokenStart + token.length();
  int valueEnd = header.indexOf('&', valueStart);
  if (valueEnd < 0 || valueEnd > httpStart) {
    valueEnd = httpStart;
  }
  String valueText = header.substring(valueStart, valueEnd);
  if (valueText.length() == 0) return false;
  value = valueText.toFloat();
  return true;
}

static float clampConfigValue(float value, float minValue, float maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

static bool handleSensorRequest(WiFiClient &client, const String &header) {
  // /sensor/ht → read SHT3x once and return temperature/humidity JSON
  if (header.indexOf("GET /sensor/ht") < 0) return false;
  if (!sht3xReady) {
    sendJson(client, "HTTP/1.1 503 Service Unavailable", "{\"error\":\"SHT3x not initialized\"}");
    return true;
  }

  float aTemperature = 0.0;
  float aHumidity = 0.0;
  if (!i2c_lock(pdMS_TO_TICKS(250))) {
    sendJson(client, "HTTP/1.1 503 Service Unavailable", "{\"error\":\"I2C bus busy\"}");
    return true;
  }
  int16_t error = sensor.measureSingleShot(REPEATABILITY_MEDIUM, false, aTemperature, aHumidity);
  i2c_unlock();
  if (error != NO_ERROR) {
#ifdef USE_SERIAL
    char errorMessage[64];
    Serial.print("Error trying to execute blockingReadMeasurement(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
#endif
    sendJson(client, "HTTP/1.1 500 Internal Server Error", "{\"error\":\"Sensor read failed\"}");
    return true;
  }

#ifdef USE_SERIAL
  Serial.print("aTemperature: ");
  Serial.print(aTemperature);
  Serial.print("\t");
  Serial.print("aHumidity: ");
  Serial.print(aHumidity);
  Serial.println();
#endif

  String jsonResponse = "{\"temperature\": " + String(aTemperature) + ", \"humidity\": " + String(aHumidity) + "}";
  sendJson(client, "HTTP/1.1 200 OK", jsonResponse);
  return true;
}

static bool handleMotorRequest(WiFiClient &client, const String &header) {
  // /motor/* → enqueue a motor command and acknowledge
  if (header.indexOf("GET /motor/") < 0) return false;
  const bool holdMode = header.indexOf("hold=1") >= 0;

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();

  if (!g_motorQueue) {
    client.println();
    return true;
  }

  MotorCommandMsg msg{};
  bool recognized = false;
  // Dispatch motor commands by matching the request path to a direction/stop action.
  // header.indexOf(...) returns the substring position or -1; >= 0 means the route is present.
  if (header.indexOf("GET /motor/test/left-high") >= 0) {
#ifdef USE_SERIAL
    Serial.println("LEFT_HIGH");
#endif
    msg.cmd = MotorCommand::LeftHigh;
    msg.timeoutMs = holdMode ? MOTOR_HOLD_REFRESH_TIME : MOTOR_DIAGNOSTIC_TIME;
    recognized = true;
  } else if (header.indexOf("GET /motor/test/left-low") >= 0) {
#ifdef USE_SERIAL
    Serial.println("LEFT_LOW");
#endif
    msg.cmd = MotorCommand::LeftLow;
    msg.timeoutMs = holdMode ? MOTOR_HOLD_REFRESH_TIME : MOTOR_DIAGNOSTIC_TIME;
    recognized = true;
  } else if (header.indexOf("GET /motor/test/right-high") >= 0) {
#ifdef USE_SERIAL
    Serial.println("RIGHT_HIGH");
#endif
    msg.cmd = MotorCommand::RightHigh;
    msg.timeoutMs = holdMode ? MOTOR_HOLD_REFRESH_TIME : MOTOR_DIAGNOSTIC_TIME;
    recognized = true;
  } else if (header.indexOf("GET /motor/test/right-low") >= 0) {
#ifdef USE_SERIAL
    Serial.println("RIGHT_LOW");
#endif
    msg.cmd = MotorCommand::RightLow;
    msg.timeoutMs = holdMode ? MOTOR_HOLD_REFRESH_TIME : MOTOR_DIAGNOSTIC_TIME;
    recognized = true;
  } else if (header.indexOf("GET /motor/forward") >= 0) { // forward command
#ifdef USE_SERIAL
    Serial.println(MOTOR_STATE_STRINGS[0]);
#endif
    msg.cmd = MotorCommand::Forward;
    msg.timeoutMs = holdMode ? MOTOR_HOLD_REFRESH_TIME : MOTOR_FORWARD_BACK_TIME;
    recognized = true;
  } else if (header.indexOf("GET /motor/back") >= 0) {
#ifdef USE_SERIAL
    Serial.println(MOTOR_STATE_STRINGS[1]);
#endif
    msg.cmd = MotorCommand::Back;
    msg.timeoutMs = holdMode ? MOTOR_HOLD_REFRESH_TIME : MOTOR_FORWARD_BACK_TIME;
    recognized = true;
  } else if (header.indexOf("GET /motor/left") >= 0) {
#ifdef USE_SERIAL
    Serial.println(MOTOR_STATE_STRINGS[2]);
#endif
    msg.cmd = MotorCommand::Left;
    msg.timeoutMs = holdMode ? MOTOR_HOLD_REFRESH_TIME : MOTOR_TURN_TIME;
    recognized = true;
  } else if (header.indexOf("GET /motor/right") >= 0) {
#ifdef USE_SERIAL
    Serial.println(MOTOR_STATE_STRINGS[3]);
#endif
    msg.cmd = MotorCommand::Right;
    msg.timeoutMs = holdMode ? MOTOR_HOLD_REFRESH_TIME : MOTOR_TURN_TIME;
    recognized = true;
  } else if (header.indexOf("GET /motor/stop") >= 0) {
#ifdef USE_SERIAL
    Serial.println(MOTOR_STATE_STRINGS[4]);
#endif
    msg.cmd = MotorCommand::Stop;
    msg.timeoutMs = 0;
    recognized = true;
  }

  if (recognized) {
#ifdef SAFE_BRINGUP
    if (msg.cmd != MotorCommand::Stop) {
      client.println("<h1>SAFE_BRINGUP: movement disabled</h1>");
      client.println("<p>Rebuild without SAFE_BRINGUP after motor polarity and stop behavior are verified.</p>");
      return true;
    }
#endif
    xQueueSend(g_motorQueue, &msg, 0);
  }

  client.println();
  return true;
}
