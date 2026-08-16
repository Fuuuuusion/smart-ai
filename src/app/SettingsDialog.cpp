#include "SettingsDialog.h"

#include "core/AppSettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setMinimumWidth(560);

    AppSettings *settings = AppSettings::instance();
    QVBoxLayout *root = new QVBoxLayout(this);

    QGroupBox *providerGroup = new QGroupBox(tr("Model provider"), this);
    QFormLayout *providerForm = new QFormLayout(providerGroup);

    m_preset = new QComboBox(providerGroup);
    for (const AppSettings::ProviderPreset &preset : AppSettings::presets())
        m_preset->addItem(preset.name, preset.baseUrl);
    m_preset->setCurrentText(settings->providerPreset());
    providerForm->addRow(tr("Preset"), m_preset);

    m_baseUrl = new QLineEdit(settings->baseUrl(), providerGroup);
    providerForm->addRow(tr("Base URL"), m_baseUrl);

    m_apiKey = new QLineEdit(settings->apiKey(), providerGroup);
    m_apiKey->setEchoMode(QLineEdit::Password);
    providerForm->addRow(tr("API key"), m_apiKey);

    m_chatModel = new QLineEdit(settings->chatModel(), providerGroup);
    providerForm->addRow(tr("Chat model"), m_chatModel);

    m_visionModel = new QLineEdit(settings->visionModel(), providerGroup);
    providerForm->addRow(tr("Vision model"), m_visionModel);

    m_temperature = new QDoubleSpinBox(providerGroup);
    m_temperature->setRange(0.0, 2.0);
    m_temperature->setSingleStep(0.1);
    m_temperature->setValue(settings->temperature());
    providerForm->addRow(tr("Temperature"), m_temperature);

    m_maxTokens = new QSpinBox(providerGroup);
    m_maxTokens->setRange(128, 16384);
    m_maxTokens->setSingleStep(128);
    m_maxTokens->setValue(settings->maxTokens());
    providerForm->addRow(tr("Max tokens"), m_maxTokens);

    m_timeout = new QSpinBox(providerGroup);
    m_timeout->setRange(5000, 300000);
    m_timeout->setSingleStep(5000);
    m_timeout->setValue(settings->timeoutMs());
    providerForm->addRow(tr("Timeout (ms)"), m_timeout);

    root->addWidget(providerGroup);

    QGroupBox *embeddingGroup = new QGroupBox(tr("Knowledge embeddings"), this);
    QFormLayout *embeddingForm = new QFormLayout(embeddingGroup);

    m_useLocalEmbedding = new QCheckBox(tr("Use built-in local embedding (no API required)"), embeddingGroup);
    m_useLocalEmbedding->setChecked(settings->useLocalEmbedding());
    embeddingForm->addRow(QString(), m_useLocalEmbedding);

    m_embeddingBaseUrl = new QLineEdit(settings->embeddingBaseUrl(), embeddingGroup);
    embeddingForm->addRow(tr("Embedding URL"), m_embeddingBaseUrl);

    m_embeddingApiKey = new QLineEdit(settings->embeddingApiKey(), embeddingGroup);
    m_embeddingApiKey->setEchoMode(QLineEdit::Password);
    embeddingForm->addRow(tr("Embedding key"), m_embeddingApiKey);

    m_embeddingModel = new QLineEdit(settings->embeddingModel(), embeddingGroup);
    embeddingForm->addRow(tr("Embedding model"), m_embeddingModel);

    m_topK = new QSpinBox(embeddingGroup);
    m_topK->setRange(1, 20);
    m_topK->setValue(settings->topK());
    embeddingForm->addRow(tr("Top-K chunks"), m_topK);

    m_chunkSize = new QSpinBox(embeddingGroup);
    m_chunkSize->setRange(64, 4096);
    m_chunkSize->setValue(settings->chunkSize());
    embeddingForm->addRow(tr("Chunk size"), m_chunkSize);

    m_chunkOverlap = new QSpinBox(embeddingGroup);
    m_chunkOverlap->setRange(0, 1024);
    m_chunkOverlap->setValue(settings->chunkOverlap());
    embeddingForm->addRow(tr("Chunk overlap"), m_chunkOverlap);

    root->addWidget(embeddingGroup);

    QLabel *hint = new QLabel(tr("API keys are stored locally. You can also set SMART_AI_API_KEY, "
                                 "SMART_AI_BASE_URL, and SMART_AI_MODEL environment variables."), this);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("MutedLabel"));
    root->addWidget(hint);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("PrimaryButton"));
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    root->addWidget(buttons);

    connect(m_preset, &QComboBox::currentIndexChanged, this, &SettingsDialog::applyPreset);
    applyPreset();
}

void SettingsDialog::applyPreset()
{
    const int index = m_preset->currentIndex();
    const QList<AppSettings::ProviderPreset> presets = AppSettings::presets();
    if (index < 0 || index >= presets.size())
        return;
    const AppSettings::ProviderPreset preset = presets.at(index);
    if (preset.name == QStringLiteral("Custom"))
        return;
    m_baseUrl->setText(preset.baseUrl);
    m_chatModel->setText(preset.chatModel);
    m_visionModel->setText(preset.visionModel);
    m_embeddingModel->setText(preset.embeddingModel);
    if (preset.local)
        m_apiKey->setText(QString());
}

void SettingsDialog::accept()
{
    AppSettings *settings = AppSettings::instance();
    settings->setProviderPreset(m_preset->currentText());
    settings->setBaseUrl(m_baseUrl->text());
    settings->setApiKey(m_apiKey->text());
    settings->setChatModel(m_chatModel->text());
    settings->setVisionModel(m_visionModel->text());
    settings->setTemperature(m_temperature->value());
    settings->setMaxTokens(m_maxTokens->value());
    settings->setTimeoutMs(m_timeout->value());
    settings->setUseLocalEmbedding(m_useLocalEmbedding->isChecked());
    settings->setEmbeddingBaseUrl(m_embeddingBaseUrl->text());
    settings->setEmbeddingApiKey(m_embeddingApiKey->text());
    settings->setEmbeddingModel(m_embeddingModel->text());
    settings->setTopK(m_topK->value());
    settings->setChunkSize(m_chunkSize->value());
    settings->setChunkOverlap(m_chunkOverlap->value());
    settings->save();
    QDialog::accept();
}
