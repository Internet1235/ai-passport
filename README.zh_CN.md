[English](README.md) | 简体中文

# 元气随身听

面向 FoloToy AI Passport 的离线随身听：九段活泼中文夸夸和三首原创纯音乐、柔和的云朵主题界面，以及本地音乐导入工具。公开版使用通用称呼，不包含商业歌曲录音；可使用导入工具添加自己的本地歌曲和同步歌词。语音采用微软晓伊神经网络声音，并缩短了过长的停顿。

| 按键 | 操作 |
| --- | --- |
| 上 / 下 | 上一首 / 下一首 |
| 确定 | 播放 / 暂停 |
| 长按上 / 下 | 增加 / 降低音量 |
| 长按确定 | 打开设置 / 返回播放页 |

设置包含五档音量（0–100%）、列表循环 / 单曲循环 / 顺序播放，以及 15/30/60 分钟播放定时。所选曲目、音量和播放模式可保存。开机处于暂停状态，从所选曲目开头开始。定时结束会暂停音频，不会关闭设备电源。

本地 MP3/M4A/WAV 导入、中文字体重新生成和测试步骤见[随身听指南](docs/walkman.zh_CN.md)。导入歌曲时使用 `--append` 可保留夸夸内容。中文界面采用 SIL OFL 许可的霞鹜文楷屏幕阅读版，来源见[素材索引](assets/README.zh_CN.md)。

```bash
# 先激活 ESP-IDF 5.5.3 环境。
./tools/validate.sh
# 输出：build/FoloToy-AI-Passport-full.bin
```

硬件为 ESP32-C3、8 MB Flash、无 PSRAM、240×320 屏幕、三个实体按键和 ES8311 音频。MP3 流式播放使内存使用不随歌曲长度增加。固件全程离线，无需账号或麦克风。

开发分支为 `feature/energy-walkman`。此前的《星桥接线》保留在 `feature/starbridge`，操作说明见[游戏指南](docs/starbridge.zh_CN.md)。

另见[开发约定](AGENTS.zh_CN.md)、[硬件指南](docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md)与[文档索引](docs/README.zh_CN.md)。编译、主机测试、实际音质和续航分别记录验证结果。
