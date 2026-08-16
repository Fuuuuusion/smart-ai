#include "ApiClient.h"

#include "AppSettings.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkRequest>
#include <QTimer>
#include <QEventLoop>
#include <QUrlQuery>

namespace {
QByteArray jsonBytes(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QJsonObject parseJson(const QByteArray &data, QString *error = nullptr)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error)
            *error = parseError.errorString();
        return {};
    }
    return doc.object();
}
}

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
{
}

void ApiClient::configureFromSettings()
{
    AppSettings *settings = AppSettings::instance();
    setChatEndpoint(settings->baseUrl(), settings->apiKey());
    setEmbeddingEndpoint(settings->embeddingBaseUrl(), settings->embeddingApiKey());
    m_temperature = settings->temperature();
    m_maxTokens = settings->maxTokens();
    m_timeoutMs = settings->timeoutMs();
}

void ApiClient::setChatEndpoint(const QString &baseUrl, const QString &apiKey)
{
    m_chatBaseUrl = AppSettings::normalizedBaseUrl(baseUrl);
    m_chatApiKey = apiKey.trimmed();
}

void ApiClient::setEmbeddingEndpoint(const QString &baseUrl, const QString &apiKey)
{
    m_embeddingBaseUrl = AppSettings::normalizedBaseUrl(baseUrl);
    m_embeddingApiKey = apiKey.trimmed();
}

void ApiClient::setChatConfig(const QString &baseUrl,
                              const QString &apiKey,
                              double temperature,
                              int maxTokens,
                              int timeoutMs)
{
    setChatEndpoint(baseUrl, apiKey);
    m_temperature = temperature;
    m_maxTokens = maxTokens;
    m_timeoutMs = timeoutMs;
}

void ApiClient::setEmbeddingConfig(const QString &baseUrl,
                                   const QString &apiKey,
                                   int timeoutMs)
{
    setEmbeddingEndpoint(baseUrl, apiKey);
    m_timeoutMs = timeoutMs;
}

QJsonObject ApiClient::buildRequestBody(const QJsonArray &messages,
                                        const QString &model,
                                        const QJsonArray &tools,
                                        bool vision,
                                        bool stream) const
{
    Q_UNUSED(vision)
    QJsonObject body;
    body.insert(QStringLiteral("model"), model);
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("temperature"), m_temperature);
    body.insert(QStringLiteral("max_tokens"), m_maxTokens);
    body.insert(QStringLiteral("stream"), stream);
    if (!tools.isEmpty())
        body.insert(QStringLiteral("tools"), tools);
    return body;
}

void ApiClient::startChatStream(const QJsonArray &messages,
                                const QString &model,
                                const QJsonArray &tools,
                                bool vision)
{
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }

    if (m_chatBaseUrl.isEmpty()) {
        emit streamError(tr("聊天接口地址未配置。"));
        return;
    }

    const QUrl url(m_chatBaseUrl + QStringLiteral("/chat/completions"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "text/event-stream");
    if (!m_chatApiKey.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + m_chatApiKey.toUtf8());

    const QJsonObject body = buildRequestBody(messages, model, tools, vision, true);
    m_lastError.clear();
    m_sseBuffer.clear();
    m_activeReply = m_nam.post(request, jsonBytes(body));

    connect(m_activeReply, &QNetworkReply::readyRead, this, [this]() {
        if (!m_activeReply)
            return;
        m_sseBuffer.append(m_activeReply->readAll());
        int newlineIndex = -1;
        while ((newlineIndex = m_sseBuffer.indexOf('\n')) >= 0) {
            QByteArray line = m_sseBuffer.left(newlineIndex).trimmed();
            m_sseBuffer.remove(0, newlineIndex + 1);
            if (!line.isEmpty())
                handleStreamLine(line);
        }
    });

    connect(m_activeReply, &QNetworkReply::finished, this, [this]() {
        if (!m_activeReply)
            return;
        QNetworkReply *reply = m_activeReply;
        m_activeReply = nullptr;

        if (!m_sseBuffer.trimmed().isEmpty())
            handleStreamLine(m_sseBuffer.trimmed());
        m_sseBuffer.clear();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray remaining = reply->readAll();
        if (!remaining.isEmpty()) {
            m_sseBuffer.append(remaining);
            int newlineIndex = -1;
            while ((newlineIndex = m_sseBuffer.indexOf('\n')) >= 0) {
                QByteArray line = m_sseBuffer.left(newlineIndex).trimmed();
                m_sseBuffer.remove(0, newlineIndex + 1);
                if (!line.isEmpty())
                    handleStreamLine(line);
            }
        }

        if (reply->error() != QNetworkReply::NoError) {
            m_lastError = reply->errorString();
            emit streamError(m_lastError);
        } else if (status >= 400) {
            m_lastError = QStringLiteral("HTTP %1").arg(status);
            emit streamError(m_lastError);
        } else {
            emit streamFinished(QStringLiteral("stop"));
        }
        reply->deleteLater();
    });
}

void ApiClient::abort()
{
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
}

void ApiClient::handleStreamLine(const QByteArray &line)
{
    QByteArray data = line;
    if (data.startsWith("data:"))
        data = data.mid(5).trimmed();
    if (data.isEmpty())
        return;
    if (data == "[DONE]") {
        emit streamFinished(QStringLiteral("stop"));
        return;
    }

    QString parseError;
    const QJsonObject object = parseJson(data, &parseError);
    if (object.isEmpty()) {
        if (!parseError.isEmpty())
            m_lastError = parseError;
        return;
    }

    const QJsonArray choices = object.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty())
        return;

    const QJsonObject choice = choices.first().toObject();
    const QJsonObject delta = choice.value(QStringLiteral("delta")).toObject();
    if (delta.contains(QStringLiteral("content")))
        emit streamDelta(delta.value(QStringLiteral("content")).toString());

    if (delta.contains(QStringLiteral("tool_calls"))) {
        const QJsonArray calls = delta.value(QStringLiteral("tool_calls")).toArray();
        for (const QJsonValue &value : calls)
            emit streamToolCallDelta(value.toObject());
    }

    if (!choice.value(QStringLiteral("finish_reason")).toString().isEmpty())
        emit streamFinished(choice.value(QStringLiteral("finish_reason")).toString());
}

QJsonObject ApiClient::chatComplete(const QJsonArray &messages,
                                    const QString &model,
                                    const QJsonArray &tools,
                                    bool vision,
                                    QString *errorMessage)
{
    if (m_chatBaseUrl.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("聊天接口地址未配置。");
        return {};
    }

    const QUrl url(m_chatBaseUrl + QStringLiteral("/chat/completions"));
    const QJsonObject body = buildRequestBody(messages, model, tools, vision, false);
    QString localError;
    const QJsonObject result = postJsonSync(url, jsonBytes(body), m_chatApiKey,
                                            m_timeoutMs, &localError);
    if (errorMessage)
        *errorMessage = localError;
    return result;
}

QVector<double> ApiClient::embedding(const QString &text, const QString &model, QString *errorMessage)
{
    if (m_embeddingBaseUrl.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("向量化接口地址未配置。");
        return {};
    }

    const QUrl url(m_embeddingBaseUrl + QStringLiteral("/embeddings"));
    QJsonObject body;
    body.insert(QStringLiteral("model"), model);
    body.insert(QStringLiteral("input"), text);

    QString localError;
    const QJsonObject result = postJsonSync(url, jsonBytes(body), m_embeddingApiKey,
                                            m_timeoutMs, &localError);
    if (errorMessage)
        *errorMessage = localError;
    if (result.isEmpty())
        return {};

    const QJsonArray data = result.value(QStringLiteral("data")).toArray();
    if (data.isEmpty())
        return {};

    const QJsonArray vector = data.first().toObject().value(QStringLiteral("embedding")).toArray();
    QVector<double> values;
    values.reserve(vector.size());
    for (const QJsonValue &value : vector)
        values.append(value.toDouble());
    return values;
}

QJsonObject ApiClient::postJsonSync(const QUrl &url,
                                    const QByteArray &body,
                                    const QString &apiKey,
                                    int timeoutMs,
                                    QString *errorMessage)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!apiKey.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + apiKey.toUtf8());

    QNetworkReply *reply = m_nam.post(request, body);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&loop, reply]() {
        reply->abort();
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, [&loop]() {
        loop.quit();
    });
    timer.start(timeoutMs);
    loop.exec();
    timer.stop();

    const QByteArray responseData = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError networkError = reply->error();
    reply->deleteLater();

    if (networkError != QNetworkReply::NoError) {
        if (errorMessage)
            *errorMessage = networkError == QNetworkReply::OperationCanceledError
                                ? tr("请求超时。")
                                : reply->errorString();
        return {};
    }
    if (status >= 400) {
        if (errorMessage) {
            QString detail;
            const QJsonObject errorObject = parseJson(responseData);
            detail = errorObject.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
            if (detail.isEmpty())
                detail = QString::fromUtf8(responseData).left(300);
            *errorMessage = QStringLiteral("HTTP %1: %2").arg(status).arg(detail);
        }
        return {};
    }
    return parseJson(responseData, errorMessage);
}

bool ApiClient::isBusy() const
{
    return m_activeReply != nullptr;
}

QString ApiClient::lastError() const
{
    return m_lastError;
}
