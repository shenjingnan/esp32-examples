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

> 更多示例持续更新中...

## 项目结构

```
esp32-examples/
├── README.md                   # 项目说明文档
├── LICENSE                     # MIT 许可证
├── .gitignore                  # Git 忽略配置
└── examples/                   # 示例项目目录
    └── 01-wifi-hello-world/    # WiFi Captive Portal 示例
```

## 贡献指南

欢迎贡献新的示例项目！请遵循以下规范：

1. **命名规范**: 示例目录使用 `NN-feature-name` 格式（NN 为两位数字编号）
2. **文档要求**: 每个示例必须包含 `README.md`，说明功能、使用方法和注意事项
3. **代码风格**: 遵循 ESP-IDF 官方代码风格
4. **提交规范**: 使用语义化提交信息（feat/fix/docs/style/refactor/test/chore）

## 许可证

本项目采用 [MIT 许可证](LICENSE)。

## 参考资料

- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/)
- [ESP32 技术参考手册](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)