/*
  Adaptive Urban Heat Shelter
  Prototype firmware for Arduino UNO / UNO-compatible board

  Functions:
  - DHT22: temperature and relative humidity
  - BH1750: ambient illuminance (lux; proxy for sunlight, not UV measurement)
  - HC-SR501 PIR: occupancy/presence signal
  - PCA9685: two servo outputs controlling the linked louver/reflective-panel mechanism
  - MOSFET outputs: 12 V ventilation fan and water pump

  IMPORTANT:
  This is a prototype rule-based controller, not a certified heat-stress/WBGT system.
  Tune thresholds and servo angles after testing your actual mechanics.
  Keep water away from electronics and use a properly rated MOSFET/driver and flyback
  protection for inductive loads. Use a separate suitable supply for servos and actuators,
  with grounds connected together. Do not power servos, fan, or pump from the Arduino 5 V pin.
*/

#include <Wire.h>
#include <DHT.h>
#include <BH1750.h>
#include <Adafruit_PWMServoDriver.h>

// ---------- Pin mapping ----------
#define DHT_PIN       2
#define DHT_TYPE      DHT22
#define PIR_PIN       3
#define FAN_PIN       8    // MOSFET gate/driver input; PWM-capable pin recommended if speed control is needed
#define PUMP_PIN      9    // MOSFET gate/driver input; PWM-capable pin recommended if speed control is needed

// PCA9685 servo channels
const uint8_t SHADE_SERVO_CHANNEL = 0;
const uint8_t PANEL_SERVO_CHANNEL = 1;

// ---------- Libraries / devices ----------
DHT dht(DHT_PIN, DHT_TYPE);
BH1750 lightMeter;
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// ---------- Prototype thresholds (adjust after testing) ----------
const float CAUTION_TEMP_C = 30.0;
const float HIGH_TEMP_C = 35.0;
const float HIGH_HUMIDITY_PERCENT = 80.0;
const float BRIGHT_LUX = 15000.0;

// Servo pulse limits for PCA9685 at 50 Hz.
// These are starting values only; calibrate to your servos/mechanism.
const uint16_t SERVO_MIN_TICK = 110;
const uint16_t SERVO_MAX_TICK = 510;

// Set angles to match your linked louver + reflective panel mechanism.
const int SHADE_OPEN_ANGLE = 20;
const int SHADE_CLOSED_ANGLE = 100;
const int PANEL_STOW_ANGLE = 15;
const int PANEL_DEPLOY_ANGLE = 95;

const unsigned long SENSOR_INTERVAL_MS = 2000;
unsigned long lastSensorRead = 0;

float temperatureC = NAN;
float humidityPct = NAN;
float illuminanceLux = NAN;
bool occupied = false;

enum HeatState { SAFE_STATE, CAUTION_STATE, HIGH_HEAT_STATE };
HeatState heatState = SAFE_STATE;

uint16_t angleToTick(int angle) {
  angle = constrain(angle, 0, 180);
  return map(angle, 0, 180, SERVO_MIN_TICK, SERVO_MAX_TICK);
}

void setServoAngle(uint8_t channel, int angle) {
  pwm.setPWM(channel, 0, angleToTick(angle));
}

void setShadeAndPanel(bool deploy) {
  // Both mechanisms are commanded together, as requested.
  if (deploy) {
    setServoAngle(SHADE_SERVO_CHANNEL, SHADE_CLOSED_ANGLE);
    setServoAngle(PANEL_SERVO_CHANNEL, PANEL_DEPLOY_ANGLE);
  } else {
    setServoAngle(SHADE_SERVO_CHANNEL, SHADE_OPEN_ANGLE);
    setServoAngle(PANEL_SERVO_CHANNEL, PANEL_STOW_ANGLE);
  }
}

void classifyConditions() {
  // A simple prototype heuristic using temperature, humidity, and illuminance.
  // It is NOT a validated Heat Index or WBGT calculation.
  bool highTemp = temperatureC >= HIGH_TEMP_C;
  bool cautionTemp = temperatureC >= CAUTION_TEMP_C;
  bool veryBright = illuminanceLux >= BRIGHT_LUX;

  if (highTemp || (temperatureC >= CAUTION_TEMP_C && veryBright) ||
      (temperatureC >= CAUTION_TEMP_C && humidityPct >= HIGH_HUMIDITY_PERCENT)) {
    heatState = HIGH_HEAT_STATE;
  } else if (cautionTemp || veryBright) {
    heatState = CAUTION_STATE;
  } else {
    heatState = SAFE_STATE;
  }
}

void applyControl() {
  bool highHumidity = humidityPct >= HIGH_HUMIDITY_PERCENT;

  // Shade/panel deployment is based on brightness or elevated temperature.
  bool deployShade = (illuminanceLux >= BRIGHT_LUX) ||
                     (temperatureC >= CAUTION_TEMP_C);
  setShadeAndPanel(deployShade);

  // Ventilation only when someone is present and conditions need cooling.
  // Digital ON/OFF is used for broad compatibility; change to analogWrite only
  // after confirming the selected pin and driver support PWM safely.
  bool fanOn = occupied && (heatState != SAFE_STATE);
  digitalWrite(FAN_PIN, fanOn ? HIGH : LOW);

  // Misting only when occupied, hot, and humidity is not already high.
  // Start with intermittent operation and test water delivery safely.
  bool pumpOn = occupied && (heatState == HIGH_HEAT_STATE) && !highHumidity;
  digitalWrite(PUMP_PIN, pumpOn ? HIGH : LOW);
}

void printStatus() {
  Serial.println(F("---- Adaptive Urban Heat Shelter ----"));

  Serial.print(F("Temperature (C): "));
  if (isnan(temperatureC)) Serial.println(F("sensor error"));
  else Serial.println(temperatureC, 1);

  Serial.print(F("Humidity (%): "));
  if (isnan(humidityPct)) Serial.println(F("sensor error"));
  else Serial.println(humidityPct, 1);

  Serial.print(F("Illuminance (lux): "));
  if (isnan(illuminanceLux)) Serial.println(F("sensor error"));
  else Serial.println(illuminanceLux, 0);

  Serial.print(F("Occupancy: "));
  Serial.println(occupied ? F("DETECTED") : F("NOT DETECTED"));

  Serial.print(F("Prototype state: "));
  switch (heatState) {
    case SAFE_STATE: Serial.println(F("SAFE")); break;
    case CAUTION_STATE: Serial.println(F("CAUTION")); break;
    case HIGH_HEAT_STATE: Serial.println(F("HIGH HEAT")); break;
  }

  Serial.print(F("Shade/panel: "));
  Serial.println((illuminanceLux >= BRIGHT_LUX || temperatureC >= CAUTION_TEMP_C)
                   ? F("DEPLOYED") : F("STOWED"));
  Serial.println();
}

void setup() {
  Serial.begin(9600);

  pinMode(PIR_PIN, INPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);

  // Safe startup: outputs off before sensor initialization.
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(PUMP_PIN, LOW);

  Wire.begin();
  dht.begin();

  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("WARNING: BH1750 not detected. Check I2C wiring."));
  }

  pwm.begin();
  pwm.setPWMFreq(50);
  delay(500);

  setShadeAndPanel(false);

  Serial.println(F("Adaptive Urban Heat Shelter controller started."));
  Serial.println(F("Open Serial Monitor at 9600 baud."));
}

void loop() {
  occupied = (digitalRead(PIR_PIN) == HIGH);

  if (millis() - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    float lux = lightMeter.readLightLevel();

    // Retain previous valid values if a sensor reading temporarily fails.
    if (!isnan(t)) temperatureC = t;
    if (!isnan(h)) humidityPct = h;
    if (lux >= 0) illuminanceLux = lux;

    if (isnan(temperatureC) || isnan(humidityPct) || isnan(illuminanceLux)) {
      // Fail-safe behavior: disable water and fan if essential readings are invalid.
      digitalWrite(FAN_PIN, LOW);
      digitalWrite(PUMP_PIN, LOW);
      Serial.println(F("Sensor reading unavailable; fan and pump forced OFF."));
      return;
    }

    classifyConditions();
    applyControl();
    printStatus();
  }
}
