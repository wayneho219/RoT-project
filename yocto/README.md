---
tags: [yocto, index]
topic: yocto
week: "7-8"
---
# Yocto 學習筆記

## 目標

用 Yocto Project 為 STM32MP215F-DK 建立 Linux image。

## Yocto 能做什麼

```
Yocto = 一個 Linux 發行版的構建框架
  ├── 下載 source code（kernel, libraries, apps）
  ├── 交叉編譯（cross-compile）for ARM
  ├── 打包成 rootfs + kernel + DTB
  └── 產生可燒錄的 image（ext4, wic）
```

## 閱讀順序

| 模組 | 主題 |
|------|------|
| [01-concepts](01-concepts.md) | Layer、Recipe、BitBake 基本概念 |
| [02-stm32mp2-setup](02-stm32mp2-setup.md) | STM32MP2 的 Yocto 環境建立 |
| [03-custom-image](03-custom-image.md) | 自訂 image、加入應用程式 |
