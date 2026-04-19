廠長來了！阿三老闆，您說得對，這份舊的手冊混合了命令列（CMD）和 VS Code 的操作，對於不熟悉指令的初學者來說，絕對會看得一頭霧水。

既然您希望大家都能用最直觀的 **VS Code 介面** 來完成安裝和燒錄，而且不要講太深奧的底層程式碼（JSON payload 那些我們一律藏起來！），我們就把整份手冊大翻修，變成「純 VS Code 傻瓜圖文版」！

這份重新編排的《MimiClaw V1.1.6 初學者終極手冊》，保證連新手都能一路順暢點擊到成功：

---

# 🦞 MimiClaw V1.1.6 終極手冊：VS Code 純圖形化一鍵安裝版

這是一份專為初學者設計的教學，全程只需使用滑鼠點擊和 VS Code，不需要打任何複雜的指令！

## ⚠️ 零階段：開工前必看「防雷守則」
1. **路徑不能有中文**：您的安裝路徑、專案資料夾，**絕對不可以**有中文、全形字元或空格。（❌ `C:\我的專案` -> ✅ `C:\mimi_work`）。
2. **防毒軟體請冷靜**：編譯會產生大量小檔案，請暫時關閉防毒軟體，否則速度會超級慢甚至失敗。
3. **線材確認**：請確保您的 Type-C 線是「資料傳輸線」，而不是買手機送的只能充電的劣質線。
4. **硬體供電**：如果您接了螢幕或擴充板，請確認它是接 **5V** 電源，不要接成 3.3V。

---

## 🛠️ 第一階段：下載並安裝 ESP-IDF (離線整合包)
*目標：安裝核心工具，不需要手動裝 Git 或 Python。*

1. **下載離線包**：前往 Espressif 官方下載頁面，下載 **ESP-IDF v5.1.4 Offline Installer**（檔案約 1GB）。
2. **執行安裝**：一路按「下一步」。**強烈建議**安裝路徑設為極簡的 `C:\Espressif`。
3. **準備 VS Code**：打開 VS Code，在左側「延伸模組 (Extensions)」搜尋並安裝 **Espressif IDF**。

---

## 📂 第二階段：下載專案與 VS Code 綁定
*目標：把 MimiClaw 專案放進來，並讓 VS Code 認識編譯器。*

1. **下載 V1.1.6 專案**：
   * 到 MimiClaw 的 GitHub 網頁。
   * 點擊左上角的 `Branch: main` -> 切換到 `Tags` -> 選擇 **`v1.1.6`**。
   * 點擊綠色的 `Code` 按鈕 -> 選擇 **Download ZIP**。
2. **解壓縮**：將 ZIP 檔解壓縮到一個沒有中文的資料夾，例如 `C:\mimi_work\mimiclaw`。
3. **在 VS Code 打開專案**：在 VS Code 點擊 `File` -> `Open Folder`，選擇剛剛解壓縮的資料夾。
4. **環境綁定 (關鍵點)**：
   * 按下鍵盤 `F1`，輸入並選擇 `ESP-IDF: Configure ESP-IDF Extension`。
   * 🚨 **重要**：在設定畫面中選擇 **Use existing setup** (使用現有設定)。VS Code 會自動找到您安裝在 `C:\Espressif` 的工具，等它顯示配置完成即可。

---

## 🔌 第三階段：硬體連線與 VS Code 狀態列對齊
*目標：讓電腦知道您的晶片接在哪個孔。*

1. **接線**：將 ESP32-S3 接上電腦。板子上有兩個 Type-C 孔，請務必插在印有 **UART 或 COM** 的那個孔。
2. **尋找狀態列**：請看 VS Code 畫面的 **最下方藍色（或紫色）橫條**。
3. **依序點擊設定**：
   * **插頭圖示 (Select Port)**：點擊後，在上方跳出的清單選擇您的 COM 埠（例如 COM3）。
   * **晶片圖示 (Select Target)**：點擊後，選擇 `esp32s3`。接著如果跳出子選單，請選 `ESP-IDF: esp32s3`。
   * **閃電旁邊的齒輪 (Flash Method)**：點擊並選擇 **UART**。（🚨 絕對不能選 JTAG）。

---

## ⚙️ 第四階段：核心參數設定 (Menuconfig)
*目標：開啟晶片記憶體，填寫您的 Wi-Fi 與 MQTT 資訊。*

1. 點擊 VS Code 下方狀態列的 **「齒輪圖示 ⚙️ (SDK Configuration Editor)」**，這會打開圖形化設定介面。
2. 在上方的搜尋框中，**逐一搜尋並修改**以下設定：

   * **[分區表設定]**
     * 搜尋 `Partition Table` -> 將設定改為 **Custom partition table CSV**
     * 搜尋 `Custom partition CSV file` -> 輸入 **`partitions.csv`**
   * **[記憶體設定 - 沒開必當機]**
     * 搜尋 `Support for external, SPI-connected RAM` -> **打勾 (啟用)**
     * 搜尋 `SPI RAM config` -> Mode 選擇 **Octal Mode PSRAM**，Speed 選擇 **80MHz**。
   * **[容量設定]**
     * 搜尋 `Flash SPI speed` -> 選擇 **80MHz**。
     * 搜尋 `Flash size` -> 🚨 選擇 **16MB**。
   * **[網路與通訊設定]**
     * 搜尋 `WiFi SSID` -> 填寫網路名稱。
     * 搜尋 `WiFi Password` -> 填寫密碼 (🚨注意：結尾千萬不能有空格！)。
     * 搜尋 `Device ID` -> 填寫您專屬的設備代號 (這非常重要，通訊必備！)。
     * 搜尋 `MQTT Topic` -> 填寫對應的通訊主題。
3. 🚨 設定全部填完後，請務必點擊畫面右上方的 **「Save」** 存檔！

---

## 🚀 第五階段：一鍵燒錄 (Build, Flash & Monitor)
*目標：清除舊資料，將新程式與靈魂檔一次燒進去。*

1. **徹底洗腦 (重要防呆)**：
   * 點擊下方狀態列的 **「垃圾桶圖示 🗑️ (ESP-IDF Full Clean)」**，清除所有舊快取。
   * 點擊下方狀態列的 **「橡皮擦圖示 🧼 (Erase Flash)」**，這會把晶片完全清空（約需 10 秒）。
2. **一鍵燒錄**：
   * 點擊下方狀態列的 **「火焰圖示 🔥 (Build, Flash and Monitor)」**。
   * 接下來請泡杯咖啡，系統會自動開始編譯並寫入程式，第一次大約需要 3-5 分鐘。
3. **寫入靈魂檔 (Storage Flash)**：
   * 為了讓定時規則等檔案生效，按下 `F1`，輸入並選擇 **`ESP-IDF: Flash SPIFFS`**。

---

## ✅ 第六階段：終極驗證
當一切燒錄完成，VS Code 下方會自動跳出一個黑色的監控視窗 (Monitor)。
如果您看到以下關鍵字樣，代表您已經大功告成！

* `I (...) esp_psram: Adding ... bytes of PSRAM to heap` (記憶體掛載成功)
* `WIFI: Connected` (網路連線成功)
* `I (...) mimi_main: SPIFFS mounted successfully` (靈魂檔讀取成功)
* `I (...) mimi_main: MimiClaw V1.1.6 Fully Operational!` (系統完美啟動)

恭喜您，您的 MimiClaw 已經成功上線服役！