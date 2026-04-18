
# 💻 MimiClaw v1.1.6 離線安裝與純指令 (CLI) 燒錄部署終極說明書

**直接使用命令列 (CLI) 是最純粹、最不容易出錯的開發方式。** 所有錯誤訊息都會原形畢露，沒有任何 GUI 介面包裝的黑盒子。我們直接用官方打包好的 Windows 專用離線安裝程式 (.exe)，它會幫你把底層環境一次裝好。

## TELEGRAM 機器人安裝教學參考
👉 [https://youtu.be/Ri938AvTOGc?si=rU7Amg0O3aOV-SI-](https://youtu.be/Ri938AvTOGc?si=rU7Amg0O3aOV-SI-)

---

### 零、補齊機密檔案 (新手必卡關點)

出於資訊安全與隱私，原作者的雲端程式碼**絕對不會**包含私人的 WiFi 密碼或 API 金鑰。當你下載別人的專案時，一定會缺少這個核心設定檔，導致編譯失敗（出現 `No such file` 錯誤）。

* 請進入解壓縮後的 `main/` 資料夾，點擊右鍵新增一個文字檔。
* 命名為 **`mimi_secrets.h`** (請注意：必須開啟 Windows 的「顯示副檔名」功能，確保它不是變成 `mimi_secrets.h.txt`)。
* 將以下程式碼貼入並填寫你的私人資料後存檔：
  ```c
  #ifndef MIMI_SECRETS_H
  #define MIMI_SECRETS_H

  #define WIFI_SSID           "你家的WiFi名稱"       // 僅支援 2.4GHz
  #define WIFI_PASS           "你家的WiFi密碼"
  #define OPENAI_API_KEY      "sk-你的OpenAI金鑰"
  #define TELEGRAM_BOT_TOKEN  "你的Telegram機器人Token"

  #endif
  ```

---

### 第一階段：下載與安裝 ESP-IDF (一路開綠燈)

1. **打開瀏覽器**：在網址列直接輸入官方專屬下載網址：
   👉 **`https://dl.espressif.com/dl/esp-idf/`**
2. **尋找「Offline Installer (離線安裝包)」**：
   * 進入網頁後往下捲動，請找到標題寫著 **`Offline Installer`** 的區塊 (不要點 Online Installer)。
3. **選擇正確的版本 (非常重要)**：
   * 為了配合 MimiClaw 和 ESP32-S3，請找到 **`ESP-IDF v5.1.4`** 或 **`ESP-IDF v5.3.5`**。
   * 點擊右邊的 **`Download`** 按鈕 (檔案約 1.5GB~2GB，請耐心等候)。
4. **執行安裝程式**：
   * 雙擊打開下載的 `.exe` 檔案。選擇 English，勾選同意條款，點選 Next。
   * **🚨 選擇安裝路徑 (生死交關的一步)**：**絕對不要改路徑！保持預設的短路徑！** 通常是 `C:\Espressif`。*(如果路徑有中文或空白鍵，之後編譯 100% 報錯死當)*。
   * **選擇要安裝的元件**：全部保持預設打勾，直接按 Next。
   * 點擊 **`Install`**。過程約 5~15 分鐘，完成後點擊 Finish。

---

### 第二階段：下載代碼與喚醒神聖的終端機

1. **下載代碼**：到 GitHub 網頁 -> [https://github.com/linhsisan/MimiClaw-ESP32S3CAM-Vision](https://github.com/linhsisan/MimiClaw-ESP32S3CAM-Vision) -> 下載 ZIP。
2. **解壓縮**：解壓到乾淨的路徑，例如 `C:\mimi_work\mimiclaw-1.1.6`。
3. **🚨 喚醒編譯環境 (非常重要)**：
   你必須使用 ESP-IDF 專屬環境，請用以下**兩種方法之一**來啟動：

   * **【方法一：使用專屬捷徑】(最簡單、強烈推薦)**
     1. 點擊 Windows 開始按鈕，搜尋 `ESP-IDF`。
     2. 雙擊打開 **`ESP-IDF 5.x CMD`** 捷徑。
     3. 畫面會跑幾行綠字，顯示 `Done! You can now compile ESP-IDF projects.` 代表成功。
     4. 輸入指令進入專案：`cd C:\mimi_work\mimiclaw-1.1.6`

   * **【方法二：使用一般 CMD 手動喚醒】**
     1. 打開普通的 CMD，進入專案路徑：`cd C:\mimi_work\mimiclaw-1.1.6`
     2. 執行 ESP-IDF 的環境配置檔：`C:\Espressif\frameworks\esp-idf-v5.3.5\export.bat` (請依你的實際版本調整路徑)。

---

### 第三階段：晶片對齊與藍色設定選單 (Menuconfig)

在 CLI 模式下，必須先告訴編譯器「你要燒給哪顆晶片」，才能進去設定參數。**執行順序不可以亂！**

1. **設定目標晶片 (Target)**：
   ```cmd
   idf.py set-target esp32s3
   ```
2. **進入設定介面 (Menuconfig)**：
   ```cmd
   idf.py menuconfig
   ```

**⌨️ 藍色介面操作指南：**
* 按 **`↑` `↓` 方向鍵** 上下移動。 / 按 **`Enter`** 進入子選單。
* 按 **`空白鍵 (Space)`** 勾選或取消勾選 (出現 `[*]` 代表開啟)。
* 按 **`Esc`** 回到上一頁。 / 按 **`S`** 存檔，按 **`Q`** 離開。

**🚨 必須在 Menuconfig 裡對齊的關鍵參數：**

1. **分割區 (Partition Table)**
   * 進入 `Partition Table` -> 選 `Custom partition table CSV`。
   * 下方的 `Custom partition CSV file` 確認文字是 `partitions.csv`。
2. **PSRAM (Component config -> ESP32S3-Specific)**
   * 勾選 `[*] Support for external, SPI-connected RAM`。
   * 進入 `SPI RAM config` -> Type 選擇 **`Octal Mode PSRAM`**，Speed 選擇 **`80MHz`**。
3. **Flash 容量 (Serial flasher config)**
   * `Flash size` -> 改為 **`16 MB`**。
   * `Flash SPI speed` -> 改為 **`80 MHz`**。
4. **WiFi (Example Connection Configuration)**
   * 填寫你的 WiFi SSID 與 Password。
5. **⚙️ ESP System Settings (系統輸出通道)**
   *(決定晶片的日誌從哪個孔噴出來。若設錯，Monitor 會一片空白)*
   * 路徑：`Component config` -> `ESP System settings`
   * `Channel for console output` -> 選擇 **`Default: UART0`**。
   * `Channel for console input` -> 選擇 **`UART0`**。
6. **🛰️ UART 燒錄與通信 (Serial Flasher Config)**
   *(決定燒錄穩定度。若傳輸線普通，降低速度是保命符)*
   * 路徑：`Serial flasher config`
   * `Flash baud rate` -> 預設 `460800`。若燒錄常斷線，手動降為 **`115200`**。

設定完畢後，**按 `S` 存檔，按 `Enter` 確認，最後按 `Q` 退出藍色介面。**

---

### 第四階段：確認硬體通訊埠 (COM)

1. 拿出 Type-C 傳輸線，插在開發板的 **UART / COM 孔**，接上電腦。
2. 對 Windows 開始按鈕按右鍵 -> 打開 **裝置管理員** -> 展開 **連接埠 (COM 和 LPT)**。
3. 記下你的開發板編號，例如 **`COM3`**。

---

### 第五階段：CLI 暴力美學（編譯與雙重燒錄）

直接用終端機指令一口氣完成清理、編譯、雙重燒錄的動作。*(假設硬體是 COM3，請自行替換)*

1. **清除舊垃圾 (防呆)**：`idf.py fullclean`
2. **編譯主程式 (Build)**：`idf.py build` *(這會跑 5~10 分鐘，綠色 `Project build complete` 即成功)*
3. **燒錄主程式 (Flash)**：`idf.py -p COM3 flash` *(看到 `Leaving... Hard resetting via RTS pin...` 代表完成)*

*💡 **大神的終極複合指令：*** 如果你以後修改了程式碼，只需要輸入這行，就會自動「編譯 -> 燒錄 -> 打開監控器」：
```cmd
idf.py -p COM3 build flash monitor
```

---

### 第六階段：開啟監控器 (Monitor) 與退出方法

程式燒進去後，要在終端機裡看它有沒有正常運作：

1. **開啟監控介面**：
   ```cmd
   idf.py -p COM3 monitor
   ```
2. **驗證成功的三句話** (看到代表完美誕生)：
   * `I (...) esp_psram: Adding ... bytes of PSRAM to heap` (記憶體掛載成功)
   * `I (...) mimi_main: SPIFFS mounted successfully` (定時規則掛載成功)
   * `I (...) mimi_main: MimiClaw V1.1.6 Fully Operational!` (版本正確)
3. **🛑 如何退出監控器？**
   在 CLI 裡不能直接按叉叉關閉，請按鍵盤快捷鍵：**`Ctrl` + `]`** (右中括號) 安全退出。

---
---

# ⚠️ 附錄：WiFi密碼設定後還是無法登錄？(必須清除全部洗乾淨)

這是一個非常經典的 ESP32 開發陷阱：**「NVS 記憶體幽靈 (NVS Ghosting)」**！
當你第一次忘記填密碼並燒錄進去時，晶片會把「空白密碼」寫入隱藏的 NVS 分割區。第二次你在 `menuconfig` 填好密碼重新燒錄時，**一般的 `flash` 指令是不會洗掉 NVS 區塊的！** 導致永遠連不上。

解決方法是進行**「物理級的全面洗腦 (Erase Flash)」**：

### 第一步：喚醒環境 (日常起手式)
確認你的 CMD 已經處於喚醒狀態 (若沒有，請執行 `export.bat` 或打開 ESP-IDF CMD 捷徑，並 `cd` 到專案資料夾)。

### 第二步：執行「全面洗腦」指令 (最關鍵)
接上開發板，我們要把晶片裡的所有分區（包含卡住的舊密碼）全部炸毀：
```cmd
idf.py -p COM3 erase-flash
```
*(花費約 10~15 秒，看到 `Erasing flash... Done` 代表晶片已完全失憶。)*

### 第三步：清除電腦端的舊快取 (防呆)
把電腦裡卡住的編譯器舊設定徹底清空：
```cmd
idf.py fullclean
```

### 第四步：最後一次參數檢查 (防手滑陷阱)
重新打開設定選單：`idf.py menuconfig`
**🚨 兩個超級常犯的致命錯誤：**
1. **WiFi 密碼的空白鍵**：確認填寫 SSID 和 Password 時，**結尾絕對沒有不小心多打一個空白鍵 (Space)**。
2. **API Key 的位置**：確認 API Key 真的是在 `menuconfig` 填寫嗎？請確認是否應該填在 `main/mimi_secrets.h` 裡。(檢查完畢後，按 `S` 存檔，`Q` 離開。)

### 第五步：終極大一統燒錄
晶片洗腦完畢、快取清除後，用終極指令把所有東西一次寫入並打開監控：
```cmd
idf.py set-target esp32s3
idf.py -p COM3 build flash monitor
```
*(盯著黑畫面日誌，這次因為 NVS 是乾淨的，一定能看到 `WIFI: Connected` 以及 API 連線成功的訊息了！)*