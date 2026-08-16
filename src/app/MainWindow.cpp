#include "MainWindow.h"

#include "AgentPage.h"
#include "ChatPage.h"
#include "KnowledgePage.h"
#include "SettingsDialog.h"
#include "VisionPage.h"
#include "core/AppSettings.h"

#include <QApplication>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QDialog>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Smart AI"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.svg")));
    resize(1280, 820);
    setMinimumSize(1000, 680);

    QFile styleFile(QStringLiteral(":/app.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
        qApp->setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    QString historyError;
    if (!m_history.init(&historyError))
        QMessageBox::warning(this, tr("Smart AI"), historyError);

    setupUi();
    statusBar()->showMessage(tr("Ready"));
    switchPage(0);
}

void MainWindow::setupUi()
{
    QWidget *root = new QWidget(this);
    root->setObjectName(QStringLiteral("RootWidget"));
    setCentralWidget(root);

    QHBoxLayout *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QFrame *sidebar = new QFrame(root);
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(220);
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(16, 22, 16, 18);
    sidebarLayout->setSpacing(8);

    QLabel *brand = new QLabel(tr("Smart AI"), sidebar);
    brand->setObjectName(QStringLiteral("BrandTitle"));
    sidebarLayout->addWidget(brand);
    QLabel *subtitle = new QLabel(tr("Multimodal desktop assistant"), sidebar);
    subtitle->setObjectName(QStringLiteral("BrandSubtitle"));
    sidebarLayout->addWidget(subtitle);
    sidebarLayout->addSpacing(20);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);
    sidebarLayout->addWidget(createNavButton(tr("💬  Chat"), 0));
    sidebarLayout->addWidget(createNavButton(tr("🖼️  Vision"), 1));
    sidebarLayout->addWidget(createNavButton(tr("📚  Knowledge"), 2));
    sidebarLayout->addWidget(createNavButton(tr("🤖  Agent"), 3));
    sidebarLayout->addStretch();

    QPushButton *settingsButton = new QPushButton(tr("⚙  Settings"), sidebar);
    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::openSettings);
    sidebarLayout->addWidget(settingsButton);

    QWidget *main = new QWidget(root);
    QVBoxLayout *mainLayout = new QVBoxLayout(main);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QFrame *topBar = new QFrame(main);
    topBar->setObjectName(QStringLiteral("TopBar"));
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(20, 12, 20, 12);
    m_pageTitle = new QLabel(topBar);
    m_pageTitle->setObjectName(QStringLiteral("PageTitle"));
    topLayout->addWidget(m_pageTitle);
    topLayout->addStretch();
    m_modelLabel = new QLabel(topBar);
    m_modelLabel->setObjectName(QStringLiteral("MutedLabel"));
    topLayout->addWidget(m_modelLabel);
    mainLayout->addWidget(topBar);

    m_stack = new QStackedWidget(main);
    m_chatPage = new ChatPage(&m_history, m_stack);
    m_visionPage = new VisionPage(m_stack);
    m_knowledgePage = new KnowledgePage(m_stack);
    m_agentPage = new AgentPage(m_stack);
    m_stack->addWidget(m_chatPage);
    m_stack->addWidget(m_visionPage);
    m_stack->addWidget(m_knowledgePage);
    m_stack->addWidget(m_agentPage);
    mainLayout->addWidget(m_stack, 1);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(main, 1);

    connect(m_navGroup, &QButtonGroup::idClicked, this, &MainWindow::switchPage);
    connect(m_chatPage, &ChatPage::conversationTitleChanged, m_pageTitle, &QLabel::setText);
    auto disableNav = [this](bool streaming) {
        const QList<QAbstractButton *> buttons = m_navGroup->buttons();
        for (QAbstractButton *button : buttons)
            button->setEnabled(!streaming);
    };
    connect(m_chatPage, &ChatPage::streamingStateChanged, this, disableNav);
    connect(m_visionPage, &VisionPage::streamingStateChanged, this, disableNav);
    connect(m_knowledgePage, &KnowledgePage::streamingStateChanged, this, disableNav);
    connect(m_agentPage, &AgentPage::streamingStateChanged, this, disableNav);

    updateModelLabel();
}

QPushButton *MainWindow::createNavButton(const QString &text, int id)
{
    QPushButton *button = new QPushButton(text);
    button->setObjectName(QStringLiteral("NavButton"));
    button->setCheckable(true);
    m_navGroup->addButton(button, id);
    return button;
}

void MainWindow::switchPage(int index)
{
    if (index < 0 || index >= m_stack->count())
        return;
    m_stack->setCurrentIndex(index);
    const QStringList titles = { tr("Chat"), tr("Image Understanding"), tr("Knowledge Base"), tr("Agent Tools") };
    if (index < titles.size())
        m_pageTitle->setText(titles.at(index));
    if (QAbstractButton *button = m_navGroup->button(index))
        button->setChecked(true);
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
        updateModelLabel();
}

void MainWindow::updateModelLabel()
{
    AppSettings *settings = AppSettings::instance();
    m_modelLabel->setText(QStringLiteral("%1 · %2").arg(settings->providerPreset(), settings->chatModel()));
}
