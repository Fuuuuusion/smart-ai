#include "KnowledgeStore.h"

#include "LocalEmbedding.h"
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

QString documentTitle(const QJsonArray &documents, qint64 documentId)
{
    for (const QJsonValue &value : documents) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("id")).toVariant().toLongLong() == documentId)
            return object.value(QStringLiteral("title")).toString();
    }
    return {};
}
}

KnowledgeStore::KnowledgeStore() = default;
KnowledgeStore::~KnowledgeStore() = default;

QString KnowledgeStore::dataDirectory() const
{
    return StoragePaths::dataDirectory();
}

QString KnowledgeStore::filePath() const
{
    return StoragePaths::knowledgeFilePath();
}

bool KnowledgeStore::init(QString *error)
{
    if (!load(error))
        return false;
    return true;
}

bool KnowledgeStore::load(QString *error)
{
    QFile file(filePath());
    if (!file.exists()) {
        m_data = QJsonObject();
        m_data.insert(QStringLiteral("documents"), QJsonArray());
        m_data.insert(QStringLiteral("chunks"), QJsonArray());
        m_data.insert(QStringLiteral("nextDocumentId"), 1);
        m_data.insert(QStringLiteral("nextChunkId"), 1);
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
    if (!m_data.contains(QStringLiteral("documents")))
        m_data.insert(QStringLiteral("documents"), QJsonArray());
    if (!m_data.contains(QStringLiteral("chunks")))
        m_data.insert(QStringLiteral("chunks"), QJsonArray());
    if (!m_data.contains(QStringLiteral("nextDocumentId")))
        m_data.insert(QStringLiteral("nextDocumentId"), 1);
    if (!m_data.contains(QStringLiteral("nextChunkId")))
        m_data.insert(QStringLiteral("nextChunkId"), 1);
    return true;
}

bool KnowledgeStore::save(QString *error) const
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

qint64 KnowledgeStore::addDocument(const QString &title,
                                   const QString &path,
                                   const QString &format,
                                   const QString &content,
                                   const QStringList &chunks,
                                   const QString &status,
                                   QString *error)
{
    const qint64 documentId = m_data.value(QStringLiteral("nextDocumentId")).toVariant().toLongLong();
    qint64 nextChunkId = m_data.value(QStringLiteral("nextChunkId")).toVariant().toLongLong();

    QJsonArray documents = m_data.value(QStringLiteral("documents")).toArray();
    QJsonObject document;
    document.insert(QStringLiteral("id"), documentId);
    document.insert(QStringLiteral("title"), title);
    document.insert(QStringLiteral("path"), path);
    document.insert(QStringLiteral("format"), format);
    document.insert(QStringLiteral("content"), content);
    document.insert(QStringLiteral("character_count"), content.size());
    document.insert(QStringLiteral("chunk_count"), chunks.size());
    document.insert(QStringLiteral("status"), status);
    document.insert(QStringLiteral("created_at"), QDateTime::currentDateTime().toString(Qt::ISODate));
    documents.append(document);

    QJsonArray chunkArray = m_data.value(QStringLiteral("chunks")).toArray();
    for (int i = 0; i < chunks.size(); ++i) {
        QJsonObject chunk;
        chunk.insert(QStringLiteral("id"), nextChunkId);
        chunk.insert(QStringLiteral("document_id"), documentId);
        chunk.insert(QStringLiteral("chunk_index"), i);
        chunk.insert(QStringLiteral("text"), chunks.at(i));
        chunk.insert(QStringLiteral("vector_json"), QString());
        chunkArray.append(chunk);
        ++nextChunkId;
    }

    m_data.insert(QStringLiteral("documents"), documents);
    m_data.insert(QStringLiteral("chunks"), chunkArray);
    m_data.insert(QStringLiteral("nextDocumentId"), documentId + 1);
    m_data.insert(QStringLiteral("nextChunkId"), nextChunkId);
    if (!save(error))
        return -1;
    return documentId;
}

bool KnowledgeStore::deleteDocument(qint64 documentId, QString *error)
{
    QJsonArray documents = m_data.value(QStringLiteral("documents")).toArray();
    QJsonArray filteredDocuments;
    for (const QJsonValue &value : documents) {
        QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("id")).toVariant().toLongLong() != documentId)
            filteredDocuments.append(object);
    }

    QJsonArray chunks = m_data.value(QStringLiteral("chunks")).toArray();
    QJsonArray filteredChunks;
    for (const QJsonValue &value : chunks) {
        QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("document_id")).toVariant().toLongLong() != documentId)
            filteredChunks.append(object);
    }
    m_data.insert(QStringLiteral("documents"), filteredDocuments);
    m_data.insert(QStringLiteral("chunks"), filteredChunks);
    return save(error);
}

QList<KnowledgeDocument> KnowledgeStore::documents(QString *error) const
{
    Q_UNUSED(error)
    QList<KnowledgeDocument> result;
    const QJsonArray documents = m_data.value(QStringLiteral("documents")).toArray();
    for (const QJsonValue &value : documents) {
        const QJsonObject object = value.toObject();
        KnowledgeDocument doc;
        doc.id = toLongLong(object.value(QStringLiteral("id")));
        doc.title = object.value(QStringLiteral("title")).toString();
        doc.path = object.value(QStringLiteral("path")).toString();
        doc.format = object.value(QStringLiteral("format")).toString();
        doc.characterCount = object.value(QStringLiteral("character_count")).toInt();
        doc.chunkCount = object.value(QStringLiteral("chunk_count")).toInt();
        doc.status = object.value(QStringLiteral("status")).toString();
        doc.createdAt = QDateTime::fromString(object.value(QStringLiteral("created_at")).toString(), Qt::ISODate);
        result.append(doc);
    }
    std::sort(result.begin(), result.end(), [](const KnowledgeDocument &a, const KnowledgeDocument &b) {
        return a.createdAt > b.createdAt;
    });
    return result;
}

QList<KnowledgeChunk> KnowledgeStore::chunksForDocument(qint64 documentId, QString *error) const
{
    Q_UNUSED(error)
    QList<KnowledgeChunk> result;
    const QJsonArray documents = m_data.value(QStringLiteral("documents")).toArray();
    const QString title = documentTitle(documents, documentId);
    const QJsonArray chunks = m_data.value(QStringLiteral("chunks")).toArray();
    for (const QJsonValue &value : chunks) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("document_id")).toVariant().toLongLong() != documentId)
            continue;
        KnowledgeChunk chunk;
        chunk.id = toLongLong(object.value(QStringLiteral("id")));
        chunk.documentId = documentId;
        chunk.documentTitle = title;
        chunk.index = object.value(QStringLiteral("chunk_index")).toInt();
        chunk.text = object.value(QStringLiteral("text")).toString();
        chunk.vector = vectorFromJson(object.value(QStringLiteral("vector_json")).toString());
        result.append(chunk);
    }
    std::sort(result.begin(), result.end(), [](const KnowledgeChunk &a, const KnowledgeChunk &b) {
        return a.index < b.index;
    });
    return result;
}

QList<KnowledgeChunk> KnowledgeStore::allChunks(QString *error) const
{
    Q_UNUSED(error)
    QList<KnowledgeChunk> result;
    const QJsonArray documents = m_data.value(QStringLiteral("documents")).toArray();
    const QJsonArray chunks = m_data.value(QStringLiteral("chunks")).toArray();
    for (const QJsonValue &value : chunks) {
        const QJsonObject object = value.toObject();
        KnowledgeChunk chunk;
        chunk.id = toLongLong(object.value(QStringLiteral("id")));
        chunk.documentId = toLongLong(object.value(QStringLiteral("document_id")));
        chunk.documentTitle = documentTitle(documents, chunk.documentId);
        chunk.index = object.value(QStringLiteral("chunk_index")).toInt();
        chunk.text = object.value(QStringLiteral("text")).toString();
        chunk.vector = vectorFromJson(object.value(QStringLiteral("vector_json")).toString());
        result.append(chunk);
    }
    std::sort(result.begin(), result.end(), [](const KnowledgeChunk &a, const KnowledgeChunk &b) {
        if (a.documentId == b.documentId)
            return a.index < b.index;
        return a.documentId < b.documentId;
    });
    return result;
}

bool KnowledgeStore::updateChunkVector(qint64 chunkId, const QVector<double> &vector, QString *error)
{
    QJsonArray chunks = m_data.value(QStringLiteral("chunks")).toArray();
    for (int i = 0; i < chunks.size(); ++i) {
        QJsonObject object = chunks.at(i).toObject();
        if (object.value(QStringLiteral("id")).toVariant().toLongLong() == chunkId) {
            object.insert(QStringLiteral("vector_json"), vectorToJson(vector));
            chunks.replace(i, object);
            m_data.insert(QStringLiteral("chunks"), chunks);
            return save(error);
        }
    }
    return false;
}

bool KnowledgeStore::updateDocumentStatus(qint64 documentId, const QString &status, QString *error)
{
    QJsonArray documents = m_data.value(QStringLiteral("documents")).toArray();
    for (int i = 0; i < documents.size(); ++i) {
        QJsonObject object = documents.at(i).toObject();
        if (object.value(QStringLiteral("id")).toVariant().toLongLong() == documentId) {
            object.insert(QStringLiteral("status"), status);
            documents.replace(i, object);
            m_data.insert(QStringLiteral("documents"), documents);
            return save(error);
        }
    }
    return false;
}

QList<SearchHit> KnowledgeStore::search(const QVector<double> &queryVector, int topK, QString *error) const
{
    Q_UNUSED(error)
    QList<SearchHit> hits;
    const QList<KnowledgeChunk> chunks = allChunks();
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
