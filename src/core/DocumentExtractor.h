#pragma once

#include <QString>

struct ExtractedDocument
{
    QString title;
    QString format;
    QString text;
    int characterCount = 0;
};

class DocumentExtractor
{
public:
    static ExtractedDocument extract(const QString &filePath, QString *error = nullptr);
};

