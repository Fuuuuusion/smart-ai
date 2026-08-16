#include "KnowledgeStore.h"

#include "LocalEmbedding.h"

#include <algorithm>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>

namespace {
QString defaultDatabasePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/smart-ai.db");
}

QString vectorToJson(const QVector<double> &vector)
{
    QJsonArray array;
    for (double value : vector)
        array.append(value);
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QVector<double> vectorFromJson(const QString &json)
{
    QVector<double> result;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    const QJsonArray array = doc.array();
    result.reserve(array.size());
    for (const QJsonValue &value : array)
        result.append(value.toDouble());
    return result;
}
}

KnowledgeStore::KnowledgeStore()
    : m_connectionName(QStringLiteral("smart_ai_knowledge_%1").arg(
          QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

KnowledgeStore::~KnowledgeStore()
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

QSqlDatabase KnowledgeStore::database() const
{
    if (!QSqlDatabase::contains(m_connectionName))
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    return QSqlDatabase::database(m_connectionName, false);
}

bool KnowledgeStore::init(QString *error)
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

bool KnowledgeStore::ensureSchema(QString *error)
{
    QSqlDatabase db = database();
    QSqlQuery query(db);
    const QStringList statements = {
        QStringLiteral("PRAGMA journal_mode = WAL;"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS knowledge_documents ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "title TEXT NOT NULL,"
                       "path TEXT NOT NULL,"
                       "format TEXT NOT NULL,"
                       "content TEXT NOT NULL,"
                       "character_count INTEGER NOT NULL DEFAULT 0,"
                       "chunk_count INTEGER NOT NULL DEFAULT 0,"
                       "status TEXT NOT NULL,"
                       "created_at TEXT NOT NULL);"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS knowledge_chunks ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "document_id INTEGER NOT NULL,"
                       "chunk_index INTEGER NOT NULL,"
                       "text TEXT NOT NULL,"
                       "vector_json TEXT,"
                       "FOREIGN KEY(document_id) REFERENCES knowledge_documents(id) ON DELETE CASCADE);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_knowledge_chunks_document ON knowledge_chunks(document_id, chunk_index);"),
    };
    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            if (error)
                *error = query.lastError().text();
            return false;
        }
    }
    return true;
}

qint64 KnowledgeStore::addDocument(const QString &title,
                                   const QString &path,
                                   const QString &format,
                                   const QString &content,
                                   const QStringList &chunks,
                                   const QString &status,
                                   QString *error)
{
    QSqlDatabase db = database();
    if (!db.transaction()) {
        if (error)
            *error = db.lastError().text();
        return -1;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("INSERT INTO knowledge_documents("
                                 "title, path, format, content, character_count, chunk_count, status, created_at) "
                                 "VALUES(:title, :path, :format, :content, :character_count, :chunk_count, :status, :created)"));
    query.bindValue(QStringLiteral(":title"), title);
    query.bindValue(QStringLiteral(":path"), path);
    query.bindValue(QStringLiteral(":format"), format);
    query.bindValue(QStringLiteral(":content"), content);
    query.bindValue(QStringLiteral(":character_count"), content.size());
    query.bindValue(QStringLiteral(":chunk_count"), chunks.size());
    query.bindValue(QStringLiteral(":status"), status);
    query.bindValue(QStringLiteral(":created"), QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!query.exec()) {
        db.rollback();
        if (error)
            *error = query.lastError().text();
        return -1;
    }
    const qint64 documentId = query.lastInsertId().toLongLong();

    QSqlQuery chunkQuery(db);
    chunkQuery.prepare(QStringLiteral("INSERT INTO knowledge_chunks(document_id, chunk_index, text, vector_json) "
                                      "VALUES(:document_id, :chunk_index, :text, :vector_json)"));
    for (int i = 0; i < chunks.size(); ++i) {
        chunkQuery.bindValue(QStringLiteral(":document_id"), documentId);
        chunkQuery.bindValue(QStringLiteral(":chunk_index"), i);
        chunkQuery.bindValue(QStringLiteral(":text"), chunks.at(i));
        chunkQuery.bindValue(QStringLiteral(":vector_json"), QVariant());
        if (!chunkQuery.exec()) {
            db.rollback();
            if (error)
                *error = chunkQuery.lastError().text();
            return -1;
        }
    }

    if (!db.commit()) {
        if (error)
            *error = db.lastError().text();
        return -1;
    }
    return documentId;
}

bool KnowledgeStore::deleteDocument(qint64 documentId, QString *error)
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("DELETE FROM knowledge_documents WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), documentId);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    return true;
}

QList<KnowledgeDocument> KnowledgeStore::documents(QString *error) const
{
    QList<KnowledgeDocument> result;
    QSqlQuery query(database());
    if (!query.exec(QStringLiteral("SELECT id, title, path, format, character_count, chunk_count, status, created_at "
                                   "FROM knowledge_documents ORDER BY created_at DESC"))) {
        if (error)
            *error = query.lastError().text();
        return result;
    }
    while (query.next()) {
        KnowledgeDocument doc;
        doc.id = query.value(0).toLongLong();
        doc.title = query.value(1).toString();
        doc.path = query.value(2).toString();
        doc.format = query.value(3).toString();
        doc.characterCount = query.value(4).toInt();
        doc.chunkCount = query.value(5).toInt();
        doc.status = query.value(6).toString();
        doc.createdAt = QDateTime::fromString(query.value(7).toString(), Qt::ISODate);
        result.append(doc);
    }
    return result;
}

QList<KnowledgeChunk> KnowledgeStore::chunksForDocument(qint64 documentId, QString *error) const
{
    QList<KnowledgeChunk> result;
    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT c.id, c.document_id, d.title, c.chunk_index, c.text, c.vector_json "
                                 "FROM knowledge_chunks c "
                                 "JOIN knowledge_documents d ON d.id = c.document_id "
                                 "WHERE c.document_id = :id ORDER BY c.chunk_index ASC"));
    query.bindValue(QStringLiteral(":id"), documentId);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return result;
    }
    while (query.next()) {
        KnowledgeChunk chunk;
        chunk.id = query.value(0).toLongLong();
        chunk.documentId = query.value(1).toLongLong();
        chunk.documentTitle = query.value(2).toString();
        chunk.index = query.value(3).toInt();
        chunk.text = query.value(4).toString();
        chunk.vector = vectorFromJson(query.value(5).toString());
        result.append(chunk);
    }
    return result;
}

QList<KnowledgeChunk> KnowledgeStore::allChunks(QString *error) const
{
    QList<KnowledgeChunk> result;
    QSqlQuery query(database());
    if (!query.exec(QStringLiteral("SELECT c.id, c.document_id, d.title, c.chunk_index, c.text, c.vector_json "
                                   "FROM knowledge_chunks c "
                                   "JOIN knowledge_documents d ON d.id = c.document_id "
                                   "ORDER BY c.document_id ASC, c.chunk_index ASC"))) {
        if (error)
            *error = query.lastError().text();
        return result;
    }
    while (query.next()) {
        KnowledgeChunk chunk;
        chunk.id = query.value(0).toLongLong();
        chunk.documentId = query.value(1).toLongLong();
        chunk.documentTitle = query.value(2).toString();
        chunk.index = query.value(3).toInt();
        chunk.text = query.value(4).toString();
        chunk.vector = vectorFromJson(query.value(5).toString());
        result.append(chunk);
    }
    return result;
}

bool KnowledgeStore::updateChunkVector(qint64 chunkId, const QVector<double> &vector, QString *error)
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("UPDATE knowledge_chunks SET vector_json = :vector WHERE id = :id"));
    query.bindValue(QStringLiteral(":vector"), vectorToJson(vector));
    query.bindValue(QStringLiteral(":id"), chunkId);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    return true;
}

bool KnowledgeStore::updateDocumentStatus(qint64 documentId, const QString &status, QString *error)
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral("UPDATE knowledge_documents SET status = :status WHERE id = :id"));
    query.bindValue(QStringLiteral(":status"), status);
    query.bindValue(QStringLiteral(":id"), documentId);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    return true;
}

QList<SearchHit> KnowledgeStore::search(const QVector<double> &queryVector, int topK, QString *error) const
{
    QList<SearchHit> hits;
    const QList<KnowledgeChunk> chunks = allChunks(error);
    for (const KnowledgeChunk &chunk : chunks) {
        SearchHit hit;
        hit.documentId = chunk.documentId;
        hit.chunkId = chunk.id;
        hit.documentTitle = chunk.documentTitle;
        hit.chunkIndex = chunk.index;
        hit.text = chunk.text;
        hit.score = chunk.vector.isEmpty() ? 0.0 : LocalEmbedding::cosineSimilarity(queryVector, chunk.vector);
        hits.append(hit);
    }

    std::sort(hits.begin(), hits.end(), [](const SearchHit &a, const SearchHit &b) {
        if (qFuzzyCompare(a.score + 1.0, b.score + 1.0))
            return a.chunkId < b.chunkId;
        return a.score > b.score;
    });
    if (topK > 0 && hits.size() > topK)
        hits = hits.mid(0, topK);
    return hits;
}
