#pragma once

#include <QObject>
#include <QString>

class QSettings;

class AppSettings : public QObject
{
    Q_OBJECT
public:
    static AppSettings *instance();

    QString providerPreset() const;
    QString baseUrl() const;
    QString apiKey() const;
    QString chatModel() const;
    QString visionModel() const;
    QString embeddingModel() const;
    QString embeddingBaseUrl() const;
    QString embeddingApiKey() const;
    double temperature() const;
    int maxTokens() const;
    int timeoutMs() const;
    bool useLocalEmbedding() const;
    int topK() const;
    int chunkSize() const;
    int chunkOverlap() const;
    QString localOllamaUrl() const;

    void setProviderPreset(const QString &value);
    void setBaseUrl(const QString &value);
    void setApiKey(const QString &value);
    void setChatModel(const QString &value);
    void setVisionModel(const QString &value);
    void setEmbeddingModel(const QString &value);
    void setEmbeddingBaseUrl(const QString &value);
    void setEmbeddingApiKey(const QString &value);
    void setTemperature(double value);
    void setMaxTokens(int value);
    void setTimeoutMs(int value);
    void setUseLocalEmbedding(bool value);
    void setTopK(int value);
    void setChunkSize(int value);
    void setChunkOverlap(int value);
    void setLocalOllamaUrl(const QString &value);

    void save();
    void load();

    static QString normalizedBaseUrl(const QString &baseUrl);
    static QString defaultApiKeyFromEnvironment();
    static QString defaultBaseUrlFromEnvironment();
    static QString defaultModelFromEnvironment();

    struct ProviderPreset
    {
        QString name;
        QString baseUrl;
        QString chatModel;
        QString visionModel;
        QString embeddingModel;
        bool local = false;
    };

    static QList<ProviderPreset> presets();

private:
    explicit AppSettings(QObject *parent = nullptr);
    QString envOrDefault(const QString &envName, const QString &fallback) const;
    QSettings *m_settings = nullptr;
};

