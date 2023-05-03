#include <Arduino.h>
#include <M5Unified.h>
#include <FastLED.h>
#include <ESP32Servo.h>


// 定数

const int D_DISPLAY_LED_PIN = 35;
const int D_DISPLAY_LED_SIZE = 1;
const int D_SHOCKER_DRIVE_ANGLE = 30;
const int D_SHOCKER_POWER_PIN = 1;
const int D_SHOCKER_POWER_DELAY = 1200;
const int D_SHOCKER_LEVEL_PIN = 2;
const int D_SHOCKER_LEVEL_DELAY = 100;


// グローバル変数

volatile bool gShockerIsDriving = false;

CRGB gDisplayLed[D_DISPLAY_LED_SIZE];
QueueHandle_t gShockerQueue = xQueueCreate(1, sizeof(int));
Servo gShockerPower;
Servo gShockerLevel;


/** ディスプレイの更新 */
void updateDisplay(void* pvParameters) {
  while (true) {
    CRGB color;
    if (gShockerIsDriving) { 
      color = millis() % 400 > 200 ? CRGB::Red : CRGB::Black; 
    } else {
      color = CRGB::Blue;
    }
    FastLED.showColor(color);
    delay(100);
  }
}


/** 電源ONOFF用サーボを駆動 */
void driveShockerPower() {
  gShockerPower.write(D_SHOCKER_DRIVE_ANGLE);
  delay(100);
  gShockerPower.write(0);
  delay(100);
  // Serial.println("[DEBUG] Shocker power driven.");
}


/** 強さUP用サーボを駆動 */
void driveShockerLevel() {
  gShockerLevel.write(D_SHOCKER_DRIVE_ANGLE);
  delay(100);
  gShockerLevel.write(0);
  delay(100);
  // Serial.println("[DEBUG] Shocker level driven.");
}


/** サーボの更新 */
void updateShocker(void* pvParameters) {
  while (true) {
    int parameter;
    if (xQueueReceive(gShockerQueue, &parameter, 0) == pdTRUE) {
      auto level = (parameter >> 8) & 0xff;
      auto duration = (parameter >> 0) & 0xff;
      Serial.printf("[INFO] Shocker parameter received. %d, %d", level, duration);
      Serial.println();
      gShockerIsDriving = true;
      // 電源ON
      driveShockerPower();
      delay(D_SHOCKER_POWER_DELAY);
      // 強さUP
      for (int i=0; i<level; i++) {
        driveShockerLevel();
        delay(D_SHOCKER_LEVEL_DELAY);
      }
      // 指定時間分待機
      for (int i=0; i<duration*10; i++) {
        if (gShockerIsDriving) { delay(100); }
      }
      // 電源OFF
      driveShockerPower();
      gShockerIsDriving = false;
      Serial.printf("[INFO] Servo task done.\n");
    }
    delay(100);
  }
}


/** ディスプレイの設定 */
void beginDisplay() {
  FastLED.addLeds<WS2812B, D_DISPLAY_LED_PIN, GRB>(gDisplayLed, D_DISPLAY_LED_SIZE);
  FastLED.setBrightness(64);
  xTaskCreatePinnedToCore(updateDisplay, "updateDisplay", 4096, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
  Serial.println("[INFO] Display begun.");
}


/** サーボの設定 */
void beginShocker() {
  gShockerPower.attach(D_SHOCKER_POWER_PIN);
  gShockerPower.write(0);
  gShockerLevel.attach(D_SHOCKER_LEVEL_PIN);
  gShockerLevel.write(0);
  xTaskCreatePinnedToCore(updateShocker, "updateShocker", 4096, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
  Serial.println("[INFO] Shocker begun.");
}


/** セットアップ */
void setup() {
  M5.begin();
  beginDisplay();
  beginShocker();
}


/** ボタンの状態更新 */
void updateButton() {
  if (M5.BtnA.wasPressed()) {
    if (gShockerIsDriving) {
      // キャンセル
      gShockerIsDriving = false;
      Serial.println("[INFO] Button cancel pressed.");
    } else {
      // サーボ試し運転
      auto level = 5;
      auto duration = 5;
      auto parameter = (level << 8) | duration;
      xQueueSend(gShockerQueue, &parameter, 0);
      Serial.println("[INFO] Button test pressed.");
    }
  }
}

/** シリアルの状態更新 */
void updateSerial() {
  if (Serial.available()) {
    auto message = Serial.readString();
    Serial.println(message);
    if (message.startsWith("-> ")) {
      auto parameter = int(message.substring(3).toInt());
      xQueueSend(gShockerQueue, &parameter, 0);
    }
  }
}


/** ループ */
void loop() {
  M5.update();
  updateButton();
  updateSerial();
  delay(100);
}
