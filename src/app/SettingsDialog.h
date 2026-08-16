#pragma once

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QLineEdit;
class QSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void applyPreset();
    void accept() override;

private:
    QComboBox *m_preset = nullptr;
    QLineEdit *m_baseUrl = nullptr;
    QLineEdit *m_apiKey = nullptr;
    QLineEdit *m_chatModel = nullptr;
    QLineEdit *m_visionModel = nullptr;
    QDoubleSpinBox *m_temperature = nullptr;
    QSpinBox *m_maxTokens = nullptr;
    QSpinBox *m_timeout = nullptr;
    QLineEdit *m_embeddingBaseUrl = nullptr;
    QLineEdit *m_embeddingApiKey = nullptr;
    QLineEdit *m_embeddingModel = nullptr;
    QCheckBox *m_useLocalEmbedding = nullptr;
    QSpinBox *m_topK = nullptr;
    QSpinBox *m_chunkSize = nullptr;
    QSpinBox *m_chunkOverlap = nullptr;
};

