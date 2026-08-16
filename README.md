# Smart AI

Smart AI is a cross-platform desktop assistant built with Qt 6 and C++17. It combines a large language model chat experience with visual understanding, private knowledge retrieval, and tool-calling automation through a single OpenAI-compatible API layer.

## Features

- **Chat** — streaming multi-turn conversations with context memory, Markdown rendering, and configurable models.
- **Vision** — drag-and-drop image input, visual question answering, OCR-style description prompts, and scene understanding.
- **Knowledge base** — import TXT, Markdown, DOCX, and PDF documents; split them into overlapping chunks; embed with a remote embedding model or a local hashing embedder; retrieve top-k chunks and answer with citations.
- **Agent tools** — function calling for calculation, live weather, web search, local knowledge search, and system time, with a transparent reasoning trace.
- **Model switching** — built-in presets for OpenAI, DeepSeek, Qwen, and local Ollama-compatible servers.
- **Local-first storage** — SQLite conversation and knowledge indexes, plus portable JSON settings.

## Build

Requirements:

- Qt 6.5+ with Widgets, Network, Sql, and Concurrent modules
- CMake 3.21+
- A C++17 compiler
- zlib development headers

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On Windows with Qt Creator, open `CMakeLists.txt` and select a Qt 6 kit.

## Configure a model

Open **Settings** in the application and select a provider preset, or provide any OpenAI-compatible endpoint:

| Provider | Base URL | Example model |
| --- | --- | --- |
| DeepSeek | `https://api.deepseek.com` | `deepseek-chat` |
| Qwen / DashScope | `https://dashscope.aliyuncs.com/compatible-mode/v1` | `qwen-plus` |
| OpenAI | `https://api.openai.com/v1` | `gpt-4o-mini` |
| Ollama | `http://localhost:11434/v1` | `qwen2.5:7b` |

The application reads `SMART_AI_API_KEY`, `SMART_AI_BASE_URL`, and `SMART_AI_MODEL` when available, but values saved in Settings take precedence.

## Project structure

```text
src/
  app/    Qt Widgets pages and dialogs
  core/   API client, SQLite stores, RAG pipeline, agent tools
resources/
  app.qss, icons
```

## Privacy

API keys are stored locally on your machine using Qt settings. The app does not upload documents or conversation data except to the model endpoint you explicitly configure.

