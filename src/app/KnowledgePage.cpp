#include "KnowledgePage.h"

#include "core/ApiClient.h"
#include "core/AppSettings.h"
#include "core/DocumentExtractor.h"
#include "core/LocalEmbedding.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTextBrowser>
#include <QAbstractItemView>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <numeric>

KnowledgePage::KnowledgePage(QWidget *parent)
    : QWidget(parent)
    , m_store(new KnowledgeStore)
    , m_api(new ApiClient(this))
{
    QString error;
    if (!m_store->init(&error))
        QMessageBox::warning(this, tr("知识库"), error);
    m_api->configureFromSettings();
    setupUi();
    refreshDocuments();

    connect(&m_importWatcher, &QFutureWatcher<QString>::finished, this, [this]() {
        const QString message = m_importWatcher.result();
        if (!message.isEmpty())
            QMessageBox::information(this, tr("导入文档"), message);
        refreshDocuments();
        setBusy(false);
    });

    connect(&m_searchWatcher, &QFutureWatcher<QList<SearchHit>>::finished, this, [this]() {
        const QList<SearchHit> hits = m_searchWatcher.result();
        m_contextList->clear();
        for (const SearchHit &hit : hits) {
            QListWidgetItem *item = new QListWidgetItem(QStringLiteral("%1 · chunk %2 · %3")
                                                            .arg(hit.documentTitle)
                                                            .arg(hit.chunkIndex + 1)
                                                            .arg(hit.score, 0, 'f', 3));
            item->setToolTip(hit.text);
            m_contextList->addItem(item);
        }
        startRagAnswer(m_lastQuestion, hits);
    });

    connect(m_api, &ApiClient::streamDelta, this, [this](const QString &delta) {
        m_answer += delta;
        m_answerBrowser->setMarkdown(m_answer + QStringLiteral(" ▍"));
    });
    connect(m_api, &ApiClient::streamFinished, this, [this](const QString &) {
        m_answerBrowser->setMarkdown(m_answer);
        setBusy(false);
        emit streamingStateChanged(false);
    });
    connect(m_api, &ApiClient::streamError, this, [this](const QString &message) {
        m_answer = message.isEmpty() ? tr("知识库请求失败。") : message;
        m_answerBrowser->setMarkdown(m_answer);
        setBusy(false);
        emit streamingStateChanged(false);
    });
}

void KnowledgePage::setupUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    QHBoxLayout *toolbar = new QHBoxLayout;
    m_importButton = new QPushButton(tr("导入文档"), this);
    m_importButton->setObjectName(QStringLiteral("PrimaryButton"));
    m_deleteButton = new QPushButton(tr("删除选中"), this);
    m_deleteButton->setObjectName(QStringLiteral("DangerButton"));
    m_statusLabel = new QLabel(tr("文档仅保存在本地，并通过向量化用于私有知识检索。"), this);
    m_statusLabel->setObjectName(QStringLiteral("MutedLabel"));
    toolbar->addWidget(m_importButton);
    toolbar->addWidget(m_deleteButton);
    toolbar->addStretch();
    toolbar->addWidget(m_statusLabel);
    root->addLayout(toolbar);

    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({ tr("标题"), tr("格式"), tr("字符数"), tr("分块数"), tr("状态"), tr("创建时间") });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 2);

    QSplitter *bottom = new QSplitter(Qt::Horizontal, this);
    QFrame *contextPanel = new QFrame(bottom);
    contextPanel->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *contextLayout = new QVBoxLayout(contextPanel);
    QLabel *contextTitle = new QLabel(tr("检索依据"), contextPanel);
    contextTitle->setObjectName(QStringLiteral("CardTitle"));
    contextLayout->addWidget(contextTitle);
    m_contextList = new QListWidget(contextPanel);
    contextLayout->addWidget(m_contextList);

    QFrame *answerPanel = new QFrame(bottom);
    answerPanel->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *answerLayout = new QVBoxLayout(answerPanel);
    QLabel *answerTitle = new QLabel(tr("知识库回答"), answerPanel);
    answerTitle->setObjectName(QStringLiteral("CardTitle"));
    answerLayout->addWidget(answerTitle);
    m_answerBrowser = new QTextBrowser(answerPanel);
    m_answerBrowser->setOpenExternalLinks(true);
    m_answerBrowser->setPlaceholderText(tr("输入问题，回答将基于你导入的文档生成。"));
    m_answerBrowser->document()->setDefaultStyleSheet(QStringLiteral(
        "body { color:#e8edf7; } code { color:#9fc1ff; } pre { background:#0c1320; border-radius:6px; padding:8px; }"));
    answerLayout->addWidget(m_answerBrowser, 1);

    QHBoxLayout *questionRow = new QHBoxLayout;
    m_question = new QPlainTextEdit(answerPanel);
    m_question->setPlaceholderText(tr("针对你的私有知识库提问…"));
    m_question->setMinimumHeight(64);
    m_askButton = new QPushButton(tr("提问"), answerPanel);
    m_askButton->setObjectName(QStringLiteral("PrimaryButton"));
    m_askButton->setMinimumWidth(90);
    questionRow->addWidget(m_question, 1);
    questionRow->addWidget(m_askButton);
    answerLayout->addLayout(questionRow);

    bottom->addWidget(contextPanel);
    bottom->addWidget(answerPanel);
    bottom->setStretchFactor(0, 1);
    bottom->setStretchFactor(1, 3);
    root->addWidget(bottom, 3);

    connect(m_importButton, &QPushButton::clicked, this, &KnowledgePage::importDocuments);
    connect(m_deleteButton, &QPushButton::clicked, this, &KnowledgePage::deleteSelectedDocument);
    connect(m_askButton, &QPushButton::clicked, this, &KnowledgePage::askQuestion);
}

void KnowledgePage::refreshDocuments()
{
    m_table->setRowCount(0);
    const QList<KnowledgeDocument> documents = m_store->documents();
    m_table->setRowCount(documents.size());
    for (int row = 0; row < documents.size(); ++row) {
        const KnowledgeDocument &doc = documents.at(row);
        auto item = [](const QString &text) {
            return new QTableWidgetItem(text);
        };
        m_table->setItem(row, 0, item(doc.title));
        m_table->setItem(row, 1, item(doc.format));
        m_table->setItem(row, 2, item(QString::number(doc.characterCount)));
        m_table->setItem(row, 3, item(QString::number(doc.chunkCount)));
        QString status = doc.status;
        if (status == QStringLiteral("indexing"))
            status = tr("索引中");
        else if (status == QStringLiteral("ready"))
            status = tr("就绪");
        else if (status == QStringLiteral("partial"))
            status = tr("部分完成");
        m_table->setItem(row, 4, item(status));
        m_table->setItem(row, 5, item(doc.createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
        m_table->item(row, 0)->setData(Qt::UserRole, doc.id);
    }
    m_statusLabel->setText(tr("共 %1 个文档 · %2 个分块").arg(documents.size()).arg(
        documents.isEmpty() ? 0 : std::accumulate(documents.begin(), documents.end(), 0,
                                                  [](int sum, const KnowledgeDocument &doc) { return sum + doc.chunkCount; })));
}

void KnowledgePage::importDocuments()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("导入文档"), QString(),
                                                            tr("文档 (*.txt *.md *.markdown *.docx *.pdf)"));
    if (paths.isEmpty())
        return;

    ImportJob job;
    job.paths = paths;
    AppSettings *settings = AppSettings::instance();
    job.chunkSize = settings->chunkSize();
    job.chunkOverlap = settings->chunkOverlap();
    job.useLocalEmbedding = settings->useLocalEmbedding();
    job.embeddingModel = settings->embeddingModel();
    job.embeddingBaseUrl = settings->embeddingBaseUrl();
    job.embeddingApiKey = settings->embeddingApiKey();
    job.timeoutMs = settings->timeoutMs();

    setBusy(true);
    m_statusLabel->setText(tr("正在导入并向量化文档…"));
    m_importWatcher.setFuture(QtConcurrent::run([this, job]() { return runImportJob(job); }));
}

QString KnowledgePage::runImportJob(const ImportJob &job)
{
    QStringList summaries;
    QStringList errors;
    KnowledgeStore workerStore;
    QString storeError;
    if (!workerStore.init(&storeError))
        return storeError;

    ApiClient client;
    client.setEmbeddingConfig(job.embeddingBaseUrl, job.embeddingApiKey, job.timeoutMs);

    for (const QString &path : job.paths) {
        QString extractError;
        const ExtractedDocument document = DocumentExtractor::extract(path, &extractError);
        if (document.text.isEmpty()) {
            errors.append(QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), extractError));
            continue;
        }

        const QStringList chunks = chunkText(document.text, job.chunkSize, job.chunkOverlap);
        QString addError;
        const qint64 documentId = workerStore.addDocument(document.title, path, document.format,
                                                          document.text, chunks,
                                                          QStringLiteral("indexing"), &addError);
        if (documentId < 0) {
            errors.append(QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), addError));
            continue;
        }

        const QList<KnowledgeChunk> storedChunks = workerStore.chunksForDocument(documentId);
        int indexed = 0;
        for (const KnowledgeChunk &chunk : storedChunks) {
            QVector<double> vector;
            if (!job.useLocalEmbedding) {
                QString embeddingError;
                vector = client.embedding(chunk.text, job.embeddingModel, &embeddingError);
                if (vector.isEmpty())
                    vector = LocalEmbedding::embed(chunk.text);
            } else {
                vector = LocalEmbedding::embed(chunk.text);
            }
            if (!vector.isEmpty() && workerStore.updateChunkVector(chunk.id, vector))
                ++indexed;
        }
        workerStore.updateDocumentStatus(documentId, indexed == chunks.size() ? QStringLiteral("ready")
                                                                              : QStringLiteral("partial"));
        summaries.append(QStringLiteral("%1 (%2 chunks, %3 indexed)")
                             .arg(document.title)
                             .arg(chunks.size())
                             .arg(indexed));
    }

    QString result;
    if (!summaries.isEmpty())
        result += tr("已导入：%1").arg(summaries.join(QStringLiteral("，")));
    if (!errors.isEmpty()) {
        if (!result.isEmpty())
            result += QStringLiteral("\n\n");
        result += tr("已跳过：%1").arg(errors.join(QStringLiteral("\n")));
    }
    return result;
}

QStringList KnowledgePage::chunkText(const QString &text, int chunkSize, int overlap) const
{
    if (text.trimmed().isEmpty())
        return {};
    QStringList chunks;
    const QString normalized = text.simplified();
    if (normalized.size() <= chunkSize) {
        chunks.append(normalized);
        return chunks;
    }
    int start = 0;
    while (start < normalized.size()) {
        int end = qMin(start + chunkSize, normalized.size());
        if (end < normalized.size()) {
            const int breakPoint = normalized.lastIndexOf(' ', end);
            if (breakPoint > start + chunkSize / 2)
                end = breakPoint;
        }
        chunks.append(normalized.mid(start, end - start).trimmed());
        if (end >= normalized.size())
            break;
        start = qMax(start + 1, end - overlap);
    }
    return chunks;
}

void KnowledgePage::deleteSelectedDocument()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;
    const qint64 id = m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
    m_store->deleteDocument(id);
    refreshDocuments();
}

void KnowledgePage::askQuestion()
{
    if (m_busy)
        return;
    const QString question = m_question->toPlainText().trimmed();
    if (question.isEmpty())
        return;
    if (m_store->documents().isEmpty()) {
        QMessageBox::information(this, tr("知识库"), tr("请先导入至少一个文档再提问。"));
        return;
    }
    m_lastQuestion = question;
    m_answer.clear();
    m_answerBrowser->clear();
    m_contextList->clear();
    setBusy(true);
    emit streamingStateChanged(true);

    SearchJob job;
    job.query = question;
    AppSettings *settings = AppSettings::instance();
    job.topK = settings->topK();
    job.useLocalEmbedding = settings->useLocalEmbedding();
    job.embeddingModel = settings->embeddingModel();
    job.embeddingBaseUrl = settings->embeddingBaseUrl();
    job.embeddingApiKey = settings->embeddingApiKey();
    job.timeoutMs = settings->timeoutMs();
    m_searchWatcher.setFuture(QtConcurrent::run([this, job]() { return runSearchJob(job); }));
}

QList<SearchHit> KnowledgePage::runSearchJob(const SearchJob &job)
{
    QVector<double> queryVector;
    if (job.useLocalEmbedding) {
        queryVector = LocalEmbedding::embed(job.query);
    } else {
        ApiClient client;
        client.setEmbeddingConfig(job.embeddingBaseUrl, job.embeddingApiKey, job.timeoutMs);
        QString embeddingError;
        queryVector = client.embedding(job.query, job.embeddingModel, &embeddingError);
        if (queryVector.isEmpty())
            queryVector = LocalEmbedding::embed(job.query);
    }

    KnowledgeStore workerStore;
    QString storeError;
    if (!workerStore.init(&storeError))
        return {};
    return workerStore.search(queryVector, job.topK, &storeError);
}

void KnowledgePage::startRagAnswer(const QString &question, const QList<SearchHit> &hits)
{
    QJsonArray messages;
    QJsonObject system;
    QString context;
    if (hits.isEmpty()) {
        context = QStringLiteral("未检索到相关上下文。");
    } else {
        for (int i = 0; i < hits.size(); ++i) {
            const SearchHit &hit = hits.at(i);
            context += QStringLiteral("\n\n[%1] %2 (chunk %3):\n%4")
                           .arg(i + 1)
                           .arg(hit.documentTitle)
                           .arg(hit.chunkIndex + 1)
                           .arg(hit.text);
        }
    }
    system.insert(QStringLiteral("role"), QStringLiteral("system"));
    system.insert(QStringLiteral("content"),
                  QStringLiteral("你是私有知识库助手。请基于检索到的上下文回答用户问题。"
                                 "如果上下文不足，请如实说明，并尽可能标注文档名称和分块编号。\n\n上下文：%1")
                      .arg(context));
    messages.append(system);
    QJsonObject user;
    user.insert(QStringLiteral("role"), QStringLiteral("user"));
    user.insert(QStringLiteral("content"), question);
    messages.append(user);

    m_api->configureFromSettings();
    m_api->startChatStream(messages, AppSettings::instance()->chatModel(), QJsonArray(), false);
}

void KnowledgePage::setBusy(bool busy)
{
    m_busy = busy;
    m_importButton->setEnabled(!busy);
    m_deleteButton->setEnabled(!busy);
    m_question->setEnabled(!busy);
    m_askButton->setEnabled(!busy && !m_question->toPlainText().trimmed().isEmpty());
    m_statusLabel->setText(busy ? tr("处理中…") : tr("文档仅保存在本地，并通过向量化用于私有知识检索。"));
}
