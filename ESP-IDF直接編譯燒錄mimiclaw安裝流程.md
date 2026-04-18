 ** MimiClaw v1.1.6 離線安裝與純指令 (CLI) 燒錄部署終極說明書**。
---
** 我們直接用官方打包好的 Windows 專用離線安裝程式 (.exe)，它會幫你把底層環境一次裝好。**

### 第一階段：到底哪裡可以下載？(精準尋寶路線)

1. **打開瀏覽器**：
   在網址列直接輸入這個官方專屬下載網址（這是他們專門放 Windows 安裝檔的秘密基地）：
   👉 **`https://dl.espressif.com/dl/esp-idf/`**
   *(或者 Google 搜尋 `ESP-IDF Windows Installer`，點擊第一個 Espressif 的連結)*

2. **尋找「Offline Installer (離線安裝包)」**：
   * 進入網頁後，**往下捲動**。
   * 不要點上面的 Online Installer（線上安裝包很容易因為網路問題斷線失敗）。
   * 請找到標題寫著 **`Offline Installer`** 的區塊。

3. **選擇正確的版本 (非常重要)**：
   * 你會看到很多版本（v5.4, v5.3, v5.1...）。
   * 為了配合 MimiClaw 和 ESP32-S3，請找到 **`ESP-IDF v5.1.4`** 或 **`ESP-IDF v5.3.5`**。
   * 點擊右邊的 **`Download`** 按鈕。
   * *(⚠️ 注意：這個檔案非常大，大約 1.5GB 到 2GB，因為它把所有需要的編譯器都包在裡面了，請耐心等它下載完。)*

## TELEGRAM 機器人安裝教學
https://youtu.be/Ri938AvTOGc?si=rU7Amg0O3aOV-SI-
---

### 第二階段：如何安裝？(一路開綠燈)

下載完那個巨大的 `.exe` 檔案後，請照著以下步驟安裝：

1. **啟動安裝程式**：
   雙擊打開你剛下載的 `.exe` 檔案。

2. **同意條款與選擇語言**：
   選擇 English，勾選同意條款，點選 Next。

3. **🚨 選擇安裝路徑 (生死交關的一步)**：
   * 畫面會問你要安裝在哪裡。
   * **絕對不要改路徑！保持預設的短路徑！** 通常是 `C:\Espressif`。
   * *(如果你的路徑裡有中文字、空白鍵，例如 `C:\Users\我的電腦\...`，之後編譯 100% 會報錯死當。)*

4. **選擇要安裝的元件**：
   * 畫面會列出一堆勾選框（ESP-IDF, ESP-IDF Tools 等）。
   * **全部保持預設打勾**，什麼都不要動，直接按 Next。

5. **開始安裝**：
   * 點擊 **`Install`**。
   * 去泡杯咖啡，這個過程大約需要 5 到 15 分鐘。安裝程式會自動幫你把 Python、Git、C 語言編譯器全部配置好，並且**不會**弄亂你電腦原本的環境。

6. **完成安裝**：
   * 看到綠色的 `Completed` 後，點擊 Finish 關閉視窗。

---

### 第三階段：如何驗證安裝成功？(尋找神聖的黑畫面)

安裝完畢後，你要怎麼知道它活過來了？

1. **回到 Windows 桌面**。
2. 仔細看，桌面上一定多出了一個名叫 **`ESP-IDF 5.x CMD`** 的黑色捷徑圖示！
   *(如果桌面沒有，點擊左下角 Windows「開始」按鈕，直接打字搜尋 `ESP-IDF` 就會出來)*。
3. **雙擊打開這個黑色捷徑**。
4. 如果黑畫面彈出來，跑了幾行字後，最後停在綠色的這句話：
   **`Done! You can now compile ESP-IDF projects.`**

🎉 **恭喜你！這代表 ESP-IDF 已經完美安裝在你的電腦裡了！**

---

**直接使用命令列 (CLI) 是最純粹、最不容易出錯的開發方式。** 所有錯誤訊息都會原形畢露，沒有任何 GUI 介面包裝的黑盒子。

---

# 💻 MimiClaw v1.1.6：純命令列 (CLI) 離線部署全攻略

## 第一階段：環境大建 (脫離 VS Code 的第一步)
我們一樣要使用離線安裝包，但這次我們完全不需要打開 VS Code。

1. **下載離線包**：前往 Espressif 官網下載 **`ESP-IDF v5.1.4 Offline Installer`**。
2. **執行安裝**：雙擊安裝，路徑設定為 `C:\Espressif`。一路按「下一步」直到完成。
3. **🚨 尋找神聖的終端機**：
   安裝完成後，你的桌面或 Windows 開始選單會出現一個名為 **`ESP-IDF 5.1 CMD`** 的黑色捷徑。
   *(這不是普通的 cmd，這是已經幫你把所有編譯器路徑都對齊好的專屬終端機。接下來所有的指令，都在這個黑畫面裡打！)*

---

## 第二階段：下載代碼與進入基地
1. **下載代碼**：到 GitHub 網頁 ->  https://github.com/linhsisan/MimiClaw-ESP32S3CAM-Vision -> 下載 ZIP。
2. **解壓縮**：解壓到乾淨的路徑，例如 `C:\mimi_work\mimiclaw-1.1.6`。
3. **導航進入專案**：
   打開你剛剛找到的 **`ESP-IDF 5.1 CMD`** 黑畫面，輸入以下指令並按 Enter：
   
   ```cmd
   cd C:\mimi_work\mimiclaw-1.1.6
   ```

**在目前的視窗裡 CMD (命令提示字元)：**
`C:\mimi_work\mimiclaw-1.1.6` 路徑下
請找出你的 ESP-IDF 安裝路徑，並執行裡面的 export.bat。例如執行：
```cmd
C:\Espressif\frameworks\esp-idf-v5.1.4\export.bat
```

**方法一：使用 ESP-IDF 專屬終端機（最簡單、強烈推薦）**
你現在用的是 Windows 內建的普通 CMD，請把它關掉，改用安裝包幫你準備好的「專屬黑畫面」。

1. 點擊你的 Windows 開始按鈕。
2. 搜尋 ESP-IDF。
3. 你會看到一個類似 **ESP-IDF 5.1 CMD** 或 **ESP-IDF 5.1 PowerShell** 的捷徑，點擊打開它。
   *(打開時，它會跑幾行綠色的字，顯示 "Done! You can now compile ESP-IDF projects."，這就代表喚醒成功了！)*

在這個專屬的黑畫面裡，重新導航到你的專案目錄：
```cmd
cd C:\Users\USER\Downloads\MimiClaw-ESP32S3CAM-Vision-main

在這個路徑專案底下建立ESP-IDF的環境。

C:\Espressif\esp-idf-v5.3.5\export.bat
```


## 第三階段：晶片對齊與藍色設定選單 (Menuconfig)
在 CLI 模式下，我們必須先告訴編譯器「你要燒給哪顆晶片」，才能進去設定參數。

很重要: 執行順序不可以亂

1. **設定目標晶片 (Target)**： 
   在黑畫面輸入：
   ```cmd
   idf.py set-target esp32s3
   ```
   *(它會跑一段文字，告訴你晶片已經切換為 S3。)*
2. **進入設定介面 設定各種參數UART P PSRAM TFT..... (Menuconfig)**：
   接著輸入：
   ```cmd
   idf.py menuconfig
   ```
   *(此時黑畫面會變成一個復古的藍底白字介面。)*

**⌨️ 藍色介面操作指南：**
* 按 **`↑` `↓` 方向鍵** 上下移動。
* 按 **`Enter`** 進入子選單。
* 按 **`空白鍵 (Space)`** 勾選或取消勾選 (出現 `[*]` 代表開啟)。
* 按 **`Esc`** 回到上一頁。
* 按 **`S`** 存檔，按 **`Q`** 離開。

### 🚨 必須在 Menuconfig 裡對齊的關鍵參數：

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
   這決定了晶片的日誌從哪個孔噴出來。若設錯，Monitor 會一片空白。
   * **路徑：** `Component config` -> `ESP System settings`
   * **設定項：** `Channel for console output`
   * **正確設定：** 選擇 **`Default: UART0`**。
   * **設定項：** `Channel for console input`
   * **正確設定：** 選擇 **`UART0`**。

6. **🛰️ UART 燒錄與通信 (Serial Flasher Config)**
   這決定了燒錄時的穩定度。若你的傳輸線普通，降低速度是唯一的保命符。
   * **路徑：** `Serial flasher config`
   * **設定項 1：`Flash baud rate` (燒錄速度)**
     * **專業建議：** 預設 `460800`。若燒錄時常斷線，請手動降為 **`115200`**。
   * **設定項 2：`Flash size` (Flash 容量)**
     * **正確設定：** 選擇 **`16 MB`**。
   * **設定項 3：`Flash SPI speed`**
     * **正確設定：** 選擇 **`80 MHz`**。

7. **記憶體與分區 (對應 MimiClaw 硬體)**
   * **Partition Table**：進入後選 `Custom partition table CSV`，確認名稱填入 **`partitions.csv`**。
   * **Component config** -> **ESP32S3-Specific**：勾選 `Support for external, SPI-connected RAM`。
   * **SPI RAM config**：Type 選 **`Octal Mode PSRAM`**，Speed 選 **`80MHz`**。

設定完畢後，**按 `S` 存檔，按 `Enter` 確認，最後按 `Q` 退出藍色介面。**

---

## 第四階段：確認硬體通訊埠 (COM)
1. 拿出 Type-C 傳輸線，插在開發板的 **UART / COM 孔**，接上電腦。
2. 對 Windows 開始按鈕按右鍵 -> 打開 **裝置管理員** -> 展開 **連接埠 (COM 和 LPT)**。
3. 記下你的開發板編號，例如 **`COM3`**。

---

## 第五階段：CLI 暴力美學（編譯與雙重燒錄）
現在，我們直接用終端機指令一口氣完成清理、編譯、雙重燒錄的動作。
*(以下指令假設你的硬體是 COM3，請自行替換成你實際的數字)*

1. **清除舊垃圾 (防呆)**：
   ```cmd
   idf.py fullclean
   ```

2. **編譯主程式 (Build)**：
   ```cmd
   idf.py build
   ```
   *(這會跑 5~10 分鐘，如果最後出現綠色的 `Project build complete` 就是成功。)*

3. **燒錄主程式 (Flash)**：
   ```cmd
   idf.py -p COM3 flash
   ```
   *(看到 `Leaving... Hard resetting via RTS pin...` 代表主程式燒錄完成。)*

4. **編譯+燒錄：**
   ```cmd
   idf.py -p COM3 build flash monitor
   ```
   *(看到 `Hash of data verified` 代表 MEMORY.md 成功寫入。)*

---

## 第六階段：開啟監控器 (Monitor) 與退出方法
程式燒進去後，我們要在終端機裡看它有沒有正常運作：

1. **開啟監控介面**：
   ```cmd
   idf.py -p COM3 monitor
   ```
   這時候黑畫面會瘋狂跳出白色的系統日誌 (Log)。

2. **驗證成功的三句話**：
   盯著螢幕，如果你看到這三行，恭喜你，完美的 v1.1.6 已經誕生：
   * `I (...) esp_psram: Adding ... bytes of PSRAM to heap` (記憶體掛載成功)
   * `I (...) mimi_main: SPIFFS mounted successfully` (定時規則掛載成功)
   * `I (...) mimi_main: MimiClaw V1.1.6 Fully Operational!` (版本正確)

3. **🛑 如何退出監控器？**
   在 CLI 裡，你不能直接按叉叉關閉。請按鍵盤上的快速鍵：
   **`Ctrl` + `]`** (右中括號)
   這樣就能安全退出監控模式，回到你可以輸入指令的狀態。

---

### 💡 為什麼 CLI 是大神的選擇？
如果你以後修改了 `main.c` 的程式碼，你不需要再走一次那麼長的流程。你只需要在這個黑畫面裡輸入終極複合指令：
```cmd
idf.py -p COM3 build flash monitor
```
這**一行指令**就會自動幫你「編譯 -> 燒錄 -> 打開監控器」，這是 VS Code 給不了的純粹與高效。照著這套 CLI 流程，沒有任何隱藏的設定能干擾你！

---

### WiFi密碼設定後還是無法登錄必須清除全部洗乾淨

這是一個非常經典的 ESP32 開發陷阱！你絕對不是第一個遇到這個問題的人。你現在遇到的狀況叫做 **「NVS 記憶體幽靈 (NVS Ghosting)」**。

ESP32 是一種很「固執」的晶片。當你第一次忘記填密碼並燒錄進去時，晶片會把這個「空白的密碼」寫入一個叫做 **NVS (Non-Volatile Storage，非揮發性儲存區)** 的隱藏分割區裡。當你第二次在 `menuconfig` 填好密碼並重新燒錄 (Flash) 時，**一般的 `flash` 指令是不會去洗掉 NVS 區塊的！** 所以開機時，程式還是會傻傻地去讀取第一次存下來的「空白密碼」，導致永遠連不上。

要解決這個問題，我們必須對晶片進行**「物理級的全面洗腦 (Erase Flash)」**，然後重新乾淨地燒錄一次。

以下是純 CLI 環境下的終極除錯與重建流程：

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
idf.py set-target esp32s3
idf.py -p COM3 build flash monitor
```

**這個流程的運作邏輯是：**
1. `build`：用你剛剛確認過的新密碼重新編譯。
2. `flash`：把全新的程式寫入剛被洗腦的乾淨晶片中。
3. `storage-flash`：把靈魂檔（定時規則）重新寫進去。
4. `monitor`：立刻打開日誌監視器。

盯著你的黑畫面日誌，這次因為 NVS 是乾淨的，程式會強制讀取你剛編譯進去的新密碼。你一定能看到 **`WIFI: Connected`** 以及 API 連線成功的訊息了！