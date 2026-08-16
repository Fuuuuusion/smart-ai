#pragma once

#include <QWidget>
#include <QJsonArray>

class ApiClient;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;

class VisionPage : public QWidget
{
    Q_OBJECT
public:
    explicit VisionPage(QWidget *parent = nullptr);

signals:
    void streamingStateChanged(bool streaming);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void uploadImage();
    void clearImage();
    void sendQuestion();
    void applyQuickPrompt(int index);

private:
    void setupUi();
    void setImagePath(const QString &path);
    void setBusy(bool busy);
    QJsonArray buildMessages(const QString &question) const;

    ApiClient *m_api = nullptr;
    QString m_imageBase64;
    QString m_imagePath;
    bool m_busy = false;
    QString m_answer;

    QLabel *m_imageLabel = nullptr;
    QLabel *m_fileLabel = nullptr;
    QPushButton *m_uploadButton = nullptr;
    QPushButton *m_clearButton = nullptr;
    QPlainTextEdit *m_question = nullptr;
    QPushButton *m_sendButton = nullptr;
    QTextBrowser *m_answerBrowser = nullptr;
    QComboBox *m_promptCombo = nullptr;
};
