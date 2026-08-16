# Smart AI

Smart AI 是一个基于 Qt 6 和 C++17 开发的跨平台桌面智能助手。它通过统一的 OpenAI 兼容接口，集成了大语言模型对话、图像理解、私有知识库检索和智能体工具调用能力。

## 主要功能

- **智能对话**：支持多轮上下文记忆、流式输出、Markdown 渲染和模型切换。
- **图像理解**：支持拖拽或上传图片，进行视觉问答、OCR 文字提取、物体识别和场景理解。
- **知识库问答**：可导入 TXT、Markdown、DOCX、PDF 文档，自动分块、向量化并检索 Top-K 相关内容，结合私有知识生成带引用回答。
- **智能体工具**：通过 Function Calling 自动调用计算器、天气查询、网络搜索、知识库检索和当前时间等工具，并展示推理过程。
- **模型服务**：内置 DeepSeek、通义千问、OpenAI 和本地 Ollama 兼容服务配置，也可自定义任意 OpenAI 兼容接口。
- **本地存储**：对话记录、知识库索引和向量数据使用 SQLite 保存，API 密钥仅保存在本机。

## 编译运行

环境要求：

- Qt 6.5 及以上，包含 Widgets、Network、Sql、Concurrent 模块
- CMake 3.21 及以上
- 支持 C++17 的编译器
- zlib 开发文件

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

在 Windows 上使用 Qt Creator 时，直接打开 `CMakeLists.txt` 并选择 Qt 6 编译套件即可。

## 配置模型

打开应用中的“设置”，选择预设服务，或填写任意 OpenAI 兼容接口：

| 服务商 | 接口地址 | 示例模型 |
| --- | --- | --- |
| DeepSeek | `https://api.deepseek.com` | `deepseek-chat` |
| 通义千问 / DashScope | `https://dashscope.aliyuncs.com/compatible-mode/v1` | `qwen-plus` |
| OpenAI | `https://api.openai.com/v1` | `gpt-4o-mini` |
| Ollama | `http://localhost:11434/v1` | `qwen2.5:7b` |

应用会读取 `SMART_AI_API_KEY`、`SMART_AI_BASE_URL` 和 `SMART_AI_MODEL` 环境变量；如果设置界面中已保存配置，则以界面配置为准。

## 项目结构

```text
src/
  app/    Qt Widgets 页面与对话框
  core/   API 客户端、SQLite 存储、RAG 流程和智能体工具
resources/
  app.qss、图标
```

## 隐私说明

API 密钥仅通过 Qt 设置保存在本机。除你主动配置的模型服务地址外，应用不会向其他服务上传文档或对话数据。
