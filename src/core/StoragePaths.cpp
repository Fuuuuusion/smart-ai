#include "StoragePaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace {
QString writableDataDirectory()
{
    const QString preferred = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(preferred);
    QFile probe(preferred + QStringLiteral("/.write-test"));
    if (probe.open(QIODevice::WriteOnly)) {
        probe.close();
        probe.remove();
        return preferred;
    }

    const QString fallback = QCoreApplication::applicationDirPath() + QStringLiteral("/smart-ai-data");
    QDir().mkpath(fallback);
    return fallback;
}
}

QString StoragePaths::dataDirectory()
{
    return writableDataDirectory();
}

QString StoragePaths::settingsFilePath()
{
    return dataDirectory() + QStringLiteral("/smart-ai.ini");
}

QString StoragePaths::chatHistoryFilePath()
{
    return dataDirectory() + QStringLiteral("/chat-history.json");
}

QString StoragePaths::knowledgeFilePath()
{
    return dataDirectory() + QStringLiteral("/knowledge.json");
}
