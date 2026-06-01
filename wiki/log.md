# Wiki Log

Append-only chronological record of operations on the wiki. Each entry begins with `## [YYYY-MM-DD] <op> | <description>` so it's parseable with `grep "^## \[" log.md | tail -N`.

Operations:
- `ingest` — a source was processed into the wiki.
- `query` — a question was answered against the wiki (typically only logged when the answer was filed back as synthesis).
- `lint` — a health check was run.
- `schema` — the schema was modified.
- `shard` — an index was sharded.

---

## [2026-05-25] schema | 初始化 wiki，設定繁體中文語言偏好
## [2026-05-25] ingest | karpathy-llm-wiki-architecture — Karpathy LLM Wiki 模式架構說明（gist）
## [2026-05-25] ingest | c-language-學習筆記 — 嵌入式 C 學習筆記 8 個 module，產生 8 個 concept 頁
## [2026-05-27] schema | 補充 01-basics.md（enum、C字串、byte序列化）、02-pointers.md（array decay）
