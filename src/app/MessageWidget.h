#pragma once

#include <QWidget>

class QLabel;
class QTextBrowser;

class MessageWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MessageWidget(const QString &role, QWidget *parent = nullptr);

    void setContent(const QString &markdown);
    void appendContent(const QString &markdownDelta);
    void setImageBase64(const QString &base64);
    void setStreaming(bool streaming);
    QTextBrowser *contentBrowser() const;

private:
    QString m_role;
    QString m_markdown;
    QTextBrowser *m_browser = nullptr;
    QLabel *m_imageLabel = nullptr;
};

