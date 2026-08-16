#pragma once

#include <QByteArray>
#include <QString>

class PdfTextExtractor
{
public:
    QString extract(const QByteArray &pdfData, QString *error = nullptr) const;

private:
    QByteArray decodeStream(const QByteArray &stream, const QByteArray &dictionary, QString *error) const;
    QString extractTextCommands(const QByteArray &content) const;
    QString decodePdfString(const QByteArray &bytes) const;
};

