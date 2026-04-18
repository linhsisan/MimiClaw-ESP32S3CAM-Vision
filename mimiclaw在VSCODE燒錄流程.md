# 🎥 MimiClaw v1.1.6 終極手冊：從零到一完整部署 (ZIP 離線整合版)

## ⚠️ 零階段：絕對避坑守則 (開工前必看)
1. **路徑守則**：你的所有安裝路徑、專案資料夾，**絕對不可以有中文、全形字元或空格**。（❌ `C:\我的專案`、❌ `C:\mimi space` -> ✅ `C:\mimi_work`）。
2. **防毒軟體**：編譯過程中會產生大量零碎檔案，請暫時關閉防毒軟體或將專案資料夾加入白名單，否則編譯極度緩慢甚至報錯。
3. **傳輸線確認**：確保你的 Type-C 線是「資料傳輸線」，而不是只能充電的劣質線。

---

## 第一階段：ESP-IDF 離線環境大建 (電腦不留痕跡)
**目標：使用官方整合包，不需手動安裝 Git 或 Python，一鍵搞定底層。**

1. **下載離線包**：
   * 前往 Espressif 官方下載頁面 (搜尋 `ESP-IDF Offline Installer`)。
   * 下載 **`ESP-IDF v5.1.4 Offline Installer`**（檔案約 1GB 左右）。
2. **執行安裝**：
   * 點擊執行，安裝路徑請保持簡短（強烈建議裝在 `C:\Espressif`）。
   * 一路按「下一步」直到安裝完成。這包軟體已經內建了專屬的編譯器，不會弄亂你電腦原有的環境。
3. **安裝 VS Code 與核心外掛**：
   * 下載並安裝 VS Code。
   * 在 VS Code 左側的「延伸模組 (Extensions)」搜尋並安裝 **`Espressif IDF`**。

---

## 第二階段：精準下載代碼與 VS Code 環境對接
**目標：拿到正確的 v1.1.6 代碼，並讓 VS Code 認得你的離線編譯器。**

1. **下載 v1.1.6 ZIP 檔**：
   * 開啟你的 MimiClaw GitHub 網頁。
   * 點擊左上角的 `Branch: main` 按鈕，切換到 **`Tags`** 分頁。
   * 點擊 **`v1.1.6`**。
   * 點擊綠色的 **`Code`** 鈕 -> **`Download ZIP`**。
2. **解壓縮專案**：
   * 將下載的壓縮檔解壓至 `C:\mimi_work\mimiclaw-1.1.6`。
   * 在 VS Code 點擊 `File` -> `Open Folder`，打開這個資料夾。
3. **VS Code 環境對接 (極度重要)**：
   * 按下鍵盤 `F1`，輸入 `ESP-IDF: Configure ESP-IDF Extension`。
   * 🚨 **關鍵選擇**：這次請選擇 **`Use existing setup` (使用現有設定)**。
   * VS Code 會自動偵測到你剛才安裝在 `C:\Espressif` 的工具，下方會亮起綠色勾勾顯示配置完成。

---

## 第三階段：硬體連線與狀態列對齊 (通訊協議)
**目標：讓電腦與開發板正確溝通。**

1. **硬體接線**：
   * ESP32-S3 板子通常有兩個 Type-C 孔。請務必插在印有 **`UART`** 或 **`COM`** 的孔（另一個是 USB/OTG，絕對不要插錯）。
2. **確認 COM 埠**：
   * 打開 Windows「裝置管理員」 -> 「連接埠 (COM 和 LPT)」。記下晶片所在的編號（例如 `COM3`）。
3. **VS Code 狀態列對齊 (請看畫面最下方藍色橫條)**：
   * **Select Port (插頭圖示)**：點擊後選擇你的 `COM3`。
   * **Select Target (晶片圖示)**：點擊後選擇 **`esp32s3`**。接著跳出選單請選 `ESP-IDF: esp32s3`。
   * **Flash Method (閃電旁邊的設定)**：點擊並選擇 **`UART`**。（🚨 絕不能選 JTAG）。

---

## 第四階段：Menuconfig 全參數絕對對齊 (最核心關鍵)
**目標：開啟大腦所需的記憶體與分區空間，錯一步開機必死當。**

點擊 VS Code 下方狀態列的 **「齒輪圖示 ⚙️ (SDK Configuration Editor)」**。請逐一搜尋並修改下列參數：

### 1. 分割區表對齊 (讓系統能燒錄靈魂檔)
* 搜尋 `Partition Table`
* 將 `Partition Table` 設定改為 -> **`Custom partition table CSV`**
* `Custom partition CSV file` -> 輸入 **`partitions.csv`**

### 2. 外部記憶體 PSRAM 對齊 (Mimi 運算必備，沒開必當機)
* 搜尋 `Support for external, SPI-connected RAM` -> **勾選 (啟用)**
* 搜尋 `SPI RAM config`
* `Mode (SRAM Type)` -> 選擇 **`Octal Mode PSRAM`**。
* `PSRAM Clock Speed` -> 選擇 **`80MHz`**。

### 3. Flash 閃存對齊 (防止程式裝不下)
* 搜尋 `Serial flasher config`
* `Flash SPI speed` -> 選擇 **`80MHz`**。
* `Flash size` -> 🚨 **選擇 `16MB`**。

### 4. 網路與基本設定
* 搜尋 `WiFi SSID` -> 填入你的網路名稱。
* 搜尋 `WiFi Password` -> 填入你的網路密碼。

設定完成後，點擊畫面上方的 **`Save`**。

---

## 第五階段：雙重燒錄與靈魂注入 (Build & Flash)


---

### 第一步：喚醒環境 (日常起手式)
請先確認你的黑畫面 (CMD) 已經處於喚醒狀態。
1. 進入專案：`cd C:\你的專案路徑\MimiClaw`
2. 執行喚醒：`C:\espidf\v5.3.5\esp-idf\export.bat`
*(看到 `Done!` 即可繼續)*

### 第二步：執行「全面洗腦」指令 (最關鍵)
接上開發板，確認你的 COM 埠（假設是 COM3）。我們要把晶片裡的所有分區（包含那個卡住的舊密碼）全部炸毀、恢復原廠空白狀態。

在終端機輸入：
```cmd
idf.py -p COM3 erase-flash
```
*(這大概會花 10 到 15 秒。當你看到 `Erasing flash... Done` 的字樣，代表晶片已經完全失憶了。)*

### 第三步：清除電腦端的舊快取 (防呆)
有時候電腦裡的編譯器也會卡住舊的設定。我們把它徹底清空：
```cmd
idf.py fullclean
```

### 第四步：最後一次參數檢查 (防手滑陷阱)
重新打開設定選單：
```cmd
idf.py menuconfig
```
**🚨 這裡有兩個超級常犯的致命錯誤，請務必檢查：**
1. **WiFi 密碼的空白鍵**：確認你在填寫 SSID 和 Password 時，**結尾絕對沒有不小心多打一個空白鍵 (Space)**。ESP32 會把空白鍵當作密碼的一部分！
2. **API Key 的位置**：確認 API Key 真的是在 `menuconfig` 裡面填寫嗎？很多開源專案的 API Key 其實是要你打開原始碼裡面的 `main/mimi_config.h` 或 `secrets.h` 手動貼上的。請確認你填對了地方。
*(檢查完畢後，按 `S` 存檔，`Q` 離開。)*

### 第五步：終極大一統燒錄
既然晶片已經被洗腦，快取也清除了，我們現在用一行指令把所有東西（程式、定時規則、監視器）一次性全部寫進去：

```cmd
idf.py -p COM3 build flash storage-flash monitor
```

**這個流程的運作邏輯是：**
1. `build`：用你剛剛確認過的新密碼重新編譯。
2. `flash`：把全新的程式寫入剛被洗腦的乾淨晶片中。
3. `storage-flash`：把靈魂檔（定時規則）重新寫進去。
4. `monitor`：立刻打開日誌監視器。

盯著你的黑畫面日誌，這次因為 NVS 是乾淨的，程式會強制讀取你剛編譯進去的新密碼。你一定能看到 `WIFI: Connected` 以及 API 連線成功的訊息了！

---

## 第六階段：終極驗證 (Monitor)
點擊狀態列的 **小螢幕圖示 📺 (Monitor)** 打開日誌。

**你必須看到這三行才算完美成功**：
1. `I (...) esp_psram: Adding ... bytes of PSRAM to heap` (證明 PSRAM 參數設定正確)
2. `I (...) mimi_main: SPIFFS mounted successfully` (證明分區與 storage-flash 成功)
3. `I (...) mimi_main: MimiClaw V1.1.6 Fully Operational!` (證明你下載的是正確的 v1.1.6 版本)

現在，這份手冊包含了從「免 Git 環境搭建」到「最後一個硬體參數對齊」的所有細節。你可以完全依靠這份內容完成實機部署！