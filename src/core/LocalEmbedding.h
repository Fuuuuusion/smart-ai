#pragma once

#include <QString>
#include <QVector>

namespace LocalEmbedding
{
QVector<double> embed(const QString &text, int dimensions = 384);
double cosineSimilarity(const QVector<double> &a, const QVector<double> &b);
}

