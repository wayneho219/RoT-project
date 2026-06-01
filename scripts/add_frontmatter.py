#!/usr/bin/env python3
"""
批次為學習筆記加上 YAML frontmatter，讓 Obsidian 可以用顏色和標籤分群。
只處理尚未有 frontmatter 的檔案（第一行不是 --- 的）。

用法：python3 scripts/add_frontmatter.py [--dry-run]
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 每個目錄對應的 tags 和 topic
TOPIC_CONFIG = {
    "c-language": {
        "topic": "c-language",
        "tags": ["c-language"],
        "week": "1-2",
    },
    "arm-architecture": {
        "topic": "arm-architecture",
        "tags": ["arm-architecture"],
        "week": "3-4",
    },
    "boot-flow": {
        "topic": "boot-flow",
        "tags": ["boot-flow"],
        "week": "5-6",
    },
    "trustzone": {
        "topic": "trustzone",
        "tags": ["trustzone"],
        "week": "5-6",
    },
    "yocto": {
        "topic": "yocto",
        "tags": ["yocto"],
        "week": "7-8",
    },
    "cryptography": {
        "topic": "cryptography",
        "tags": ["cryptography"],
        "week": "9",
    },
    "stm32mp2": {
        "topic": "stm32mp2",
        "tags": ["stm32mp2"],
        "week": "9+",
    },
    "networking": {
        "topic": "networking",
        "tags": ["networking"],
        "week": "supplement",
    },
}

# 模組名稱從檔名猜測
def guess_module_tags(filename):
    name = os.path.splitext(filename)[0]
    if name == "README":
        return ["index"]
    return ["module"]


def has_frontmatter(content):
    return content.startswith("---\n")


def build_frontmatter(dirkey, filename):
    config = TOPIC_CONFIG[dirkey]
    topic = config["topic"]
    tags = config["tags"] + guess_module_tags(filename)
    week = config["week"]

    lines = ["---"]
    lines.append(f"tags: [{', '.join(tags)}]")
    lines.append(f"topic: {topic}")
    lines.append(f"week: \"{week}\"")
    lines.append("---")
    lines.append("")
    return "\n".join(lines)


def process_file(filepath, dirkey, dry_run):
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()

    if has_frontmatter(content):
        return False  # 已有 frontmatter，跳過

    filename = os.path.basename(filepath)
    fm = build_frontmatter(dirkey, filename)
    new_content = fm + content

    if dry_run:
        print(f"[DRY RUN] Would add frontmatter to: {filepath}")
    else:
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(new_content)
        print(f"Added frontmatter: {filepath}")

    return True


def main():
    dry_run = "--dry-run" in sys.argv
    count = 0

    for dirkey in TOPIC_CONFIG:
        dirpath = os.path.join(ROOT, dirkey)
        if not os.path.isdir(dirpath):
            continue
        for fname in sorted(os.listdir(dirpath)):
            if not fname.endswith(".md"):
                continue
            fpath = os.path.join(dirpath, fname)
            if process_file(fpath, dirkey, dry_run):
                count += 1

    print(f"\n{'Would process' if dry_run else 'Processed'} {count} files.")
    if dry_run:
        print("Run without --dry-run to apply changes.")


if __name__ == "__main__":
    main()
