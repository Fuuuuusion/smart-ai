#pragma once

#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>

struct KnowledgeDocument
{
    qint64 id = -1;
    QString title;
    QString path;
    QString format;
    int characterCount = 0;
    int chunkCount = 0;
    QString status;
    QDateTime createdAt;
};

struct KnowledgeChunk
{
    qint64 id = -1;
    qint64 documentId = -1;
    QString documentTitle;
    int index = 0;
    QString text;
    QVector<double> vector;
};

struct SearchHit
{
    qint64 documentId = -1;
    qint64 chunkId = -1;
    QString documentTitle;
    int chunkIndex = 0;
    QString text;
    double score = 0.0;
};

class KnowledgeStore
{
public:
    KnowledgeStore();
    ~KnowledgeStore();

    bool init(QString *error = nullptr);

    qint64 addDocument(const QString &title,
                       const QString &path,
                       const QString &format,
                       const QString &content,
                       const QStringList &chunks,
                       const QString &status,
                       QString *error = nullptr);
    bool deleteDocument(qint64 documentId, QString *error = nullptr);
    QList<KnowledgeDocument> documents(QString *error = nullptr) const;
    QList<KnowledgeChunk> chunksForDocument(qint64 documentId, QString *error = nullptr) const;
    QList<KnowledgeChunk> allChunks(QString *error = nullptr) const;

    bool updateChunkVector(qint64 chunkId, const QVector<double> &vector, QString *error = nullptr);
    bool updateDocumentStatus(qint64 documentId, const QString &status, QString *error = nullptr);

    QList<SearchHit> search(const QVector<double> &queryVector, int topK, QString *error = nullptr) const;

private:
    bool ensureSchema(QString *error);
    QSqlDatabase database() const;

    QString m_connectionName;
};
