#include "ChatHistoryStore.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSettings>
#include <QStandardPaths>
#include <QVariant>

namespace {
QString defaultDatabasePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/smart-ai.db");
}
}

ChatHistoryStore::ChatHistoryStore() = default;

ChatHistoryStore::~ChatHistoryStore()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

QSqlDatabase ChatHistoryStore::database() const
{
    if (!QSqlDatabase::contains(m_connectionName))
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    return QSqlDatabase::database(m_connectionName, false);
}

bool ChatHistoryStore::init(QString *error)
{
    QSqlDatabase db = database();
    if (!db.isOpen()) {
        db.setDatabaseName(defaultDatabasePath());
        if (!db.open()) {
            if (error)
                *error = db.lastError().text();
            return false;
        }
    }
    return ensureSchema(error);
}

bool ChatHistoryStore::ensureSchema(QString *error)
{
    QSqlDatabase db = database();
    QSqlQuery query(db);
    const QStringList statements = {
        QStringLiteral("PRAGMA journal_mode = WAL;"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS conversations ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "title TEXT NOT NULL,"
                       "created_at TEXT NOT NULL,"
                       "updated_at TEXT NOT NULL);"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS messages ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "conversation_id INTEGER NOT NULL,"
                       "role TEXT NOT NULL,"
                       "content TEXT NOT NULL,"
                       "image_base64 TEXT,"
                       "created_at TEXT NOT NULL,"
                       "FOREIGN KEY(conversation_id) REFERENCES conversations(id) ON DELETE CASCADE);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_messages_conversation ON messages(conversation_id, id);"),
    };
    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            if (error)
                *error = query.lastError().text();
            return false;
        }
    }

    QSqlQuery countQuery(db);
    if (countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM conversations")) && countQuery.next()) {
        if (countQuery.value(0).toLongLong() == 0)
            createConversation(QStringLiteral("新对话"), error);
    }
    return true;
}

qint64 ChatHistoryStore::createConversation(const QString &title, QString *error)
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("INSERT INTO conversations(title, created_at, updated_at) VALUES(:title, :created, :updated)"));
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    query.bindValue(QStringLiteral(":title"), title);
    query.bindValue(QStringLiteral(":created"), now);
    query.bindValue(QStringLiteral(":updated"), now);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return -1;
    }
    const qint64 id = query.lastInsertId().toLongLong();
    setLastConversationId(id);
    return id;
}

bool ChatHistoryStore::deleteConversation(qint64 conversationId, QString *error)
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("DELETE FROM conversations WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), conversationId);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    return true;
}

bool ChatHistoryStore::renameConversation(qint64 conversationId, const QString &title, QString *error)
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("UPDATE conversations SET title = :title, updated_at = :updated WHERE id = :id"));
    query.bindValue(QStringLiteral(":title"), title);
    query.bindValue(QStringLiteral(":updated"), QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":id"), conversationId);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    return true;
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
    QList<ConversationRecord> result;
    QSqlQuery query(database());
    if (!query.exec(QStringLiteral("SELECT id, title, created_at, updated_at FROM conversations ORDER BY updated_at DESC"))) {
        if (error)
            *error = query.lastError().text();
        return result;
    }
    while (query.next()) {
        ConversationRecord item;
        item.id = query.value(0).toLongLong();
        item.title = query.value(1).toString();
        item.createdAt = QDateTime::fromString(query.value(2).toString(), Qt::ISODate);
        item.updatedAt = QDateTime::fromString(query.value(3).toString(), Qt::ISODate);
        result.append(item);
    }
    return result;
}

bool ChatHistoryStore::addMessage(qint64 conversationId,
                                  const QString &role,
                                  const QString &content,
                                  const QString &imageBase64,
                                  QString *error)
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("INSERT INTO messages(conversation_id, role, content, image_base64, created_at) "
                                 "VALUES(:conversation_id, :role, :content, :image, :created)"));
    query.bindValue(QStringLiteral(":conversation_id"), conversationId);
    query.bindValue(QStringLiteral(":role"), role);
    query.bindValue(QStringLiteral(":content"), content);
    query.bindValue(QStringLiteral(":image"), imageBase64);
    query.bindValue(QStringLiteral(":created"), QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return false;
    }

    QSqlQuery update(database());
    update.prepare(QStringLiteral("UPDATE conversations SET updated_at = :updated WHERE id = :id"));
    update.bindValue(QStringLiteral(":updated"), QDateTime::currentDateTime().toString(Qt::ISODate));
    update.bindValue(QStringLiteral(":id"), conversationId);
    update.exec();
    return true;
}

QList<ChatMessageRecord> ChatHistoryStore::messages(qint64 conversationId, QString *error) const
{
    QList<ChatMessageRecord> result;
    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT id, role, content, image_base64, created_at FROM messages "
                                 "WHERE conversation_id = :conversation_id ORDER BY id ASC"));
    query.bindValue(QStringLiteral(":conversation_id"), conversationId);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return result;
    }
    while (query.next()) {
        ChatMessageRecord item;
        item.id = query.value(0).toLongLong();
        item.role = query.value(1).toString();
        item.content = query.value(2).toString();
        item.imageBase64 = query.value(3).toString();
        item.createdAt = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);
        result.append(item);
    }
    return result;
}

qint64 ChatHistoryStore::lastConversationId() const
{
    QSettings settings(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/smart-ai.ini"),
                       QSettings::IniFormat);
    return settings.value(QStringLiteral("chat/last_conversation"), -1).toLongLong();
}

void ChatHistoryStore::setLastConversationId(qint64 id)
{
    QSettings settings(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/smart-ai.ini"),
                       QSettings::IniFormat);
    settings.setValue(QStringLiteral("chat/last_conversation"), id);
    settings.sync();
}
