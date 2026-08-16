#pragma once

#include <QWidget>
#include <QList>
#include <QFutureWatcher>
#include <QStringList>

#include "core/KnowledgeStore.h"

class ApiClient;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTextBrowser;

class KnowledgePage : public QWidget
{
    Q_OBJECT
public:
    explicit KnowledgePage(QWidget *parent = nullptr);

signals:
    void streamingStateChanged(bool streaming);

private slots:
    void importDocuments();
    void deleteSelectedDocument();
    void refreshDocuments();
    void askQuestion();

private:
    struct ImportJob
    {
        QStringList paths;
        int chunkSize = 512;
        int chunkOverlap = 50;
        bool useLocalEmbedding = false;
        QString embeddingModel;
        QString embeddingBaseUrl;
        QString embeddingApiKey;
        int timeoutMs = 60000;
    };

    struct SearchJob
    {
        QString query;
        int topK = 4;
        bool useLocalEmbedding = false;
        QString embeddingModel;
        QString embeddingBaseUrl;
        QString embeddingApiKey;
        int timeoutMs = 60000;
    };

    void setupUi();
    QStringList chunkText(const QString &text, int chunkSize, int overlap) const;
    QString runImportJob(const ImportJob &job);
    QList<SearchHit> runSearchJob(const SearchJob &job);
    void setBusy(bool busy);
    QList<SearchHit> retrieveContext(const QString &question);
    void startRagAnswer(const QString &question, const QList<SearchHit> &hits);

    KnowledgeStore *m_store = nullptr;
    ApiClient *m_api = nullptr;
    bool m_busy = false;
    QString m_answer;
    QString m_lastQuestion;

    QTableWidget *m_table = nullptr;
    QListWidget *m_contextList = nullptr;
    QPlainTextEdit *m_question = nullptr;
    QTextBrowser *m_answerBrowser = nullptr;
    QPushButton *m_importButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_askButton = nullptr;
    QLabel *m_statusLabel = nullptr;

    QFutureWatcher<QString> m_importWatcher;
    QFutureWatcher<QList<SearchHit>> m_searchWatcher;
};
