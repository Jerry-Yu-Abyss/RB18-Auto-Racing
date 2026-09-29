#include <ps5Controller.h>

// 本專題固定使用 Arduino-ESP32 Core 3.3.6。
// 已知 Core 3.3.11 會讓目前使用的 esp-ps5 Scanner 掃不到 DualSense。

// ==========================
// 控制參數
// ==========================

// 原始搖桿值在 -10 ~ +10（包含邊界）時視為中央。
constexpr int STEERING_DEAD_ZONE = 10;

// 控制迴圈 50 Hz；Serial 顯示 10 Hz。
constexpr unsigned long CONTROL_INTERVAL_MS = 20;
constexpr unsigned long TELEMETRY_INTERVAL_MS = 100;
constexpr unsigned long DISCONNECTED_MESSAGE_INTERVAL_MS = 1000;

// -1.0 = 全左
//  0.0 = 中央
// +1.0 = 全右
float steering = 0.0;

// -1.0 = 全速倒車
//  0.0 = 停止
// +1.0 = 全速前進
float throttle = 0.0;

unsigned long lastControlTime = 0;
unsigned long lastTelemetryTime = 0;
unsigned long lastDisconnectedMessageTime = 0;
bool wasConnected = false;


// ==========================
// 統一車輛控制介面
// ==========================

void drive(float requestedSteering, float requestedThrottle) {

  // 所有輸入來源最後都必須經過這個範圍保護。
  steering = constrain(requestedSteering, -1.0f, 1.0f);
  throttle = constrain(requestedThrottle, -1.0f, 1.0f);

  // 下一階段會在這裡加入：
  // 1. steering -> Servo PWM
  // 2. throttle -> DRV8876 PWM / DIR
}


void failsafeStop() {

  // 手把斷線時，不允許沿用最後一筆控制命令。
  drive(0.0f, 0.0f);
}


// ==========================
// 讀取 PS5 手把
// ==========================

void updateControls() {

  // -------- Steering --------

  int rawSteering = ps5.lx;

  // Dead Zone
  // 搖桿在 -10 ~ +10 都視為中央
  if (abs(rawSteering) <= STEERING_DEAD_ZONE) {
    rawSteering = 0;
  }

  // 負方向除以 128、正方向除以 127，讓兩端都精確等於 1.0。
  float requestedSteering = 0.0f;
  if (rawSteering < 0) {
    requestedSteering = rawSteering / 128.0f;
  } else {
    requestedSteering = rawSteering / 127.0f;
  }


  // -------- Throttle --------

  // R2：0 ~ 255
  float forward = ps5.r2 / 255.0f;

  // L2：0 ~ 255
  float reverse = ps5.l2 / 255.0f;

  // R2 = 正
  // L2 = 負
  float requestedThrottle = forward - reverse;

  drive(requestedSteering, requestedThrottle);
}


void printControls() {

  Serial.print("CONNECTED | Steering: ");
  Serial.print(steering, 2);

  Serial.print(" | Throttle: ");
  Serial.println(throttle, 2);
}


// ==========================
// ESP32 啟動
// ==========================

void setup() {

  Serial.begin(115200);

  delay(1500);

  Serial.println();
  Serial.println("=======================");
  Serial.println("RB18 Controller");
  Serial.println("=======================");

  Serial.println("Waiting for DualSense...");

  // 搜尋 PS5 手把 30 秒
  ps5.begin(30);
}


// ==========================
// 主程式
// ==========================

void loop() {

  const unsigned long now = millis();
  const bool isConnected = ps5.isConnected();

  // 如果 PS5 尚未連線，或連線中途斷開。
  if (!isConnected) {

    // Failsafe 必須先執行，再處理顯示或等待。
    failsafeStop();

    if (wasConnected) {
      Serial.println("PS5 DISCONNECTED -> FAILSAFE ACTIVE | Steering: 0.00 | Throttle: 0.00");
      wasConnected = false;
      lastDisconnectedMessageTime = now;
    } else if (now - lastDisconnectedMessageTime >= DISCONNECTED_MESSAGE_INTERVAL_MS) {
      Serial.println("PS5: NOT CONNECTED | FAILSAFE ACTIVE");
      lastDisconnectedMessageTime = now;
    }

    return;  // 沒有手把命令時，不執行後面的正常控制流程。
  }

  if (!wasConnected) {
    Serial.println("PS5 CONNECTED");
    wasConnected = true;
  }

  // 固定 50 Hz 更新控制命令，避免 Serial 顯示速度影響控制週期。
  if (now - lastControlTime >= CONTROL_INTERVAL_MS) {
    updateControls();
    lastControlTime = now;
  }

  // Serial Monitor 維持 10 Hz，方便人眼觀察。
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    printControls();
    lastTelemetryTime = now;
  }
}
