#include "ChatPage.h"

#include "MessageWidget.h"
#include "core/ApiClient.h"
#include "core/AppSettings.h"
#include "core/ChatHistoryStore.h"

#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QStringList>

ChatPage::ChatPage(ChatHistoryStore *historyStore, QWidget *parent)
    : QWidget(parent)
    , m_history(historyStore)
    , m_api(new ApiClient(this))
{
    m_api->configureFromSettings();
    setupUi();

    connect(m_api, &ApiClient::streamDelta, this, [this](const QString &delta) {
        if (!m_streamingWidget)
            return;
        m_streamingContent += delta;
        m_streamingWidget->appendContent(delta);
        QScrollBar *bar = m_scroll->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
    connect(m_api, &ApiClient::streamFinished, this, [this](const QString &reason) {
        Q_UNUSED(reason)
        finishStreaming(true, m_streamingContent);
    });
    connect(m_api, &ApiClient::streamError, this, [this](const QString &message) {
        finishStreaming(false, message);
    });

    refreshConversations();
}

void ChatPage::setupUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    QHBoxLayout *toolbar = new QHBoxLayout;
    m_newButton = new QPushButton(tr("＋ 新建对话"), this);
    m_deleteButton = new QPushButton(tr("删除"), this);
    m_deleteButton->setObjectName(QStringLiteral("DangerButton"));
    m_modelCombo = new QComboBox(this);
    m_modelCombo->setEditable(true);
    m_modelCombo->addItems(QStringList{ AppSettings::instance()->chatModel(),
                                        QStringLiteral("qwen-plus"),
                                        QStringLiteral("deepseek-chat"),
                                        QStringLiteral("gpt-4o-mini"),
                                        QStringLiteral("qwen2.5:7b") });
    m_modelCombo->setCurrentText(AppSettings::instance()->chatModel());
    toolbar->addWidget(m_newButton);
    toolbar->addWidget(m_deleteButton);
    toolbar->addStretch();
    toolbar->addWidget(new QLabel(tr("模型"), this));
    toolbar->addWidget(m_modelCombo);
    root->addLayout(toolbar);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QFrame *historyPanel = new QFrame(splitter);
    historyPanel->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *historyLayout = new QVBoxLayout(historyPanel);
    historyLayout->setContentsMargins(8, 8, 8, 8);
    QLabel *historyTitle = new QLabel(tr("会话列表"), historyPanel);
    historyTitle->setObjectName(QStringLiteral("CardTitle"));
    historyLayout->addWidget(historyTitle);
    m_conversationList = new QListWidget(historyPanel);
    historyLayout->addWidget(m_conversationList);
    historyPanel->setMinimumWidth(210);
    historyPanel->setMaximumWidth(300);

    m_messagesContainer = new QWidget(this);
    m_messagesLayout = new QVBoxLayout(m_messagesContainer);
    m_messagesLayout->setContentsMargins(0, 0, 0, 0);
    m_messagesLayout->setSpacing(4);
    m_messagesLayout->addStretch();

    m_scroll = new QScrollArea(splitter);
    m_scroll->setWidgetResizable(true);
    m_scroll->setWidget(m_messagesContainer);

    splitter->addWidget(historyPanel);
    splitter->addWidget(m_scroll);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    QHBoxLayout *inputRow = new QHBoxLayout;
    m_input = new QPlainTextEdit(this);
    m_input->setPlaceholderText(tr("输入问题…（可将图片拖到此处进行视觉问答）"));
    m_input->setMinimumHeight(72);
    m_input->setMaximumHeight(150);
    m_input->setAcceptDrops(false);
    m_sendButton = new QPushButton(tr("发送"), this);
    m_sendButton->setObjectName(QStringLiteral("PrimaryButton"));
    m_sendButton->setMinimumWidth(92);
    inputRow->addWidget(m_input, 1);
    QVBoxLayout *sendColumn = new QVBoxLayout;
    m_attachmentLabel = new QLabel(this);
    m_attachmentLabel->setVisible(false);
    m_attachmentLabel->setObjectName(QStringLiteral("ToolBadge"));
    sendColumn->addWidget(m_attachmentLabel);
    sendColumn->addWidget(m_sendButton);
    inputRow->addLayout(sendColumn);
    root->addLayout(inputRow);

    connect(m_newButton, &QPushButton::clicked, this, &ChatPage::newConversation);
    connect(m_deleteButton, &QPushButton::clicked, this, &ChatPage::deleteConversation);
    connect(m_conversationList, &QListWidget::currentRowChanged, this, &ChatPage::selectConversation);
    connect(m_sendButton, &QPushButton::clicked, this, &ChatPage::sendMessage);
    connect(m_input, &QPlainTextEdit::textChanged, this, [this]() {
        m_sendButton->setEnabled(!m_input->toPlainText().trimmed().isEmpty() && !m_streaming);
    });
    connect(m_attachmentLabel, &QLabel::linkActivated, this, &ChatPage::clearAttachment);
    m_sendButton->setEnabled(false);
}

void ChatPage::refreshConversations()
{
    m_conversationList->blockSignals(true);
    m_conversationList->clear();
    const QList<ConversationRecord> conversations = m_history->conversations();
    int selectedRow = -1;
    for (int i = 0; i < conversations.size(); ++i) {
        const ConversationRecord &conversation = conversations.at(i);
        QListWidgetItem *item = new QListWidgetItem(conversation.title, m_conversationList);
        item->setData(Qt::UserRole, conversation.id);
        if (conversation.id == m_history->lastConversationId())
            selectedRow = i;
    }
    if (selectedRow < 0 && m_conversationList->count() > 0)
        selectedRow = 0;
    m_conversationList->blockSignals(false);
    if (selectedRow >= 0)
        m_conversationList->setCurrentRow(selectedRow);
    else if (m_conversationList->count() > 0)
        m_conversationList->setCurrentRow(0);
}

void ChatPage::selectConversation()
{
    QListWidgetItem *item = m_conversationList->currentItem();
    if (!item)
        return;
    m_conversationId = item->data(Qt::UserRole).toLongLong();
    m_history->setLastConversationId(m_conversationId);
    loadMessages(m_conversationId);
    const QList<ConversationRecord> conversations = m_history->conversations();
    for (const ConversationRecord &conversation : conversations) {
        if (conversation.id == m_conversationId)
            emit conversationTitleChanged(conversation.title);
    }
}

void ChatPage::newConversation()
{
    QString error;
    const qint64 id = m_history->createConversation(QStringLiteral("新对话"), &error);
    if (id < 0)
        return;
    refreshConversations();
    for (int i = 0; i < m_conversationList->count(); ++i) {
        if (m_conversationList->item(i)->data(Qt::UserRole).toLongLong() == id) {
            m_conversationList->setCurrentRow(i);
            break;
        }
    }
    m_input->setFocus();
}

void ChatPage::deleteConversation()
{
    if (m_conversationId <= 0)
        return;
    m_history->deleteConversation(m_conversationId);
    m_conversationId = -1;
    refreshConversations();
}

void ChatPage::loadMessages(qint64 conversationId)
{
    while (m_messagesLayout->count() > 1) {
        QLayoutItem *item = m_messagesLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    m_streamingWidget = nullptr;
    m_streamingContent.clear();

    const QList<ChatMessageRecord> messages = m_history->messages(conversationId);
    for (const ChatMessageRecord &message : messages)
        addMessageWidget(message.role, message.content, message.imageBase64, false);
    QScrollBar *bar = m_scroll->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void ChatPage::addMessageWidget(const QString &role,
                                const QString &content,
                                const QString &imageBase64,
                                bool streaming)
{
    MessageWidget *widget = new MessageWidget(role, m_messagesContainer);
    widget->setContent(content);
    widget->setImageBase64(imageBase64);
    widget->setStreaming(streaming);
    appendToScroll(widget);
}

void ChatPage::appendToScroll(MessageWidget *widget)
{
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, widget);
    QScrollBar *bar = m_scroll->verticalScrollBar();
    QTimer::singleShot(0, this, [bar]() { bar->setValue(bar->maximum()); });
}

QJsonArray ChatPage::buildMessages() const
{
    QJsonArray messages;
    QJsonObject system;
    system.insert(QStringLiteral("role"), QStringLiteral("system"));
    system.insert(QStringLiteral("content"),
                  QStringLiteral("你是 Smart AI，一个乐于助人的桌面智能助手。"
                                 "请清晰回答用户问题，并在合适时使用 Markdown 提高可读性。"));
    messages.append(system);

    const QList<ChatMessageRecord> records = m_history->messages(m_conversationId);
    for (const ChatMessageRecord &record : records) {
        QJsonObject message;
        message.insert(QStringLiteral("role"), record.role);
        if (record.role == QStringLiteral("user") && !record.imageBase64.isEmpty()) {
            QJsonArray content;
            QJsonObject textPart;
            textPart.insert(QStringLiteral("type"), QStringLiteral("text"));
            textPart.insert(QStringLiteral("text"), record.content);
            content.append(textPart);
            QJsonObject imagePart;
            imagePart.insert(QStringLiteral("type"), QStringLiteral("image_url"));
            QJsonObject imageUrl;
            imageUrl.insert(QStringLiteral("url"),
                            QStringLiteral("data:image/png;base64,%1").arg(record.imageBase64));
            imagePart.insert(QStringLiteral("image_url"), imageUrl);
            content.append(imagePart);
            message.insert(QStringLiteral("content"), content);
        } else {
            message.insert(QStringLiteral("content"), record.content);
        }
        messages.append(message);
    }
    return messages;
}

void ChatPage::sendMessage()
{
    if (m_streaming)
        return;
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    if (m_conversationId <= 0) {
        QString error;
        m_conversationId = m_history->createConversation(QStringLiteral("新对话"), &error);
        if (m_conversationId <= 0)
            return;
        refreshConversations();
        for (int i = 0; i < m_conversationList->count(); ++i) {
            if (m_conversationList->item(i)->data(Qt::UserRole).toLongLong() == m_conversationId) {
                m_conversationList->blockSignals(true);
                m_conversationList->setCurrentRow(i);
                m_conversationList->blockSignals(false);
                break;
            }
        }
    }

    const QString attached = m_attachedImageBase64;
    m_history->addMessage(m_conversationId, QStringLiteral("user"), text, attached);
    m_history->autoTitleConversation(m_conversationId, text);

    addMessageWidget(QStringLiteral("user"), text, attached, false);
    m_input->clear();
    clearAttachment();
    m_sendButton->setEnabled(false);

    const QString model = attached.isEmpty() ? m_modelCombo->currentText().trimmed()
                                             : AppSettings::instance()->visionModel();
    m_streamingWidget = new MessageWidget(QStringLiteral("assistant"), m_messagesContainer);
    m_streamingWidget->setContent(QString());
    m_streamingWidget->setStreaming(true);
    appendToScroll(m_streamingWidget);
    m_streamingContent.clear();
    setBusy(true);
    emit streamingStateChanged(true);

    m_api->configureFromSettings();
    m_api->startChatStream(buildMessages(), model, QJsonArray(), !attached.isEmpty());
}

void ChatPage::finishStreaming(bool success, const QString &content)
{
    if (!m_streamingWidget)
        return;
    const QString finalText = success ? content : (content.isEmpty() ? tr("请求失败。") : content);
    m_streamingWidget->setContent(finalText);
    m_streamingWidget->setStreaming(false);
    if (success)
        m_history->addMessage(m_conversationId, QStringLiteral("assistant"), finalText);
    m_streamingWidget = nullptr;
    m_streamingContent.clear();
    setBusy(false);
    emit streamingStateChanged(false);
    refreshConversations();
    m_sendButton->setEnabled(!m_input->toPlainText().trimmed().isEmpty());
}

void ChatPage::clearAttachment()
{
    m_attachedImageBase64.clear();
    m_attachmentLabel->clear();
    m_attachmentLabel->setVisible(false);
}

void ChatPage::setBusy(bool busy)
{
    m_streaming = busy;
    m_newButton->setEnabled(!busy);
    m_deleteButton->setEnabled(!busy);
    m_modelCombo->setEnabled(!busy);
    m_input->setEnabled(!busy);
    m_sendButton->setEnabled(!busy && !m_input->toPlainText().trimmed().isEmpty());
}

QString ChatPage::loadImageAsBase64(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromLatin1(file.readAll().toBase64());
}

void ChatPage::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
            if (suffix == QStringLiteral("png") || suffix == QStringLiteral("jpg") ||
                suffix == QStringLiteral("jpeg") || suffix == QStringLiteral("bmp") ||
                suffix == QStringLiteral("webp")) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void ChatPage::dropEvent(QDropEvent *event)
{
    for (const QUrl &url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix == QStringLiteral("png") || suffix == QStringLiteral("jpg") ||
            suffix == QStringLiteral("jpeg") || suffix == QStringLiteral("bmp") ||
            suffix == QStringLiteral("webp")) {
            m_attachedImageBase64 = loadImageAsBase64(path);
            m_attachmentLabel->setText(tr("📎 已添加 %1 · <a href=\"clear\">移除</a>").arg(QFileInfo(path).fileName()));
            m_attachmentLabel->setVisible(true);
            event->acceptProposedAction();
            return;
        }
    }
}
