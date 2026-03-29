# ESP32 示例项目集合

一个 ESP32 开发示例项目集合，基于 ESP-IDF 框架开发，包含多个实用的 ESP32 应用示例。

## 前置条件

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html) v5.0 或更高版本
- Python 3.8+
- CMake 3.16+
- ESP32 开发板

## 快速开始

### 1. 克隆项目

```bash
git clone https://github.com/shenjingnan/esp32-examples.git
cd esp32-examples
```

### 2. 设置 ESP-IDF 环境

```bash
# 如果已安装 ESP-IDF，激活环境
. $HOME/esp/esp-idf/export.sh

# 或使用别名（如果已配置）
get_idf
```

### 3. 进入示例目录并构建

```bash
cd examples/01-wifi-hello-world
idf.py build
```

### 4. 烧录到设备

```bash
# 连接 ESP32 开发板，然后执行
idf.py -p PORT flash

# 例如：
# idf.py -p /dev/ttyUSB0 flash           # Linux
# idf.py -p COM3 flash                    # Windows
# idf.py -p /dev/tty.usbserial-110 flash # macOS
```

### 5. 查看日志输出

```bash
idf.py -p PORT monitor

# 也可以一步完成构建、烧录和监控
idf.py -p PORT flash monitor
```

## 示例列表

| 编号 | 示例名称 | 描述 |
|------|----------|------|
| 01 | [WiFi Hello World](examples/01-wifi-hello-world/) | WiFi Captive Portal 示例，创建热点并自动弹出 Web 页面 |
| 02 | [WiFi Config Portal](examples/02-wifi-config-portal/) | WiFi 强制门户配网，通过表单配置 WiFi 连接 |

> 更多示例持续更新中...

## 开发环境配置

### VSCode IntelliSense 配置

如果你使用 VSCode 打开项目后发现头文件报错（如 `esp_wifi.h`、`esp_event.h` 等找不到），这是正常的，因为 IntelliSense 需要读取构建生成的配置文件。

#### 解决方法

**第一步：构建项目**

无论使用什么操作系统，首先需要在示例目录下执行构建：

```bash
cd examples/01-wifi-hello-world
idf.py build
```

构建成功后会在 `build/` 目录下生成 `compile_commands.json` 文件，这个文件包含了所有头文件的路径信息。

**第二步：配置 VSCode**

有两种方式配置 IntelliSense：

##### 方式一：使用 ESP-IDF 扩展（推荐）

安装 VSCode 的 [ESP-IDF 扩展](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension)，扩展会自动配置 IntelliSense。

##### 方式二：手动配置

在项目根目录或示例目录下创建 `.vscode/c_cpp_properties.json`：

```json
{
    "configurations": [{
        "name": "ESP-IDF",
        "compileCommands": "${workspaceFolder}/build/compile_commands.json",
        "includePath": ["${workspaceFolder}/**"],
        "cStandard": "c17",
        "cppStandard": "c++17"
    }],
    "version": 4
}
```

**注意**：`intelliSenseMode` 需要根据你的系统选择：
- macOS (Apple Silicon): `"macos-clang-arm64"`
- macOS (Intel): `"macos-clang-x64"`
- Linux: `"linux-gcc-x64"`
- Windows: `"windows-msvc-x64"` 或 `"windows-gcc-x64"`

配置完成后，按 `Cmd+Shift+P`（macOS）或 `Ctrl+Shift+P`（Windows/Linux），输入 `C/C++: Reload IntelliSense Database` 刷新数据库。

### 推荐的开发方式

由于本项目包含多个独立示例，推荐以下两种开发方式：

##### 方式一：打开根目录（推荐）

```bash
# 打开项目根目录
code /path/to/esp32-examples
```

然后在 VSCode 中切换到不同示例目录进行开发。

##### 方式二：打开单个示例目录

```bash
# 打开单个示例
code /path/to/esp32-examples/examples/01-wifi-hello-world
```

这种方式需要在示例目录下单独配置 `.vscode/c_cpp_properties.json`。

## 项目结构

```
esp32-examples/
├── README.md                   # 项目说明文档
├── LICENSE                     # GNU GPL v3 许可证
├── .gitignore                  # Git 忽略配置
└── examples/                   # 示例项目目录
    ├── 01-wifi-hello-world/    # WiFi Captive Portal 示例
    └── 02-wifi-config-portal/  # WiFi 强制门户配网示例
```

## 贡献指南

欢迎贡献新的示例项目！请遵循以下规范：

1. **命名规范**: 示例目录使用 `NN-feature-name` 格式（NN 为两位数字编号）
2. **文档要求**: 每个示例必须包含 `README.md`，说明功能、使用方法和注意事项
3. **代码风格**: 遵循 ESP-IDF 官方代码风格
4. **提交规范**: 使用语义化提交信息（feat/fix/docs/style/refactor/test/chore）

## 许可证

本项目采用 [GNU GPL v3 许可证](LICENSE)。

## 参考资料

- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/)
- [ESP32 技术参考手册](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)