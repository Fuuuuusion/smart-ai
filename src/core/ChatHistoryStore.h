#pragma once

#include <QList>
#include <QDateTime>
#include <QSqlDatabase>
#include <QString>

struct ConversationRecord
{
    qint64 id = -1;
    QString title;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct ChatMessageRecord
{
    qint64 id = -1;
    QString role;
    QString content;
    QString imageBase64;
    QDateTime createdAt;
};

class ChatHistoryStore
{
public:
    ChatHistoryStore();
    ~ChatHistoryStore();

    bool init(QString *error = nullptr);

    qint64 createConversation(const QString &title = QStringLiteral("New conversation"), QString *error = nullptr);
    bool deleteConversation(qint64 conversationId, QString *error = nullptr);
    bool renameConversation(qint64 conversationId, const QString &title, QString *error = nullptr);
    bool autoTitleConversation(qint64 conversationId, const QString &firstUserMessage, QString *error = nullptr);
    QList<ConversationRecord> conversations(QString *error = nullptr) const;

    bool addMessage(qint64 conversationId,
                    const QString &role,
                    const QString &content,
                    const QString &imageBase64 = QString(),
                    QString *error = nullptr);
    QList<ChatMessageRecord> messages(qint64 conversationId, QString *error = nullptr) const;

    qint64 lastConversationId() const;
    void setLastConversationId(qint64 id);

private:
    bool ensureSchema(QString *error);
    QSqlDatabase database() const;

    QString m_connectionName = QStringLiteral("smart_ai_chat_history");
};

