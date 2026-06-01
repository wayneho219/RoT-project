# Wiki Index

The catalog of all pages in this wiki. Each entry: a wikilink to the page and a one-line summary. The LLM reads this first when answering queries to identify candidate pages.

Keep summaries tight — one line each. The index is engineered to be cheap to read; a fat index defeats its purpose.

When this file exceeds ~300 lines or the wiki passes ~150 pages, shard into `wiki/indexes/<type>.md` and replace this file with a directory of shards. See the `scaling-playbook.md` reference in the `llm-wiki` skill for the migration procedure.

---

## Sources

- [[karpathy-llm-wiki-architecture]] — Karpathy 的 LLM Wiki 模式架構說明（四階段：收集→編譯→查詢→維護）
- [[c-language-學習筆記]] — 嵌入式導向 C 語言學習筆記，8 個 module，從 C# 視角切入

## Entities

- [[andrej-karpathy]] — AI 研究員，LLM Wiki 模式提出者，前 OpenAI / Tesla AI

## Concepts

- [[llm-wiki-模式]] — LLM 作為編譯器的知識庫架構，與 RAG 的差異、三層結構、飛輪機制
- [[c-基礎與型別系統]] — C vs C# 對照、固定寬度型別、struct、static 語意
- [[c-指標與記憶體存取]] — 指標基礎、MMIO、const 指標組合、常見錯誤
- [[c-位元運算]] — set/clear/toggle/read bit、mask 模式、STM32 GPIO 實例
- [[c-函式指標與回調模式]] — callback、jump table、模擬 interface、ARM 向量表
- [[c-嵌入式關鍵語法]] — volatile、MMIO struct 模式、macro、__attribute__
- [[c-記憶體模型與分段]] — stack/heap/BSS/Data/Text、linker script、startup code 流程
- [[c-多檔案組織與模組化]] — .h/.c 分工、opaque pointer、命名慣例、Makefile
- [[c-未定義行為與安全程式碼]] — 整數溢位、strict aliasing、buffer overflow、7 條安全規則

## Synthesis

(populated as query answers are filed back)
