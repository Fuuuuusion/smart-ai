#pragma once

#include <QWidget>
#include <QJsonArray>

class ApiClient;
class ChatHistoryStore;
class MessageWidget;
class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

class ChatPage : public QWidget
{
    Q_OBJECT
public:
    explicit ChatPage(ChatHistoryStore *historyStore, QWidget *parent = nullptr);

signals:
    void conversationTitleChanged(const QString &title);
    void streamingStateChanged(bool streaming);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void newConversation();
    void deleteConversation();
    void refreshConversations();
    void selectConversation();
    void sendMessage();
    void clearAttachment();

private:
    void setupUi();
    void loadMessages(qint64 conversationId);
    void addMessageWidget(const QString &role,
                          const QString &content,
                          const QString &imageBase64 = QString(),
                          bool streaming = false);
    QJsonArray buildMessages() const;
    QString loadImageAsBase64(const QString &path);
    void setBusy(bool busy);
    void appendToScroll(MessageWidget *widget);
    void finishStreaming(bool success, const QString &content);

    ChatHistoryStore *m_history = nullptr;
    ApiClient *m_api = nullptr;
    qint64 m_conversationId = -1;
    bool m_streaming = false;
    QString m_streamingContent;
    MessageWidget *m_streamingWidget = nullptr;
    QString m_attachedImageBase64;

    QComboBox *m_modelCombo = nullptr;
    QListWidget *m_conversationList = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_messagesContainer = nullptr;
    QVBoxLayout *m_messagesLayout = nullptr;
    QPlainTextEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
    QPushButton *m_newButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QLabel *m_attachmentLabel = nullptr;
};

