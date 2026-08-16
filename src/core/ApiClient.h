#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class AppSettings;

class ApiClient : public QObject
{
    Q_OBJECT
public:
    explicit ApiClient(QObject *parent = nullptr);

    void configureFromSettings();
    void setChatEndpoint(const QString &baseUrl, const QString &apiKey);
    void setEmbeddingEndpoint(const QString &baseUrl, const QString &apiKey);
    void setChatConfig(const QString &baseUrl,
                       const QString &apiKey,
                       double temperature,
                       int maxTokens,
                       int timeoutMs);
    void setEmbeddingConfig(const QString &baseUrl,
                            const QString &apiKey,
                            int timeoutMs);

    void startChatStream(const QJsonArray &messages,
                         const QString &model,
                         const QJsonArray &tools = QJsonArray(),
                         bool vision = false);
    void abort();

    QJsonObject chatComplete(const QJsonArray &messages,
                             const QString &model,
                             const QJsonArray &tools = QJsonArray(),
                             bool vision = false,
                             QString *errorMessage = nullptr);

    QVector<double> embedding(const QString &text, const QString &model, QString *errorMessage = nullptr);

    bool isBusy() const;
    QString lastError() const;

signals:
    void streamDelta(QString text);
    void streamToolCallDelta(const QJsonObject &toolCallDelta);
    void streamFinished(QString finishReason);
    void streamError(QString message);

private:
    QJsonObject buildRequestBody(const QJsonArray &messages,
                                 const QString &model,
                                 const QJsonArray &tools,
                                 bool vision,
                                 bool stream) const;
    QJsonObject postJsonSync(const QUrl &url,
                             const QByteArray &body,
                             const QString &apiKey,
                             int timeoutMs,
                             QString *errorMessage);
    void handleStreamLine(const QByteArray &line);

    QNetworkAccessManager m_nam;
    QNetworkReply *m_activeReply = nullptr;
    QString m_chatBaseUrl;
    QString m_chatApiKey;
    QString m_embeddingBaseUrl;
    QString m_embeddingApiKey;
    QString m_lastError;
    QByteArray m_sseBuffer;
    double m_temperature = 0.7;
    int m_maxTokens = 2048;
    int m_timeoutMs = 60000;
};
