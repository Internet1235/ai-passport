<p align="right"><strong>简体中文</strong> · <a href="walkman.md">English</a></p>

# 元气随身听

面向 AI Passport 的离线随身听，内置九段活泼中文夸夸和三首原创纯音乐。语音采用微软晓伊神经网络声音，并缩短了长停顿。公开版文案和语音使用通用称呼，个人录音保留在版本库之外。固件使用独立的 `walkman` NVS 命名空间，分段刷机时保留星桥接线的存档。

## 操作

| 按键 | 播放页 | 设置页 |
| --- | --- | --- |
| 上 / 下 | 上一首 / 下一首 | 选择设置项 |
| 确定 | 播放 / 暂停 | 修改所选设置 |
| 长按上 / 下 | 增加 / 降低音量 | 增加 / 降低音量 |
| 长按确定 | 打开设置 | 返回播放页 |

音量共五档：静音至 100%。支持列表循环、单曲循环、顺序播放。定时可选 15/30/60 分钟，按实际播放时间倒计时，暂停时也暂停倒计时；到时暂停音频，不会关闭设备电源。停止按键操作 1.5 秒后保存所选曲目、音量和循环模式。重启后处于暂停状态，从所选曲目开头开始；不恢复定时和播放进度。

## 导入本地音乐

创建本地 JSON 数组，每项包含 `file`、`title` 和可选 `caption`。文件路径相对于 JSON 文件。支持 MP3、M4A、WAV 及 FFmpeg 可以解码的其他格式。使用 `--append` 可保留已有夸夸并追加歌曲；不加该选项会替换播放列表。生成的固件元数据不包含音频源文件路径；不要把私人路径放入版本库。

```json
[
  {"file": "music.wav", "title": "喜欢的歌", "caption": "给心情一点阳光"}
]
```

```bash
python3 tools/generate_walkman_assets.py --append --playlist /absolute/path/playlist.json
python3 tools/generate_walkman_fonts.py --font /absolute/path/LXGWWenKaiScreen.ttf
./tools/validate.sh
```

使用[素材说明](../assets/README.zh_CN.md)中的固定版本霞鹜文楷屏幕阅读版 v1.522。标题或字幕改变后需重新生成字体。导入会统一响度、添加短淡入淡出，并编码为 96 kbps、22,050 Hz 单声道 MP3。最多 24 首，每首短于八分钟，编码后音频包总计不超过 6 MiB；固件布局校验是最终容量检查。不支持或超大的文件会报错，不会静默截断播放列表。播放从 Flash 每次解码 256 个采样，不把整首歌载入内存。

运行 `python3 tools/generate_walkman_assets.py` 可通过 FFmpeg 重新打包源播放列表。重新生成语音时安装 `edge-tts` 并运行 `tools/generate_walkman_voices.py`，会将保存的夸夸文案发给微软合成；设备播放本身离线。原创纯音乐使用 `tools/compose_walkman_music.py` 重建。来源见[素材索引](../assets/README.zh_CN.md)。本应用在独立分支 `feature/energy-walkman` 开发。

## 验证

`tests/test_walkman.c` 覆盖解码饱和、切歌、循环及结束行为、暂停、静音、定时结束、分块一致性、存档损坏和反复操作。`tools/check_walkman_assets.py` 检查音频包边界、解码每首完整曲目，并在 FFmpeg 可用时比对 MP3 解码结果。完整验证门禁会构建并校验合并固件。

USB 诊断命令：`?` 查看状态，`u/d/o` 模拟短按，`U/D/O` 模拟长按，`c` 采集屏幕帧，`s` 保存。实体按键与诊断命令进入同一个输入队列。音频故障会在播放页显示，不会误报播放成功。主机测试和编译通过不能代表实际音质或续航已经验证。

整数 MP3 解码器来自 [Helix](https://github.com/pschatzmann/codec-helix)，原始声明和 RPSL/RCSL 许可文件保留在 `main/vendor/helix`。`provenance.json` 记录各文件的上游 blob 版本；本地内存适配器使用内部堆内存。播放只使用一个解码器及 6 KiB 音频任务栈；屏幕采集采用 20 行分条缓冲，避免整帧内存与音频争用。

语音字幕使用合成服务返回的词语时间戳。缩短静音时同步调整字幕时间，并补偿 MP3 编码延迟。当前句显示在前后句之间；暂停或切换曲目时字幕也暂停或重置。没有字幕文件的音乐显示曲名和进度。

字幕整理会把短句尾并回原句，并保留原句开始时间。耳机小云朵在语音和音乐页面都采用固定尺寸；字幕行高度有固定边界，不会改变图标大小。

所有字幕行固定为 14 px，仅用颜色区分当前句；长句不会缩小字号或改变插图尺寸。歌曲字幕从本地音频转写并核对时间，保存为毫秒时间轴。字幕 JSON 数组中的每项包含 `ms` 和 `text`，通过播放列表的 `cues` 字段引用；每行不超过 14 个汉字。

使用 `python3 tools/import_walkman_lrc.py song.lrc song.cues.json` 转换本地 LRC 歌词，再在本地播放列表条目中添加 `"cues": "song.cues.json"`。支持 LRC 时间偏移和同句的多个时间标记。超过 14 个字的行会明确报错，请拆分为分别计时的短句。导入后重新生成字库并运行完整验证，再刷机。社区固件仅包含内置的 12 首内容。
