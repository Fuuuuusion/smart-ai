#include "ChatHistoryStore.h"

#include "StoragePaths.h"

#include <algorithm>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QVariant>

namespace {
qint64 toLongLong(const QJsonValue &value)
{
    bool ok = false;
    const qint64 number = value.toVariant().toLongLong(&ok);
    return ok ? number : -1;
}

QJsonObject findConversation(const QJsonObject &data, qint64 id)
{
    const QJsonArray conversations = data.value(QStringLiteral("conversations")).toArray();
    for (const QJsonValue &value : conversations) {
        QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("id")).toVariant().toLongLong() == id)
            return object;
    }
    return {};
}
}

ChatHistoryStore::ChatHistoryStore() = default;
ChatHistoryStore::~ChatHistoryStore() = default;

QString ChatHistoryStore::dataDirectory() const
{
    return StoragePaths::dataDirectory();
}

QString ChatHistoryStore::filePath() const
{
    return StoragePaths::chatHistoryFilePath();
}

bool ChatHistoryStore::init(QString *error)
{
    if (!load(error))
        return false;
    if (m_data.value(QStringLiteral("conversations")).toArray().isEmpty()) {
        QString createError;
        if (createConversation(QStringLiteral("新对话"), &createError) < 0) {
            if (error)
                *error = createError;
            return false;
        }
    }
    return true;
}

bool ChatHistoryStore::load(QString *error)
{
    QFile file(filePath());
    if (!file.exists()) {
        m_data = QJsonObject();
        m_data.insert(QStringLiteral("conversations"), QJsonArray());
        m_data.insert(QStringLiteral("messages"), QJsonArray());
        m_data.insert(QStringLiteral("lastConversationId"), -1);
        m_data.insert(QStringLiteral("nextConversationId"), 1);
        m_data.insert(QStringLiteral("nextMessageId"), 1);
        return save(error);
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = parseError.errorString();
        return false;
    }
    m_data = document.object();
    if (!m_data.contains(QStringLiteral("conversations")))
        m_data.insert(QStringLiteral("conversations"), QJsonArray());
    if (!m_data.contains(QStringLiteral("messages")))
        m_data.insert(QStringLiteral("messages"), QJsonArray());
    if (!m_data.contains(QStringLiteral("lastConversationId")))
        m_data.insert(QStringLiteral("lastConversationId"), -1);
    if (!m_data.contains(QStringLiteral("nextConversationId")))
        m_data.insert(QStringLiteral("nextConversationId"), 1);
    if (!m_data.contains(QStringLiteral("nextMessageId")))
        m_data.insert(QStringLiteral("nextMessageId"), 1);
    return true;
}

bool ChatHistoryStore::save(QString *error) const
{
    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(m_data).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

qint64 ChatHistoryStore::createConversation(const QString &title, QString *error)
{
    const qint64 id = m_data.value(QStringLiteral("nextConversationId")).toVariant().toLongLong();
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonArray conversations = m_data.value(QStringLiteral("conversations")).toArray();
    QJsonObject conversation;
    conversation.insert(QStringLiteral("id"), id);
    conversation.insert(QStringLiteral("title"), title);
    conversation.insert(QStringLiteral("created_at"), now);
    conversation.insert(QStringLiteral("updated_at"), now);
    conversations.append(conversation);
    m_data.insert(QStringLiteral("conversations"), conversations);
    m_data.insert(QStringLiteral("nextConversationId"), id + 1);
    m_data.insert(QStringLiteral("lastConversationId"), id);
    if (!save(error))
        return -1;
    return id;
}

bool ChatHistoryStore::deleteConversation(qint64 conversationId, QString *error)
{
    QJsonArray conversations = m_data.value(QStringLiteral("conversations")).toArray();
    QJsonArray filtered;
    for (const QJsonValue &value : conversations) {
        QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("id")).toVariant().toLongLong() != conversationId)
            filtered.append(object);
    }
    m_data.insert(QStringLiteral("conversations"), filtered);

    QJsonArray messages = m_data.value(QStringLiteral("messages")).toArray();
    QJsonArray filteredMessages;
    for (const QJsonValue &value : messages) {
        QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("conversation_id")).toVariant().toLongLong() != conversationId)
            filteredMessages.append(object);
    }
    m_data.insert(QStringLiteral("messages"), filteredMessages);
    return save(error);
}

bool ChatHistoryStore::renameConversation(qint64 conversationId, const QString &title, QString *error)
{
    QJsonArray conversations = m_data.value(QStringLiteral("conversations")).toArray();
    for (int i = 0; i < conversations.size(); ++i) {
        QJsonObject object = conversations.at(i).toObject();
        if (object.value(QStringLiteral("id")).toVariant().toLongLong() == conversationId) {
            object.insert(QStringLiteral("title"), title);
            object.insert(QStringLiteral("updated_at"), QDateTime::currentDateTime().toString(Qt::ISODate));
            conversations.replace(i, object);
            m_data.insert(QStringLiteral("conversations"), conversations);
            return save(error);
        }
    }
    return false;
}

bool ChatHistoryStore::autoTitleConversation(qint64 conversationId, const QString &firstUserMessage, QString *error)
{
    QString title = firstUserMessage.simplified();
    if (title.length() > 32)
        title = title.left(32) + QStringLiteral("…");
    return renameConversation(conversationId, title, error);
}

QList<ConversationRecord> ChatHistoryStore::conversations(QString *error) const
{
    Q_UNUSED(error)
    QList<ConversationRecord> result;
    const QJsonArray conversations = m_data.value(QStringLiteral("conversations")).toArray();
    for (const QJsonValue &value : conversations) {
        const QJsonObject object = value.toObject();
        ConversationRecord record;
        record.id = toLongLong(object.value(QStringLiteral("id")));
        record.title = object.value(QStringLiteral("title")).toString();
        record.createdAt = QDateTime::fromString(object.value(QStringLiteral("created_at")).toString(), Qt::ISODate);
        record.updatedAt = QDateTime::fromString(object.value(QStringLiteral("updated_at")).toString(), Qt::ISODate);
        result.append(record);
    }
    std::sort(result.begin(), result.end(), [](const ConversationRecord &a, const ConversationRecord &b) {
        return a.updatedAt > b.updatedAt;
    });
    return result;
}

bool ChatHistoryStore::addMessage(qint64 conversationId,
                                  const QString &role,
                                  const QString &content,
                                  const QString &imageBase64,
                                  QString *error)
{
    const qint64 id = m_data.value(QStringLiteral("nextMessageId")).toVariant().toLongLong();
    QJsonArray messages = m_data.value(QStringLiteral("messages")).toArray();
    QJsonObject message;
    message.insert(QStringLiteral("id"), id);
    message.insert(QStringLiteral("conversation_id"), conversationId);
    message.insert(QStringLiteral("role"), role);
    message.insert(QStringLiteral("content"), content);
    message.insert(QStringLiteral("image_base64"), imageBase64);
    message.insert(QStringLiteral("created_at"), QDateTime::currentDateTime().toString(Qt::ISODate));
    messages.append(message);
    m_data.insert(QStringLiteral("messages"), messages);
    m_data.insert(QStringLiteral("nextMessageId"), id + 1);

    QJsonArray conversations = m_data.value(QStringLiteral("conversations")).toArray();
    for (int i = 0; i < conversations.size(); ++i) {
        QJsonObject object = conversations.at(i).toObject();
        if (object.value(QStringLiteral("id")).toVariant().toLongLong() == conversationId) {
            object.insert(QStringLiteral("updated_at"), QDateTime::currentDateTime().toString(Qt::ISODate));
            conversations.replace(i, object);
            break;
        }
    }
    m_data.insert(QStringLiteral("conversations"), conversations);
    return save(error);
}

QList<ChatMessageRecord> ChatHistoryStore::messages(qint64 conversationId, QString *error) const
{
    Q_UNUSED(error)
    QList<ChatMessageRecord> result;
    const QJsonArray messages = m_data.value(QStringLiteral("messages")).toArray();
    for (const QJsonValue &value : messages) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("conversation_id")).toVariant().toLongLong() != conversationId)
            continue;
        ChatMessageRecord record;
        record.id = toLongLong(object.value(QStringLiteral("id")));
        record.conversationId = conversationId;
        record.role = object.value(QStringLiteral("role")).toString();
        record.content = object.value(QStringLiteral("content")).toString();
        record.imageBase64 = object.value(QStringLiteral("image_base64")).toString();
        record.createdAt = QDateTime::fromString(object.value(QStringLiteral("created_at")).toString(), Qt::ISODate);
        result.append(record);
    }
    std::sort(result.begin(), result.end(), [](const ChatMessageRecord &a, const ChatMessageRecord &b) {
        return a.id < b.id;
    });
    return result;
}

qint64 ChatHistoryStore::lastConversationId() const
{
    return toLongLong(m_data.value(QStringLiteral("lastConversationId")));
}

void ChatHistoryStore::setLastConversationId(qint64 id)
{
    m_data.insert(QStringLiteral("lastConversationId"), id);
    save(nullptr);
}
