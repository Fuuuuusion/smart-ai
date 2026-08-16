#pragma once

#include <QWidget>
#include <QFutureWatcher>

#include "core/AgentExecutor.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
class QTreeWidget;

class AgentPage : public QWidget
{
    Q_OBJECT
public:
    explicit AgentPage(QWidget *parent = nullptr);

signals:
    void streamingStateChanged(bool streaming);

private slots:
    void runAgent();

private:
    void setupUi();
    void setBusy(bool busy);

    bool m_busy = false;
    QPlainTextEdit *m_input = nullptr;
    QPushButton *m_runButton = nullptr;
    QTreeWidget *m_traceTree = nullptr;
    QTextBrowser *m_answerBrowser = nullptr;
    QLabel *m_statusLabel = nullptr;
    QFutureWatcher<AgentExecutionResult> m_watcher;
};

