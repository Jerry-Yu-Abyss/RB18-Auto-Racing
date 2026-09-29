# RB18 Auto Racing

## 從模擬到實體：基於電腦視覺與深度學習之自主賽車系統

**From Simulation to Reality: An Autonomous Racing System Based on Computer Vision and Deep Learning**

本專案以 RASTAR 1:12 Oracle Red Bull Racing RB18 遙控車為平台，逐步建立一套可由人工遙控切換至自主駕駛的賽車系統。

目前階段先完成實體車的底層控制：由 PS5 DualSense 透過 Bluetooth 控制 ESP32，再由 ESP32 輸出轉向與動力命令。底層控制穩定後，才會加入 Raspberry Pi 5、相機、電腦視覺、CNN、Donkey Simulator 與 Sim-to-Real。

## 系統架構

最終自主模式：

```text
Camera
   ↓
Raspberry Pi 5
CV / CNN / AI
   ↓ UART
ESP32
   ├─ PWM → Servo → 原車 steering rack
   └─ PWM → BTS7960 → 260S 馬達
```

人工遙控與測試模式：

```text
PS5 DualSense
   ↓ Bluetooth Classic
ESP32
   ↓
drive(steering, throttle)
```

無論命令來自 DualSense 或未來的 Raspberry Pi，ESP32 最後都只處理統一格式：

```text
steering：-1.0（全左）～ 0.0（中央）～ +1.0（全右）
throttle：-1.0（倒車）～ 0.0（停止）～ +1.0（前進）
```

## 目前進度

已完成：

- 確認 Classic ESP32 Bluetooth 功能正常。
- 使用 `esp-ps5` 成功掃描並連接 DualSense。
- 成功讀取 `ps5.lx`、`ps5.r2` 與 `ps5.l2`。
- 建立 `steering` 與 `throttle` 統一控制介面。
- 加入搖桿 Dead Zone。
- 加入 DualSense 斷線 Failsafe，斷線時立即將控制命令歸零。
- 控制更新頻率與 Serial 顯示頻率分離。

尚未完成：

- Servo 實體輸出、中心校正與左右極限設定。
- BTS7960 馬達 PWM、正反轉與煞車控制。
- 有源蜂鳴器狀態提示。
- 電壓與電流監控。
- Raspberry Pi UART命令介面。
- 相機、CV、CNN與Sim-to-Real。

## 重要軟體版本

| 項目 | 版本／設定 |
|---|---|
| Arduino IDE | 2.3.10 |
| Arduino Board | ESP32 Dev Module |
| Arduino-ESP32 Core | **3.3.6** |
| Serial baud rate | 115200 |
| PS5函式庫 | `esp-ps5` |

> [!IMPORTANT]
> 本專案目前必須固定使用 Arduino-ESP32 Core **3.3.6**。實測 Core 3.3.11 雖可正常編譯與上傳，但 `esp-ps5` Scanner 無法掃描 DualSense；降回3.3.6後立即恢復正常。升級Core前必須重新驗證Bluetooth掃描與連線。

## 目前硬體

| 類別 | 元件 |
|---|---|
| 車體 | RASTAR 1:12 Oracle Red Bull Racing RB18 |
| 主控制器 | Classic ESP32 Dev Module，ESP32-D0WD-V3 revision 3.1 |
| 人工控制 | PS5 DualSense |
| 後輪馬達 | 原車260S有刷直流馬達 |
| 馬達驅動 | IBT-2／雙BTS7960 |
| 轉向 | MG92B優先，MG90S可作雛形 |
| 主電池 | Panasonic EVOLTA鈦元素3號鹼性電池 × 5 |
| 降壓模組 | Mini-360可調式 × 2 |
| 狀態提示 | KY-012有源蜂鳴器 |
| 電力監控 | INA3221，選配且不直接量測馬達主電流 |

完整零件與配線材料請參考 [`元件清單.txt`](./元件清單.txt)。

## 原車量測資料

後輪260S有刷馬達的原廠驅動輸出實測約為：

```text
Forward：約 +5.6V
Reverse：約 -6.0V
```

5顆全新AA鹼性電池可能接近7.8～8.0V，因此BTS7960初期測試不應直接使用100% PWM。建議先將最大PWM限制在約70～75%，架高後輪測試，再依馬達電流、溫度與實際速度調整。

## 電源規劃

```text
5×AA電池
  │
  ├─ 主開關 → 保險絲 → BTS7960馬達電源 → 260S馬達
  │
  ├─ Mini-360 #1（5.0V）
  │     ├─ ESP32 VIN/5V
  │     ├─ BTS7960邏輯電源
  │     └─ 有源蜂鳴器
  │
  └─ Mini-360 #2（5.0V）
        └─ MG92B／MG90S Servo
```

電源原則：

- 馬達不得直接連接ESP32 GPIO。
- 馬達主電流不得經過麵包板、杜邦線或Mini-360。
- 所有模組GND必須共地。
- 兩塊Mini-360的5V輸出正極不可互接。
- Servo使用獨立5V支路，避免瞬間電流造成ESP32重啟。
- 每塊Mini-360接上元件前，必須先以萬用電表調整並確認為5.00V。
- Raspberry Pi 5未來需要獨立、可靠的大電流5V電源，不能由Mini-360供電。

## DualSense控制映射

### 轉向

DualSense左搖桿X軸：

```text
ps5.lx：-128 ～ +127
             ↓
steering：-1.0 ～ +1.0
```

原始搖桿值在 `-10～+10`（包含邊界）時視為中央，以消除搖桿放開後的正常類比偏差。

### 油門

```text
R2：0～255，代表前進
L2：0～255，代表倒車

throttle = R2 / 255.0 - L2 / 255.0
```

## 程式結構

目前Arduino草稿位於：

```text
sketch_sep28a/sketch_sep28a.ino
```

主要函式：

- `updateControls()`：讀取DualSense並完成標準化。
- `drive()`：所有控制來源共用的車輛控制入口。
- `failsafeStop()`：斷線時將轉向與油門命令歸零。
- `printControls()`：透過Serial Monitor輸出控制狀態。

目前`drive()`只保存標準化命令，尚未輸出至Servo或BTS7960。

## 執行與驗證

1. 安裝Arduino IDE 2.3.10。
2. 安裝 `esp32 by Espressif Systems` 3.3.6。
3. 安裝本專案使用的 `esp-ps5` 函式庫。
4. 開啟 `sketch_sep28a/sketch_sep28a.ino`。
5. Board選擇 `ESP32 Dev Module`。
6. 選擇ESP32所在的COM連接埠。
7. 編譯並上傳程式。
8. 開啟115200 baud的Serial Monitor。
9. 長按DualSense的Create＋PS約3～5秒，直到燈條快速脈衝閃爍。
10. 連線後操作左搖桿、R2與L2，確認輸出範圍與方向正確。
11. 關閉DualSense或離開連線範圍，確認Serial顯示Failsafe且`steering`、`throttle`歸零。

預期輸出範例：

```text
PS5 CONNECTED
CONNECTED | Steering: 0.00 | Throttle: 0.00
CONNECTED | Steering: -0.60 | Throttle: 0.35
PS5 DISCONNECTED -> FAILSAFE ACTIVE | Steering: 0.00 | Throttle: 0.00
```

## 安全注意事項

- 第一次馬達測試必須架高後輪。
- 上電前確認電源正負極、導通與短路狀態。
- Servo安裝連桿前先完成中心位置校正。
- 設定Servo左右軟體極限，避免steering rack卡死造成堵轉。
- 馬達與電池使用20 AWG左右的多股線；Servo電源使用22 AWG左右的多股線。
- 杜邦線只用於低電流控制訊號。
- 電池正極靠近電池處安裝保險絲。
- ESP32同時連接USB與外部5V前，必須確認開發板的電源路徑，避免5V回灌。
- INA3221照片所示模組量程約±1.638A，不可直接串入260S馬達主電流路徑。

## 開發路線

1. 完成並驗證標準化、Dead Zone及Failsafe。
2. 加入有源蜂鳴器狀態提示。
3. 完成Servo中心、方向與行程限制。
4. 完成BTS7960低PWM馬達輸出。
5. 整合 `drive(steering, throttle)`。
6. 加入電池電壓與低電流支路監控。
7. 建立Raspberry Pi至ESP32的UART控制協定。
8. 導入相機與電腦視覺。
9. 進行Donkey Simulator訓練與Sim-to-Real驗證。

## 專案結構

```text
RB18-Auto-Racing/
├─ README.md
├─ 元件清單.txt
└─ sketch_sep28a/
   └─ sketch_sep28a.ino
```

## 專案狀態

目前專案處於「DualSense輸入與底層控制介面」階段，尚未進入AI控制。現階段優先確保實體車的轉向、動力、電源與Failsafe可靠，再逐步擴充自主駕駛功能。
