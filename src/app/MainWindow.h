#pragma once

#include <QMainWindow>

#include "core/ChatHistoryStore.h"

class AgentPage;
class ChatPage;
class KnowledgePage;
class QButtonGroup;
class QLabel;
class QPushButton;
class QStackedWidget;
class VisionPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void switchPage(int index);
    void openSettings();

private:
    void setupUi();
    QPushButton *createNavButton(const QString &text, int id);
    void updateModelLabel();

    ChatHistoryStore m_history;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_pageTitle = nullptr;
    QLabel *m_modelLabel = nullptr;
    QButtonGroup *m_navGroup = nullptr;
    ChatPage *m_chatPage = nullptr;
    VisionPage *m_visionPage = nullptr;
    KnowledgePage *m_knowledgePage = nullptr;
    AgentPage *m_agentPage = nullptr;
};

