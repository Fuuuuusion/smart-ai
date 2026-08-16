#include "AgentPage.h"

#include "core/AppSettings.h"
#include "core/ToolRegistry.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {
QString toolDisplayName(const QString &name)
{
    if (name == QStringLiteral("calculator"))
        return QStringLiteral("计算器");
    if (name == QStringLiteral("weather"))
        return QStringLiteral("天气查询");
    if (name == QStringLiteral("web_search"))
        return QStringLiteral("网络搜索");
    if (name == QStringLiteral("knowledge_search"))
        return QStringLiteral("知识库检索");
    if (name == QStringLiteral("current_time"))
        return QStringLiteral("当前时间");
    return name;
}
}

AgentPage::AgentPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    connect(&m_watcher, &QFutureWatcher<AgentExecutionResult>::finished, this, [this]() {
        const AgentExecutionResult result = m_watcher.result();
        m_traceTree->clear();
        for (const AgentTraceStep &step : result.trace) {
            QTreeWidgetItem *item = new QTreeWidgetItem(m_traceTree);
            item->setText(0, step.label);
            item->setText(1, step.detail);
            item->setToolTip(1, step.detail);
        }
        m_traceTree->expandAll();
        m_answerBrowser->setMarkdown(result.ok ? result.finalAnswer
                                               : QStringLiteral("**智能体错误：** %1").arg(result.error));
        m_statusLabel->setText(result.ok ? tr("完成") : tr("失败"));
        setBusy(false);
        emit streamingStateChanged(false);
    });
}

void AgentPage::setupUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    QHBoxLayout *toolbar = new QHBoxLayout;
    QLabel *title = new QLabel(tr("智能体工具"), this);
    title->setObjectName(QStringLiteral("CardTitle"));
    m_statusLabel = new QLabel(tr("空闲"), this);
    m_statusLabel->setObjectName(QStringLiteral("MutedLabel"));
    toolbar->addWidget(title);
    toolbar->addStretch();
    toolbar->addWidget(m_statusLabel);
    root->addLayout(toolbar);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QFrame *toolPanel = new QFrame(splitter);
    toolPanel->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *toolLayout = new QVBoxLayout(toolPanel);
    QLabel *toolTitle = new QLabel(tr("可用工具"), toolPanel);
    toolTitle->setObjectName(QStringLiteral("CardTitle"));
    toolLayout->addWidget(toolTitle);
    QTreeWidget *toolTree = new QTreeWidget(toolPanel);
    toolTree->setHeaderLabels({ tr("工具"), tr("说明") });
    toolTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    toolTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    const QJsonArray definitions = ToolRegistry::toolDefinitions();
    for (const QJsonValue &value : definitions) {
        const QJsonObject function = value.toObject().value(QStringLiteral("function")).toObject();
        QTreeWidgetItem *item = new QTreeWidgetItem(toolTree);
        const QString name = function.value(QStringLiteral("name")).toString();
        item->setText(0, toolDisplayName(name));
        item->setText(1, function.value(QStringLiteral("description")).toString());
    }
    toolTree->expandAll();
    toolLayout->addWidget(toolTree);

    QFrame *runPanel = new QFrame(splitter);
    runPanel->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *runLayout = new QVBoxLayout(runPanel);
    QLabel *traceTitle = new QLabel(tr("推理过程"), runPanel);
    traceTitle->setObjectName(QStringLiteral("CardTitle"));
    runLayout->addWidget(traceTitle);
    m_traceTree = new QTreeWidget(runPanel);
    m_traceTree->setHeaderLabels({ tr("步骤"), tr("详情") });
    m_traceTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_traceTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    runLayout->addWidget(m_traceTree, 1);

    QLabel *answerTitle = new QLabel(tr("最终回答"), runPanel);
    answerTitle->setObjectName(QStringLiteral("CardTitle"));
    runLayout->addWidget(answerTitle);
    m_answerBrowser = new QTextBrowser(runPanel);
    m_answerBrowser->setOpenExternalLinks(true);
    m_answerBrowser->setPlaceholderText(tr("智能体的最终回答将显示在这里。"));
    runLayout->addWidget(m_answerBrowser, 2);

    splitter->addWidget(toolPanel);
    splitter->addWidget(runPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    QHBoxLayout *inputRow = new QHBoxLayout;
    m_input = new QPlainTextEdit(this);
    m_input->setPlaceholderText(tr("描述一个多步骤任务，例如：查询上海天气，并计算 480 的 15% 是多少？"));
    m_input->setMinimumHeight(72);
    m_runButton = new QPushButton(tr("运行智能体"), this);
    m_runButton->setObjectName(QStringLiteral("PrimaryButton"));
    m_runButton->setMinimumWidth(110);
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_runButton);
    root->addLayout(inputRow);

    connect(m_runButton, &QPushButton::clicked, this, &AgentPage::runAgent);
    connect(m_input, &QPlainTextEdit::textChanged, this, [this]() {
        m_runButton->setEnabled(!m_input->toPlainText().trimmed().isEmpty() && !m_busy);
    });
    m_runButton->setEnabled(false);
}

void AgentPage::runAgent()
{
    if (m_busy)
        return;
    const QString input = m_input->toPlainText().trimmed();
    if (input.isEmpty())
        return;
    AgentConfig config;
    AppSettings *settings = AppSettings::instance();
    config.chatBaseUrl = settings->baseUrl();
    config.chatApiKey = settings->apiKey();
    config.chatModel = settings->chatModel();
    config.embeddingBaseUrl = settings->embeddingBaseUrl();
    config.embeddingApiKey = settings->embeddingApiKey();
    config.embeddingModel = settings->embeddingModel();
    config.temperature = settings->temperature();
    config.maxTokens = settings->maxTokens();
    config.timeoutMs = settings->timeoutMs();
    config.topK = settings->topK();
    config.useLocalEmbedding = settings->useLocalEmbedding();
    m_traceTree->clear();
    m_answerBrowser->clear();
    setBusy(true);
    emit streamingStateChanged(true);
    m_statusLabel->setText(tr("思考中…"));
    m_watcher.setFuture(QtConcurrent::run([input, config]() {
        return AgentExecutor::run(input, nullptr, config);
    }));
}

void AgentPage::setBusy(bool busy)
{
    m_busy = busy;
    m_input->setEnabled(!busy);
    m_runButton->setEnabled(!busy && !m_input->toPlainText().trimmed().isEmpty());
}
