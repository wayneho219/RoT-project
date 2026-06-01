---
type: source
title: LLM 知識庫架構（Karpathy）
tags: [llm-wiki, 知識管理, 架構]
authors: [Andrej Karpathy]
url: https://gist.github.com/karpathy/442a6bf555914893e9891c11519de94f
raw: raw/karpathy-llm-wiki-architecture.md
ingested: 2026-05-25
created: 2026-05-25
updated: 2026-05-25
---

# LLM 知識庫架構（Karpathy）

## 核心洞見

LLM 作為「編譯器」：不是每次問問題都從原始資料重新推導，而是一次性將知識編譯進結構化 wiki，之後的查詢從 wiki 讀取。知識會隨時間累積，而非每次從零開始。

與 RAG 的根本差異：RAG 每次查詢都重新從原始文本提取；LLM Wiki 預先合成知識，查詢時讀取已整理好的頁面。

## 四個階段

### Phase 1：收集
- **Obsidian Web Clipper**：瀏覽器擴充，把網頁文章轉成 `.md`，圖片本地化下載
- 論文（arXiv）、GitHub repo、資料集放進 `raw/` 暫存區
- `raw/` 是所有原始資料的落點，LLM 只讀不改

### Phase 2：編譯（核心）
LLM 增量讀取 `raw/`，建立 wiki：
- 自動維護索引與摘要（所有文件的入口）
- 概念文章：約 100 篇、40 萬字，依主題組織，含反向連結
- 衍生輸出：Marp 投影片、matplotlib 圖表、查詢答案存回 wiki
- 自動建立反向連結與跨連結，幫助發現新文章候選

### Phase 3：查詢與強化
- **Obsidian**：wiki 的前端瀏覽器與視覺化工具
- **Q&A Agent**：跨文章的複雜研究問題，答案渲染成 markdown / 投影片 / 圖表
- **搜尋引擎**：簡單的全文搜尋，可從 web UI 或 CLI 使用
- **關鍵**：查詢結果存回 wiki，每次探索都留下痕跡

### Phase 4：維護與擴充
- 掃描矛盾資料
- 透過網路搜尋補充缺漏
- 發現概念間的新連結，提出新文章候選
- 建議進一步探索的問題
- 完成後回到 Phase 2，持續循環

## 反饋迴路

```
收集 → 編譯 → 查詢
              ↓
           答案存回 wiki
              ↓
           維護/Lint
              ↓
        回到編譯（Phase 2）
```

## 未來方向

從 wiki 生成合成訓練資料，微調 LLM，讓知識直接進入模型權重，而非只存在 context window。

## 工具清單

| 工具 | 用途 |
|------|------|
| Obsidian | wiki 前端、檔案瀏覽 |
| Obsidian Web Clipper | 文章收集 |
| LLM（大 context window） | 編譯、查詢、維護 |
| Markdown 目錄結構 | wiki 儲存 |

## 相關頁面

- [[llm-wiki-模式]] — 核心概念詳解
- [[andrej-karpathy]] — 作者資訊
