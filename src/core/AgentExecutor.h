#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVector>

class KnowledgeStore;

struct AgentTraceStep
{
    QString label;
    QString detail;
};

struct AgentConfig
{
    QString chatBaseUrl;
    QString chatApiKey;
    QString chatModel;
    QString embeddingBaseUrl;
    QString embeddingApiKey;
    QString embeddingModel;
    double temperature = 0.7;
    int maxTokens = 2048;
    int timeoutMs = 60000;
    int topK = 4;
    bool useLocalEmbedding = false;
};

struct AgentExecutionResult
{
    bool ok = false;
    QString finalAnswer;
    QString error;
    QList<AgentTraceStep> trace;
};

class AgentExecutor
{
public:
    static AgentExecutionResult run(const QString &userMessage,
                                    KnowledgeStore *knowledgeStore,
                                    const AgentConfig &config,
                                    QString *error = nullptr);

private:
    static QVector<double> queryVector(const QString &query,
                                       const AgentConfig &config,
                                       QString *error);
};
