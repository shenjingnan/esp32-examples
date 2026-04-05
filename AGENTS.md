# CLAUDE.md — ESP32 示例项目

## 项目简介

ESP32 示例项目集合（monorepo），基于 ESP-IDF v5.0+ 框架，包含多个独立的 ESP32 应用示例。

## 项目结构

- `examples/` 下每个子目录是一个独立的 ESP-IDF 项目
- 目录命名：`NN-feature-name`（NN 为两位数编号）
- 每个示例包含自己的 `CMakeLists.txt`、`main/`、`components/` 等
- HTML 等资源文件通过 `EMBED_FILES` 嵌入固件

## 构建与开发

```bash
# 激活 ESP-IDF 环境
. $HOME/esp/esp-idf/export.sh

# 进入示例目录构建
cd examples/01-wifi-hello-world
idf.py build

# 烧录
idf.py -p PORT flash

# 构建并烧录 + 监控
idf.py -p PORT flash monitor
```

## 代码风格

- 主程序使用 C++（`.cc`），组件使用 C（`.c`）
- 组件头文件使用 `extern "C"` 包装以支持 C++ 调用
- 遵循 ESP-IDF 官方代码风格
- 项目级 CMakeLists.txt 设置 `MINIMAL_BUILD ON`
- 组件通过 `PRIV_REQUIRES` 声明依赖

## Git 提交规范

格式：`type(scope): subject`

- **scope**：示例名称（如 `wifi-config-portal`）或模块名（如 `commands`）
- **type**：`feat` / `fix` / `docs` / `style` / `refactor` / `perf` / `test` / `chore` / `revert`

示例：
```
feat(wifi-config-portal): 添加 WiFi 扫描列表功能
docs: 更新 README.md 添加新示例
refactor(wifi-config-portal): 将 HTTP Server 逻辑提取到独立文件
```

## 自定义命令

### `/update-readme`

扫描 `examples/` 目录下所有示例，自动更新 `README.md` 中的「示例列表」表格和「项目结构」目录树。

## 关键文件

| 路径 | 说明 |
|------|------|
| `README.md` | 项目说明文档 |
| `LICENSE` | GNU GPL v3 许可证 |
| `.claude/commands/update-readme.md` | `/update-readme` 命令定义 |
