#include "AppSettings.h"

#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QProcessEnvironment>

namespace {
QString cleanBaseUrl(QString value)
{
    value = value.trimmed();
    while (value.endsWith('/'))
        value.chop(1);
    return value;
}
}

AppSettings *AppSettings::instance()
{
    static AppSettings settings;
    return &settings;
}

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_settings = new QSettings(path + QStringLiteral("/smart-ai.ini"), QSettings::IniFormat, this);
    load();
}

QString AppSettings::providerPreset() const
{
    return m_settings->value(QStringLiteral("provider/preset"), QStringLiteral("deepseek")).toString();
}

QString AppSettings::baseUrl() const
{
    const QString env = envOrDefault(QStringLiteral("SMART_AI_BASE_URL"), QString());
    if (!env.isEmpty())
        return cleanBaseUrl(env);
    return cleanBaseUrl(m_settings->value(QStringLiteral("provider/base_url"), QStringLiteral("https://api.deepseek.com")).toString());
}

QString AppSettings::apiKey() const
{
    const QString env = envOrDefault(QStringLiteral("SMART_AI_API_KEY"), QString());
    if (!env.isEmpty())
        return env;
    return m_settings->value(QStringLiteral("provider/api_key")).toString();
}

QString AppSettings::chatModel() const
{
    const QString env = envOrDefault(QStringLiteral("SMART_AI_MODEL"), QString());
    if (!env.isEmpty())
        return env;
    return m_settings->value(QStringLiteral("provider/chat_model"), QStringLiteral("deepseek-chat")).toString();
}

QString AppSettings::visionModel() const
{
    return m_settings->value(QStringLiteral("provider/vision_model"), QStringLiteral("qwen-vl-plus")).toString();
}

QString AppSettings::embeddingModel() const
{
    return m_settings->value(QStringLiteral("embedding/model"), QStringLiteral("text-embedding-v3")).toString();
}

QString AppSettings::embeddingBaseUrl() const
{
    QString value = m_settings->value(QStringLiteral("embedding/base_url")).toString();
    if (value.trimmed().isEmpty())
        value = baseUrl();
    return cleanBaseUrl(value);
}

QString AppSettings::embeddingApiKey() const
{
    const QString value = m_settings->value(QStringLiteral("embedding/api_key")).toString();
    if (!value.trimmed().isEmpty())
        return value;
    return apiKey();
}

double AppSettings::temperature() const
{
    return m_settings->value(QStringLiteral("provider/temperature"), 0.7).toDouble();
}

int AppSettings::maxTokens() const
{
    return m_settings->value(QStringLiteral("provider/max_tokens"), 2048).toInt();
}

int AppSettings::timeoutMs() const
{
    return m_settings->value(QStringLiteral("network/timeout_ms"), 60000).toInt();
}

bool AppSettings::useLocalEmbedding() const
{
    return m_settings->value(QStringLiteral("embedding/use_local"), false).toBool();
}

int AppSettings::topK() const
{
    return m_settings->value(QStringLiteral("rag/top_k"), 4).toInt();
}

int AppSettings::chunkSize() const
{
    return m_settings->value(QStringLiteral("rag/chunk_size"), 512).toInt();
}

int AppSettings::chunkOverlap() const
{
    return m_settings->value(QStringLiteral("rag/chunk_overlap"), 50).toInt();
}

QString AppSettings::localOllamaUrl() const
{
    return cleanBaseUrl(m_settings->value(QStringLiteral("provider/ollama_url"), QStringLiteral("http://localhost:11434/v1")).toString());
}

void AppSettings::setProviderPreset(const QString &value)
{
    m_settings->setValue(QStringLiteral("provider/preset"), value);
}

void AppSettings::setBaseUrl(const QString &value)
{
    m_settings->setValue(QStringLiteral("provider/base_url"), cleanBaseUrl(value));
}

void AppSettings::setApiKey(const QString &value)
{
    m_settings->setValue(QStringLiteral("provider/api_key"), value);
}

void AppSettings::setChatModel(const QString &value)
{
    m_settings->setValue(QStringLiteral("provider/chat_model"), value);
}

void AppSettings::setVisionModel(const QString &value)
{
    m_settings->setValue(QStringLiteral("provider/vision_model"), value);
}

void AppSettings::setEmbeddingModel(const QString &value)
{
    m_settings->setValue(QStringLiteral("embedding/model"), value);
}

void AppSettings::setEmbeddingBaseUrl(const QString &value)
{
    m_settings->setValue(QStringLiteral("embedding/base_url"), cleanBaseUrl(value));
}

void AppSettings::setEmbeddingApiKey(const QString &value)
{
    m_settings->setValue(QStringLiteral("embedding/api_key"), value);
}

void AppSettings::setTemperature(double value)
{
    m_settings->setValue(QStringLiteral("provider/temperature"), value);
}

void AppSettings::setMaxTokens(int value)
{
    m_settings->setValue(QStringLiteral("provider/max_tokens"), value);
}

void AppSettings::setTimeoutMs(int value)
{
    m_settings->setValue(QStringLiteral("network/timeout_ms"), value);
}

void AppSettings::setUseLocalEmbedding(bool value)
{
    m_settings->setValue(QStringLiteral("embedding/use_local"), value);
}

void AppSettings::setTopK(int value)
{
    m_settings->setValue(QStringLiteral("rag/top_k"), value);
}

void AppSettings::setChunkSize(int value)
{
    m_settings->setValue(QStringLiteral("rag/chunk_size"), value);
}

void AppSettings::setChunkOverlap(int value)
{
    m_settings->setValue(QStringLiteral("rag/chunk_overlap"), value);
}

void AppSettings::setLocalOllamaUrl(const QString &value)
{
    m_settings->setValue(QStringLiteral("provider/ollama_url"), cleanBaseUrl(value));
}

void AppSettings::save()
{
    m_settings->sync();
}

void AppSettings::load()
{
    // Loading is implicit in QSettings; call sync only after explicit changes.
}

QString AppSettings::normalizedBaseUrl(const QString &baseUrl)
{
    return cleanBaseUrl(baseUrl);
}

QString AppSettings::defaultApiKeyFromEnvironment()
{
    return QProcessEnvironment::systemEnvironment().value(QStringLiteral("SMART_AI_API_KEY"));
}

QString AppSettings::defaultBaseUrlFromEnvironment()
{
    return QProcessEnvironment::systemEnvironment().value(QStringLiteral("SMART_AI_BASE_URL"));
}

QString AppSettings::defaultModelFromEnvironment()
{
    return QProcessEnvironment::systemEnvironment().value(QStringLiteral("SMART_AI_MODEL"));
}

QString AppSettings::envOrDefault(const QString &envName, const QString &fallback) const
{
    const QString value = QProcessEnvironment::systemEnvironment().value(envName);
    return value.isEmpty() ? fallback : value;
}

QList<AppSettings::ProviderPreset> AppSettings::presets()
{
    return {
        { QStringLiteral("DeepSeek"), QStringLiteral("https://api.deepseek.com"), QStringLiteral("deepseek-chat"), QStringLiteral("deepseek-chat"), QStringLiteral("text-embedding-v3"), false },
        { QStringLiteral("Qwen / DashScope"), QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode/v1"), QStringLiteral("qwen-plus"), QStringLiteral("qwen-vl-plus"), QStringLiteral("text-embedding-v3"), false },
        { QStringLiteral("OpenAI"), QStringLiteral("https://api.openai.com/v1"), QStringLiteral("gpt-4o-mini"), QStringLiteral("gpt-4o-mini"), QStringLiteral("text-embedding-3-small"), false },
        { QStringLiteral("Ollama (local)"), QStringLiteral("http://localhost:11434/v1"), QStringLiteral("qwen2.5:7b"), QStringLiteral("llava:7b"), QStringLiteral("nomic-embed-text"), true },
        { QStringLiteral("Custom"), QString(), QStringLiteral("your-model"), QStringLiteral("your-vision-model"), QStringLiteral("text-embedding-3-small"), false },
    };
}

