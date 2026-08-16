#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

class KnowledgeStore;

struct ToolResult
{
    bool ok = false;
    QString content;
};

struct ToolContext
{
    KnowledgeStore *knowledgeStore = nullptr;
    QString knowledgeQuery;
    QVector<double> knowledgeQueryVector;
    int topK = 4;
};

class ToolRegistry
{
public:
    static QJsonArray toolDefinitions();
    static ToolResult execute(const QString &name,
                              const QJsonObject &arguments,
                              const ToolContext &context,
                              QString *error = nullptr);
};

