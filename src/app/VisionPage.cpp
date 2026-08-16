#include "VisionPage.h"

#include "core/ApiClient.h"
#include "core/AppSettings.h"

#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMimeData>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QVBoxLayout>

VisionPage::VisionPage(QWidget *parent)
    : QWidget(parent)
    , m_api(new ApiClient(this))
{
    m_api->configureFromSettings();
    setupUi();

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
        m_answer = message.isEmpty() ? tr("Vision request failed.") : message;
        m_answerBrowser->setMarkdown(m_answer);
        setBusy(false);
        emit streamingStateChanged(false);
    });
}

void VisionPage::setupUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(12);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QFrame *imagePanel = new QFrame(splitter);
    imagePanel->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *imageLayout = new QVBoxLayout(imagePanel);
    imageLayout->setContentsMargins(12, 12, 12, 12);
    QLabel *imageTitle = new QLabel(tr("Image input"), imagePanel);
    imageTitle->setObjectName(QStringLiteral("CardTitle"));
    imageLayout->addWidget(imageTitle);

    m_imageLabel = new QLabel(tr("Drag an image here or click Upload"), imagePanel);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(300, 280);
    m_imageLabel->setStyleSheet(QStringLiteral("border:1px dashed #40577f;border-radius:12px;color:#8290ad;padding:24px;"));
    imageLayout->addWidget(m_imageLabel, 1);

    m_fileLabel = new QLabel(imagePanel);
    m_fileLabel->setObjectName(QStringLiteral("MutedLabel"));
    m_fileLabel->setWordWrap(true);
    imageLayout->addWidget(m_fileLabel);

    QHBoxLayout *imageButtons = new QHBoxLayout;
    m_uploadButton = new QPushButton(tr("Upload image"), imagePanel);
    m_clearButton = new QPushButton(tr("Clear"), imagePanel);
    m_clearButton->setObjectName(QStringLiteral("DangerButton"));
    imageButtons->addWidget(m_uploadButton);
    imageButtons->addWidget(m_clearButton);
    imageLayout->addLayout(imageButtons);

    QFrame *chatPanel = new QFrame(splitter);
    chatPanel->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *chatLayout = new QVBoxLayout(chatPanel);
    chatLayout->setContentsMargins(12, 12, 12, 12);
    QLabel *chatTitle = new QLabel(tr("Visual understanding"), chatPanel);
    chatTitle->setObjectName(QStringLiteral("CardTitle"));
    chatLayout->addWidget(chatTitle);

    m_answerBrowser = new QTextBrowser(chatPanel);
    m_answerBrowser->setOpenExternalLinks(true);
    m_answerBrowser->setPlaceholderText(tr("The model's description or answer will appear here."));
    m_answerBrowser->document()->setDefaultStyleSheet(QStringLiteral(
        "body { color:#e8edf7; } code { color:#9fc1ff; } pre { background:#0c1320; border-radius:6px; padding:8px; }"));
    chatLayout->addWidget(m_answerBrowser, 1);

    QHBoxLayout *promptRow = new QHBoxLayout;
    m_promptCombo = new QComboBox(chatPanel);
    m_promptCombo->addItem(tr("Quick prompt"));
    m_promptCombo->addItem(tr("Describe this image in detail."));
    m_promptCombo->addItem(tr("Extract all visible text (OCR)."));
    m_promptCombo->addItem(tr("Detect and list the main objects."));
    m_promptCombo->addItem(tr("Explain the chart or diagram if present."));
    promptRow->addWidget(m_promptCombo);
    chatLayout->addLayout(promptRow);

    QHBoxLayout *questionRow = new QHBoxLayout;
    m_question = new QPlainTextEdit(chatPanel);
    m_question->setPlaceholderText(tr("Ask a question about the image…"));
    m_question->setMinimumHeight(64);
    m_sendButton = new QPushButton(tr("Ask"), chatPanel);
    m_sendButton->setObjectName(QStringLiteral("PrimaryButton"));
    m_sendButton->setMinimumWidth(90);
    questionRow->addWidget(m_question, 1);
    questionRow->addWidget(m_sendButton);
    chatLayout->addLayout(questionRow);

    splitter->addWidget(imagePanel);
    splitter->addWidget(chatPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    root->addWidget(splitter);

    connect(m_uploadButton, &QPushButton::clicked, this, &VisionPage::uploadImage);
    connect(m_clearButton, &QPushButton::clicked, this, &VisionPage::clearImage);
    connect(m_sendButton, &QPushButton::clicked, this, &VisionPage::sendQuestion);
    connect(m_promptCombo, &QComboBox::currentIndexChanged, this, &VisionPage::applyQuickPrompt);
}

void VisionPage::uploadImage()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Select image"), QString(),
                                                      tr("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
    if (!path.isEmpty())
        setImagePath(path);
}

void VisionPage::setImagePath(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    m_imageBase64 = QString::fromLatin1(file.readAll().toBase64());
    m_imagePath = path;

    QPixmap pixmap(path);
    if (!pixmap.isNull())
        m_imageLabel->setPixmap(pixmap.scaled(m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    else
        m_imageLabel->setText(tr("Unable to preview this image."));
    m_fileLabel->setText(QFileInfo(path).fileName());
    m_sendButton->setEnabled(!m_question->toPlainText().trimmed().isEmpty());
}

void VisionPage::clearImage()
{
    m_imageBase64.clear();
    m_imagePath.clear();
    m_imageLabel->setText(tr("Drag an image here or click Upload"));
    m_imageLabel->setPixmap(QPixmap());
    m_fileLabel->clear();
    m_sendButton->setEnabled(false);
}

void VisionPage::sendQuestion()
{
    if (m_busy || m_imageBase64.isEmpty())
        return;
    const QString question = m_question->toPlainText().trimmed();
    if (question.isEmpty())
        return;
    m_answer.clear();
    m_answerBrowser->clear();
    setBusy(true);
    emit streamingStateChanged(true);
    m_api->configureFromSettings();
    m_api->startChatStream(buildMessages(question), AppSettings::instance()->visionModel(), QJsonArray(), true);
}

QJsonArray VisionPage::buildMessages(const QString &question) const
{
    QJsonArray messages;
    QJsonObject system;
    system.insert(QStringLiteral("role"), QStringLiteral("system"));
    system.insert(QStringLiteral("content"), QStringLiteral("You are a precise visual assistant. Answer based on the image."));
    messages.append(system);

    QJsonObject user;
    user.insert(QStringLiteral("role"), QStringLiteral("user"));
    QJsonArray content;
    QJsonObject textPart;
    textPart.insert(QStringLiteral("type"), QStringLiteral("text"));
    textPart.insert(QStringLiteral("text"), question);
    content.append(textPart);
    QJsonObject imagePart;
    imagePart.insert(QStringLiteral("type"), QStringLiteral("image_url"));
    QJsonObject imageUrl;
    imageUrl.insert(QStringLiteral("url"), QStringLiteral("data:image/png;base64,%1").arg(m_imageBase64));
    imagePart.insert(QStringLiteral("image_url"), imageUrl);
    content.append(imagePart);
    user.insert(QStringLiteral("content"), content);
    messages.append(user);
    return messages;
}

void VisionPage::applyQuickPrompt(int index)
{
    if (index <= 0)
        return;
    m_question->setPlainText(m_promptCombo->itemText(index));
    m_promptCombo->setCurrentIndex(0);
    m_sendButton->setEnabled(!m_imageBase64.isEmpty() && !m_question->toPlainText().trimmed().isEmpty());
}

void VisionPage::setBusy(bool busy)
{
    m_busy = busy;
    m_uploadButton->setEnabled(!busy);
    m_clearButton->setEnabled(!busy);
    m_question->setEnabled(!busy);
    m_sendButton->setEnabled(!busy && !m_imageBase64.isEmpty() && !m_question->toPlainText().trimmed().isEmpty());
}

void VisionPage::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void VisionPage::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    setImagePath(urls.first().toLocalFile());
    event->acceptProposedAction();
}

