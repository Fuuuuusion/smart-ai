#include "LocalEmbedding.h"

#include <QChar>
#include <QStringList>
#include <cmath>

namespace {
quint32 fnv1a(const QByteArray &bytes, quint32 hash = 2166136261u)
{
    for (const unsigned char byte : bytes) {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

void addFeature(QVector<double> &vector, const QString &feature, double weight)
{
    if (feature.isEmpty())
        return;
    const quint32 hash = fnv1a(feature.toUtf8());
    const int index = static_cast<int>(hash % vector.size());
    vector[index] += weight * ((hash & 1u) ? 1.0 : -1.0);
}

QString normalize(const QString &text)
{
    QString result;
    result.reserve(text.size());
    for (const QChar &ch : text) {
        if (ch.isLetterOrNumber())
            result.append(ch.toLower());
        else if (!result.isEmpty() && result.back() != ' ')
            result.append(' ');
    }
    return result.simplified();
}
}

QVector<double> LocalEmbedding::embed(const QString &text, int dimensions)
{
    QVector<double> vector(dimensions, 0.0);
    const QString normalized = normalize(text);
    const QStringList words = normalized.split(' ', Qt::SkipEmptyParts);

    for (const QString &word : words) {
        addFeature(vector, word, 1.0);
        if (word.size() >= 2) {
            for (int i = 0; i + 2 <= word.size(); ++i)
                addFeature(vector, word.mid(i, 2), 0.55);
        }
        if (word.size() >= 3) {
            for (int i = 0; i + 3 <= word.size(); ++i)
                addFeature(vector, word.mid(i, 3), 0.35);
        }
    }

    // Character n-grams give usable signal for Chinese, Japanese, and mixed text.
    QString compact = normalized;
    compact.remove(' ');
    for (int i = 0; i < compact.size(); ++i) {
        addFeature(vector, QString(compact.at(i)), 0.8);
        if (i + 1 < compact.size())
            addFeature(vector, compact.mid(i, 2), 0.6);
    }

    double norm = 0.0;
    for (double value : vector)
        norm += value * value;
    norm = std::sqrt(norm);
    if (norm > 1e-9) {
        for (double &value : vector)
            value /= norm;
    }
    return vector;
}

double LocalEmbedding::cosineSimilarity(const QVector<double> &a, const QVector<double> &b)
{
    if (a.size() != b.size() || a.isEmpty())
        return 0.0;
    double dot = 0.0;
    double normA = 0.0;
    double normB = 0.0;
    for (int i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    if (normA <= 1e-12 || normB <= 1e-12)
        return 0.0;
    return dot / (std::sqrt(normA) * std::sqrt(normB));
}
