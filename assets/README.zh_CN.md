<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

星桥接线使用 `fonts/starbridge_12.c` 和 `fonts/starbridge_16.c`，位图子集命名为
Starbridge，来源为[霞鹜文楷屏幕阅读版 v1.522](https://github.com/lxgw/LxgwWenKai-Screen/releases/tag/v1.522)。
采用 SIL OFL 1.1，版权与许可见 `fonts/OFL-WenKai.txt`。以屏幕阅读字重替换思源黑体，
保留 12/16 像素字号，并用 4 位灰度平滑笔画。生成的 C 数组存放在闪存中，不需要
为整份字体分配 RAM。仅嵌入界面字符和可打印 ASCII。

从上述发布页下载 `LXGWWenKaiScreen.ttf`，修改界面文字后重新生成：

```bash
python3 tools/generate_starbridge_fonts.py --font /path/to/LXGWWenKaiScreen.ttf
python3 tools/generate_starbridge_fonts.py --check
```

转换器固定为 `lv_font_conv@1.5.3`，脚本校验源文件 SHA-256：
`cd1a6fa39c4ea42fd8f4e289945789b0e510cf7016435640f8893cdad9b220f3`。
构建已提交的 C 子集不需要下载约 25 MB 的原始字库。

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。

Starbridge 的原创旋律与音效由 `main/starbridge_sound.c` 实时合成，采用本仓库 MIT 许可；
`tools/render_starbridge_audio.py` 可生成试听 WAV，输出到忽略的 `delivery/` 目录。
