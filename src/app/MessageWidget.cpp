#include "MessageWidget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QTextBrowser>
#include <QVBoxLayout>

MessageWidget::MessageWidget(const QString &role, QWidget *parent)
    : QWidget(parent)
    , m_role(role)
{
    QHBoxLayout *outer = new QHBoxLayout(this);
    outer->setContentsMargins(12, 5, 12, 5);

    QFrame *bubble = new QFrame(this);
    bubble->setObjectName(role == QStringLiteral("user") ? QStringLiteral("UserBubble") : QStringLiteral("AssistantBubble"));
    bubble->setMaximumWidth(860);

    QVBoxLayout *layout = new QVBoxLayout(bubble);
    layout->setContentsMargins(14, 11, 14, 11);
    layout->setSpacing(6);

    m_imageLabel = new QLabel(bubble);
    m_imageLabel->setVisible(false);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_imageLabel);

    QLabel *author = new QLabel(role == QStringLiteral("user") ? tr("You") : tr("Smart AI"), bubble);
    author->setStyleSheet(role == QStringLiteral("user")
                              ? QStringLiteral("color:#d7e4ff;font-weight:600;")
                              : QStringLiteral("color:#9fc1ff;font-weight:600;"));
    layout->addWidget(author);

    m_browser = new QTextBrowser(bubble);
    m_browser->setObjectName(QStringLiteral("MessageContent"));
    m_browser->setOpenExternalLinks(true);
    m_browser->setFrameShape(QFrame::NoFrame);
    m_browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_browser->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_browser->document()->setDefaultStyleSheet(QStringLiteral(
        "body { color:#e8edf7; }"
        "code { color:#9fc1ff; background:#121a2b; }"
        "pre { background:#0c1320; border:1px solid #263049; border-radius:6px; padding:8px; }"
        "a { color:#8ab4ff; }"
        "table { border-collapse: collapse; }"
        "td, th { border:1px solid #334263; padding:4px; }"));
    layout->addWidget(m_browser);

    if (role == QStringLiteral("user")) {
        outer->addStretch();
        outer->addWidget(bubble);
    } else {
        outer->addWidget(bubble);
        outer->addStretch();
    }
}

void MessageWidget::setContent(const QString &markdown)
{
    m_markdown = markdown;
    m_browser->setMarkdown(m_markdown);
}

void MessageWidget::appendContent(const QString &markdownDelta)
{
    m_markdown += markdownDelta;
    m_browser->setMarkdown(m_markdown);
}

void MessageWidget::setImageBase64(const QString &base64)
{
    if (base64.isEmpty()) {
        m_imageLabel->clear();
        m_imageLabel->setVisible(false);
        return;
    }
    QPixmap pixmap;
    pixmap.loadFromData(QByteArray::fromBase64(base64.toLatin1()));
    if (pixmap.isNull()) {
        m_imageLabel->setText(tr("Image preview unavailable"));
        m_imageLabel->setVisible(true);
        return;
    }
    if (pixmap.width() > 360)
        pixmap = pixmap.scaledToWidth(360, Qt::SmoothTransformation);
    m_imageLabel->setPixmap(pixmap);
    m_imageLabel->setVisible(true);
}

void MessageWidget::setStreaming(bool streaming)
{
    if (streaming)
        m_browser->setMarkdown(m_markdown + QStringLiteral(" ▍"));
    else
        m_browser->setMarkdown(m_markdown);
}

QTextBrowser *MessageWidget::contentBrowser() const
{
    return m_browser;
}

