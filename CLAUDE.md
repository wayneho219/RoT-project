# 專案設定

## LLM Wiki

此目錄維護一個 LLM 策展的知識庫（`wiki/`），遵循 Andrej Karpathy 的「LLM Wiki」模式（https://gist.github.com/karpathy/442a6bf555914893e9891c11519de94f）。

回答與知識庫相關的問題前，先讀 `wiki/index.md`，用一行摘要找出相關頁面再深入讀取。引用格式用 `[[wikilink]]`。若 index 找不到好的候選頁，使用 `python3 scripts/wiki_search.py wiki/ "關鍵字"` 做 BM25 搜尋。

新增資料時，遵循 `llm-wiki` skill 的 ingest 流程：寫 `wiki/sources/` 摘要頁 → 更新相關 entity/concept 頁（用 str_replace，不要整頁重寫）→ 更新 `wiki/index.md` → 在 `wiki/log.md` 加一行記錄。

所有 wiki 頁面使用繁體中文。完整規範見 `wiki/SCHEMA.md`，與本文衝突時以 SCHEMA.md 為準。
