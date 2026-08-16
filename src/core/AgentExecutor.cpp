#include "AgentExecutor.h"

#include "ApiClient.h"
#include "KnowledgeStore.h"
#include "LocalEmbedding.h"
#include "ToolRegistry.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

namespace {
QJsonObject messageObject(const QString &role, const QString &content)
{
    QJsonObject object;
    object.insert(QStringLiteral("role"), role);
    object.insert(QStringLiteral("content"), content);
    return object;
}
}

AgentExecutionResult AgentExecutor::run(const QString &userMessage,
                                        KnowledgeStore *knowledgeStore,
                                        const AgentConfig &config,
                                        QString *error)
{
    Q_UNUSED(knowledgeStore)
    AgentExecutionResult result;
    ApiClient client;
    client.setChatConfig(config.chatBaseUrl,
                         config.chatApiKey,
                         config.temperature,
                         config.maxTokens,
                         config.timeoutMs);
    client.setEmbeddingConfig(config.embeddingBaseUrl,
                              config.embeddingApiKey,
                              config.timeoutMs);

    KnowledgeStore localKnowledge;
    QString storeError;
    localKnowledge.init(&storeError);

    QJsonArray messages;
    QJsonObject system;
    system.insert(QStringLiteral("role"), QStringLiteral("system"));
    system.insert(QStringLiteral("content"),
                  QStringLiteral("你是 Smart AI，一个严谨的桌面智能助手。"
                                 "在工具能帮助回答用户时主动调用工具；"
                                 "当工具返回事实时，请清晰总结并引用工具结果，不要编造输出。"));
    messages.append(system);
    messages.append(messageObject(QStringLiteral("user"), userMessage));

    const QJsonArray tools = ToolRegistry::toolDefinitions();
    QString finalAnswer;

    for (int round = 0; round < 6; ++round) {
        QString chatError;
        const QJsonObject response = client.chatComplete(messages, config.chatModel, tools, false, &chatError);
        if (response.isEmpty()) {
            result.error = chatError.isEmpty() ? QStringLiteral("模型返回了空响应。") : chatError;
            return result;
        }

        const QJsonArray choices = response.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty()) {
            result.error = QStringLiteral("模型响应中没有 choices 字段。");
            return result;
        }
        const QJsonObject message = choices.first().toObject().value(QStringLiteral("message")).toObject();
        const QString content = message.value(QStringLiteral("content")).toString();
        const QJsonArray toolCalls = message.value(QStringLiteral("tool_calls")).toArray();

        QJsonObject assistantMessage;
        assistantMessage.insert(QStringLiteral("role"), QStringLiteral("assistant"));
        assistantMessage.insert(QStringLiteral("content"), content);
        if (!toolCalls.isEmpty())
            assistantMessage.insert(QStringLiteral("tool_calls"), toolCalls);
        messages.append(assistantMessage);

        if (toolCalls.isEmpty()) {
            finalAnswer = content;
            break;
        }

        for (const QJsonValue &value : toolCalls) {
            const QJsonObject toolCall = value.toObject();
            const QString id = toolCall.value(QStringLiteral("id")).toString();
            const QJsonObject function = toolCall.value(QStringLiteral("function")).toObject();
            const QString name = function.value(QStringLiteral("name")).toString();
            const QJsonObject arguments = QJsonDocument::fromJson(function.value(QStringLiteral("arguments")).toString().toUtf8()).object();

            AgentTraceStep step;
            step.label = QStringLiteral("工具调用：%1").arg(name);
            step.detail = QJsonDocument(arguments).toJson(QJsonDocument::Indented);
            result.trace.append(step);

            ToolContext context;
            context.knowledgeStore = &localKnowledge;
            context.topK = config.topK;
            if (name == QStringLiteral("knowledge_search")) {
                const QString query = arguments.value(QStringLiteral("query")).toString();
                context.knowledgeQuery = query;
                QString vectorError;
                context.knowledgeQueryVector = queryVector(query, config, &vectorError);
            }

            QString toolError;
            const ToolResult toolResult = ToolRegistry::execute(name, arguments, context, &toolError);

            AgentTraceStep resultStep;
            resultStep.label = QStringLiteral("工具结果：%1").arg(name);
            resultStep.detail = toolResult.ok ? toolResult.content : (toolError.isEmpty() ? toolResult.content : toolError);
            result.trace.append(resultStep);

            QJsonObject toolMessage;
            toolMessage.insert(QStringLiteral("role"), QStringLiteral("tool"));
            toolMessage.insert(QStringLiteral("tool_call_id"), id);
            toolMessage.insert(QStringLiteral("content"), toolResult.ok ? toolResult.content
                                                                       : QStringLiteral("错误：%1").arg(toolError.isEmpty() ? toolResult.content : toolError));
            messages.append(toolMessage);
        }
    }

    if (finalAnswer.isEmpty()) {
        result.error = QStringLiteral("智能体经过多轮工具调用后仍未生成最终回答。");
        return result;
    }

    result.ok = true;
    result.finalAnswer = finalAnswer;
    AgentTraceStep finalStep;
    finalStep.label = QStringLiteral("最终回答");
    finalStep.detail = finalAnswer;
    result.trace.append(finalStep);
    if (error)
        *error = QString();
    return result;
}

QVector<double> AgentExecutor::queryVector(const QString &query,
                                           const AgentConfig &config,
                                           QString *error)
{
    if (config.useLocalEmbedding)
        return LocalEmbedding::embed(query);

    ApiClient client;
    client.setEmbeddingConfig(config.embeddingBaseUrl,
                              config.embeddingApiKey,
                              config.timeoutMs);
    QString embeddingError;
    QVector<double> vector = client.embedding(query, config.embeddingModel, &embeddingError);
    if (!vector.isEmpty()) {
        if (error)
            *error = QString();
        return vector;
    }

    if (error)
        *error = QStringLiteral("远程向量化失败（%1），已使用本地回退方案。").arg(embeddingError);
    return LocalEmbedding::embed(query);
}
