# MimiClaw-AI-Assistant

這是基於 ESP32-S3 的智慧 AI 助手專案，具備 Telegram 控制、TFT 氣泡對話顯示與環境監測功能。

## 📌 致謝 (Credits)

本專案是基於 [memovai/mimiclaw] 的原始程式碼進行深度修改與優化。
- **原始專案位址**: [原作者的 GitHub 連結](https://github.com/memovai/mimiclaw)]
- **原作者**: [memovai]

中文的聊天，繁體中文的界面本專案是基於 [twtomato/mimiclaw-lcd] 的fork程式碼進行深度修改與優化。
- **fork專案位址**: [張紹文 GitHub 連結](https://github.com/twtomato/mimiclaw-lcd/tree/main/main)]
- **fork作者**: [張紹文]


感謝原作者以及fork創客 提供的優秀基礎架構，讓我能在此基礎上開發出更多功能。

## 🚀 主要修改與優化
- 修正了 TFT 螢幕顯示邏輯，支援中文氣泡對話。
- 優化了 LLM (GPT-4o-mini) 的工具調用與即時回報機制。
- 解決了 DHT 溫濕度感測器在無實體上拉電阻環境下的時序問題。
- 增加了馬達控制與計時器的非同步回傳顯示。