# LLM Knowledge Bases - Architecture (Karpathy)

來源：https://gist.github.com/karpathy/442a6bf555914893e9891c11519de94f
收集日期：2026-05-25

---

## Overview
A personal knowledge base system where an LLM acts as a compiler that reads raw source documents and produces a structured, interlinked markdown wiki. No vector databases or embeddings needed at personal scale.

## Phase 1: Ingest
- Obsidian Web Clipper: Browser extension converts web articles into clean .md files with locally downloaded images
- Papers & Repos: arXiv papers, GitHub repos, datasets collected into raw/ staging directory
- raw/ directory: All source documents land here first - the LLM reads from this staging area

## Phase 2: Compile (LLM Compiler)
The LLM incrementally reads raw/ and builds a structured wiki:
- Index & Summaries: Auto-maintained index files with brief summaries of all documents - entry point for queries
- Concept Articles: ~100 articles, ~400K words, organized by topic with backlinks and cross-references
- Derived Outputs: Marp slide decks, matplotlib charts, filed-back query answers
- Backlinks & Cross-links: Auto-generated link graph between concepts, finding connections for new article candidates

## Phase 3: Query & Enhance
- Obsidian IDE: Frontend for browsing the wiki and visualizations
- Q&A Agent: Complex research questions across articles - answers rendered as markdown, slides, or charts
- Search Engine: Vibe-coded naive search over the wiki, usable via web UI or as a CLI tool for the LLM
- Key insight: Outputs from queries get filed back into the wiki - every exploration adds up

## Phase 4: Lint & Maintain
- Scan for inconsistent data
- Impute missing information via web search
- Find connections between concepts for new articles
- Suggest further questions to explore
- After linting, cycle returns to Phase 2 - wiki keeps growing

## Feedback Loops
- Q&A Agent outputs -> filed back into wiki (Derived Outputs)
- Linting results -> enhance the wiki
- Phase 4 cycles back to Phase 2 continuously

## Future Direction
Synthetic data generation from the wiki to fine-tune an LLM so it "knows" the data in its weights rather than just through context windows.

## Tools Used
- Obsidian (IDE + file viewer)
- Obsidian Web Clipper (article ingestion)
- LLM with large context window (compilation)
- Markdown directory structure (wiki storage)
