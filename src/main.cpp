#include <Arduino.h>
#include <M5Unified.h>
#include <FastLED.h>
#include <ESP32Servo.h>


const int D_DISPLAY_LED_PIN = 35;
const int D_DISPLAY_LED_NUM = 1;
const int D_SERVO_DRIVE_ANGLE = 30;
const int D_POWER_SERVO_PIN = 1;
const int D_POWER_SERVO_DELAY = 1200;
const int D_LEVEL_SERVO_PIN = 2;
const int D_LEVEL_SERVO_DELAY = 100;


CRGB gDisplayLed[D_DISPLAY_LED_NUM];
volatile bool gServoIsDriven = false;
QueueHandle_t gServoQueue = xQueueCreate(1, sizeof(int));
Servo gPowerServo;
Servo gLevelServo;


/** ディスプレイのタスク */
void taskDisplay(void* pvParameters) {
  while (true) {
    CRGB color;
    if (gServoIsDriven) { 
      color = millis() % 400 > 200 ? CRGB::Red : CRGB::Black; 
    } else {
      color = CRGB::Blue;
    }
    FastLED.showColor(color);
    delay(100);
  }
}


/** 電源ボタンを押すサーボの駆動 */
void drivePowerServo() {
  gPowerServo.write(D_SERVO_DRIVE_ANGLE);
  delay(100);
  gPowerServo.write(0);
  delay(100);
  // Serial.printf("[DEBUG] Power servo driven.\n");
}


/** 強さボタンを押すサーボの駆動 */
void driveLevelServo() {
  gLevelServo.write(D_SERVO_DRIVE_ANGLE);
  delay(100);
  gLevelServo.write(0);
  delay(100);
  // Serial.printf("[DEBUG] Level servo driven.\n");
}


/** サーボのタスク */
void taskServo(void* pvParameters) {
  while (true) {
    int parameter;
    if (xQueueReceive(gServoQueue, &parameter, 0) == pdTRUE) {
      auto level = (parameter >> 8) & 0xff;
      auto duration = (parameter >> 0) & 0xff;
      Serial.printf("[INFO] Servo parameter received. %d, %d\n", level, duration);
      gServoIsDriven = true;
      // 電源ON
      drivePowerServo();
      delay(D_POWER_SERVO_DELAY);
      // 強さUP
      for (int i=0; i<level; i++) {
        driveLevelServo();
        delay(D_LEVEL_SERVO_DELAY);
      }
      // 指定時間分待機
      for (int i=0; i<duration*10; i++) {
        if (gServoIsDriven) { delay(100); }
      }
      // 電源OFF
      drivePowerServo();
      gServoIsDriven = false;
      Serial.printf("[INFO] Servo task done.\n");
    }
    delay(100);
  }
}


/** ディスプレイの初期設定 */
void beginDisplay() {
  FastLED.addLeds<WS2812B, D_DISPLAY_LED_PIN, GRB>(gDisplayLed, D_DISPLAY_LED_NUM);
  FastLED.setBrightness(64);
  xTaskCreatePinnedToCore(taskDisplay, "taskDisplay", 4096, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
  // Serial.printf("[DEBUG] Display begun.\n");
}


/** サーボの初期設定 */
void beginServo() {
  gPowerServo.attach(D_POWER_SERVO_PIN);
  gPowerServo.write(0);
  gLevelServo.attach(D_LEVEL_SERVO_PIN);
  gLevelServo.write(0);
  xTaskCreatePinnedToCore(taskServo, "taskServo", 4096, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
  // Serial.printf("[DEBUG] Servo begun.\n");
}


/** セットアップ */
void setup() {
  M5.begin();
  beginDisplay();
  beginServo();
  // Serial.printf("[DEBUG] M5 begun.\n");
}


/** ボタンの状態更新 */
void updateButton() {
  if (M5.BtnA.wasPressed()) {
    if (gServoIsDriven) {
      // キャンセル
      gServoIsDriven = false;
      Serial.printf("[INFO] Button cancel pressed.\n");
    } else {
      // サーボ試し運転
      auto level = 5;
      auto duration = 5;
      int parameter = (level << 8) | duration;
      xQueueSend(gServoQueue, &parameter, 0);
      Serial.printf("[INFO] Button test pressed.\n");
    }
  }
}

/** シリアルの状態更新 */
void updateSerial() {
  if (Serial.available()) {
    auto message = Serial.readString();
    Serial.printf("[INFO] Serial message received. %s\n");
    if (message.startsWith("-> ")) {
      int parameter = message.substring(3).toInt();
      xQueueSend(gServoQueue, &parameter, 0);
    }
  }
  // Serial.printf("[DEBUG] Serial updated.\n");
}


/** ループ */
void loop() {
  M5.update();
  updateButton();
  updateSerial();
  // Serial.printf("[DEBUG] M5 updated.\n");
  delay(100);
}
