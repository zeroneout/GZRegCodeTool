# 工资序号转换工具

一个用 **纯 C + Win32 API** 编写的极简序号转换小工具，单文件 `regcode.exe` 仅约 **13.5 KB**，零依赖，支持 **Windows 7 ~ Windows 11**。

> 输入 8 位数字序号，点击「转换」即可得到对应的 10 位转换码。

---

## 功能特性

- **极简体积**：纯 Win32 实现，静态编译，单文件 13.5 KB，无需任何运行库。
- **输入即转换**：输入框限制 8 位数字（`ES_NUMBER` + 限长），输满后按 **回车** 直接转换。
- **输出框自绘**：结果框由程序自绘（白底 + 凹陷边框 + 居中文字），彻底避免系统编辑框在连续转换时的文字叠加/压字问题。
- **一键复制**：
  - **双击** 输出框即可复制结果；
  - 输入框**未选中文本**时按 `Ctrl+C` 复制结果；选中输入框数字时按 `Ctrl+C` 则复制输入框内容。
- **清晰字体**：使用微软雅黑（ClearType），文字锐利不糊；高分辨率屏下按 DPI 自适应，跨屏拖动也不变形。
- **关于页**：点击「关于」在主窗口内显示说明，**无系统提示音**；点击窗口任意处或按 `Esc` 返回。
- **无冗余提示音**：输入框满 8 位后再敲数字会静默忽略，不再「嘟」一声。

---

## 使用方法

1. 下载 `regcode.exe`（见右侧 **Releases** 或仓库根目录）。
2. 双击运行，在「序号：」框输入 8 位数字，点击「转换」（或直接回车）。
3. 结果出现在右侧结果框，双击结果框即可复制到剪贴板。

---

## 构建方法

本工具使用 [winlibs](https://winlibs.com/) 的 MinGW-w64 + Clang 编译。

### 环境准备

1. 下载并解压 winlibs 的 **MinGW-w64 + Clang** 整合包（LLVM/Clang/LLD/MinGW-w64）。
2. 将 `bin` 目录（如 `C:\mingw64\bin`）加入系统 `PATH`。
3. 验证：`clang --version` 能正常输出版本即可。

### 一键编译

在项目目录执行：

```bat
build.bat
```

`build.bat` 内部执行的编译命令如下（已做体积优化）：

```bat
windres resource.rc -O coff -o resource.o
clang -Os -s -static -nostartfiles -mwindows ^
      -ffunction-sections -fdata-sections ^
      -Wl,--gc-sections -Wl,-e,app_entry ^
      regcode.c resource.o -o regcode.exe ^
      -lgdi32 -luser32 -lkernel32
```

也可重新生成应用图标：

```bat
python3 gen_icon.py
```

---

## 文件说明

| 文件 | 说明 |
|------|------|
| `regcode.c`   | 主程序源码 |
| `build.bat`   | 一键编译脚本 |
| `gen_icon.py` | 应用图标生成脚本（粉嫩可爱风格） |
| `resource.rc` | 资源描述（图标） |
| `app.manifest`| DPI 感知清单 |
| `appicon.ico` | 应用图标 |
| `regcode.exe` | 编译产物（Release 附件中提供） |

---

## 许可证

本工具仅供学习与交流使用。
