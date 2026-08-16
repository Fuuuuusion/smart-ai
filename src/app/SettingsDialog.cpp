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
    setWindowTitle(tr("设置"));
    setMinimumWidth(560);

    AppSettings *settings = AppSettings::instance();
    QVBoxLayout *root = new QVBoxLayout(this);

    QGroupBox *providerGroup = new QGroupBox(tr("模型服务"), this);
    QFormLayout *providerForm = new QFormLayout(providerGroup);

    m_preset = new QComboBox(providerGroup);
    for (const AppSettings::ProviderPreset &preset : AppSettings::presets())
        m_preset->addItem(preset.name, preset.baseUrl);
    int presetIndex = m_preset->findText(settings->providerPreset());
    if (presetIndex < 0) {
        const QString stored = settings->providerPreset();
        if (stored == QStringLiteral("Qwen / DashScope"))
            presetIndex = m_preset->findText(QStringLiteral("通义千问 / DashScope"));
        else if (stored == QStringLiteral("Ollama (local)"))
            presetIndex = m_preset->findText(QStringLiteral("Ollama（本地）"));
        else if (stored == QStringLiteral("Custom"))
            presetIndex = m_preset->findText(QStringLiteral("自定义"));
    }
    m_preset->setCurrentIndex(presetIndex < 0 ? 0 : presetIndex);
    providerForm->addRow(tr("预设方案"), m_preset);

    m_baseUrl = new QLineEdit(settings->baseUrl(), providerGroup);
    providerForm->addRow(tr("接口地址"), m_baseUrl);

    m_apiKey = new QLineEdit(settings->apiKey(), providerGroup);
    m_apiKey->setEchoMode(QLineEdit::Password);
    providerForm->addRow(tr("API 密钥"), m_apiKey);

    m_chatModel = new QLineEdit(settings->chatModel(), providerGroup);
    providerForm->addRow(tr("对话模型"), m_chatModel);

    m_visionModel = new QLineEdit(settings->visionModel(), providerGroup);
    providerForm->addRow(tr("视觉模型"), m_visionModel);

    m_temperature = new QDoubleSpinBox(providerGroup);
    m_temperature->setRange(0.0, 2.0);
    m_temperature->setSingleStep(0.1);
    m_temperature->setValue(settings->temperature());
    providerForm->addRow(tr("温度"), m_temperature);

    m_maxTokens = new QSpinBox(providerGroup);
    m_maxTokens->setRange(128, 16384);
    m_maxTokens->setSingleStep(128);
    m_maxTokens->setValue(settings->maxTokens());
    providerForm->addRow(tr("最大 Token 数"), m_maxTokens);

    m_timeout = new QSpinBox(providerGroup);
    m_timeout->setRange(5000, 300000);
    m_timeout->setSingleStep(5000);
    m_timeout->setValue(settings->timeoutMs());
    providerForm->addRow(tr("超时时间（毫秒）"), m_timeout);

    root->addWidget(providerGroup);

    QGroupBox *embeddingGroup = new QGroupBox(tr("知识库向量化"), this);
    QFormLayout *embeddingForm = new QFormLayout(embeddingGroup);

    m_useLocalEmbedding = new QCheckBox(tr("使用内置本地向量化（无需 API）"), embeddingGroup);
    m_useLocalEmbedding->setChecked(settings->useLocalEmbedding());
    embeddingForm->addRow(QString(), m_useLocalEmbedding);

    m_embeddingBaseUrl = new QLineEdit(settings->embeddingBaseUrl(), embeddingGroup);
    embeddingForm->addRow(tr("向量化接口地址"), m_embeddingBaseUrl);

    m_embeddingApiKey = new QLineEdit(settings->embeddingApiKey(), embeddingGroup);
    m_embeddingApiKey->setEchoMode(QLineEdit::Password);
    embeddingForm->addRow(tr("向量化密钥"), m_embeddingApiKey);

    m_embeddingModel = new QLineEdit(settings->embeddingModel(), embeddingGroup);
    embeddingForm->addRow(tr("向量化模型"), m_embeddingModel);

    m_topK = new QSpinBox(embeddingGroup);
    m_topK->setRange(1, 20);
    m_topK->setValue(settings->topK());
    embeddingForm->addRow(tr("Top-K 分块数"), m_topK);

    m_chunkSize = new QSpinBox(embeddingGroup);
    m_chunkSize->setRange(64, 4096);
    m_chunkSize->setValue(settings->chunkSize());
    embeddingForm->addRow(tr("分块大小"), m_chunkSize);

    m_chunkOverlap = new QSpinBox(embeddingGroup);
    m_chunkOverlap->setRange(0, 1024);
    m_chunkOverlap->setValue(settings->chunkOverlap());
    embeddingForm->addRow(tr("分块重叠"), m_chunkOverlap);

    root->addWidget(embeddingGroup);

    QLabel *hint = new QLabel(tr("API 密钥仅保存在本地。也可以通过 SMART_AI_API_KEY、"
                                 "SMART_AI_BASE_URL 和 SMART_AI_MODEL 环境变量配置。"), this);
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
    if (preset.name == QStringLiteral("自定义"))
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
