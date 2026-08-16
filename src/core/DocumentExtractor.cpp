#include "DocumentExtractor.h"

#include "PdfTextExtractor.h"
#include "ZipReader.h"

#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
#include <QXmlStreamReader>

namespace {
QString decodeTextFile(const QByteArray &bytes)
{
    if (bytes.startsWith("\xEF\xBB\xBF"))
        return QString::fromUtf8(bytes.mid(3));
    if (bytes.startsWith("\xFF\xFE") || bytes.startsWith("\xFE\xFF")) {
        QStringDecoder decoder(bytes.startsWith("\xFF\xFE") ? QStringConverter::Utf16LE : QStringConverter::Utf16BE);
        return decoder.decode(bytes);
    }
    QStringDecoder utf8(QStringConverter::Utf8);
    QString result = utf8.decode(bytes);
    if (utf8.hasError())
        result = QString::fromLocal8Bit(bytes);
    return result;
}

QString docxText(const QByteArray &documentXml)
{
    QXmlStreamReader xml(documentXml);
    QString text;
    bool paragraphOpen = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == QStringLiteral("p"))
                paragraphOpen = true;
            else if (xml.name() == QStringLiteral("t"))
                text.append(xml.readElementText(QXmlStreamReader::IncludeChildElements));
            else if (xml.name() == QStringLiteral("tab"))
                text.append('\t');
            else if (xml.name() == QStringLiteral("br") || xml.name() == QStringLiteral("cr"))
                text.append('\n');
        } else if (xml.isEndElement()) {
            if (xml.name() == QStringLiteral("p")) {
                if (paragraphOpen)
                    text.append('\n');
                paragraphOpen = false;
            }
        }
    }
    return text;
}
}

ExtractedDocument DocumentExtractor::extract(const QString &filePath, QString *error)
{
    ExtractedDocument result;
    QFileInfo info(filePath);
    if (!info.exists()) {
        if (error)
            *error = QStringLiteral("File does not exist.");
        return result;
    }

    result.title = info.completeBaseName();
    const QString suffix = info.suffix().toLower();
    result.format = suffix;

    if (suffix == QStringLiteral("txt") || suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown")) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            if (error)
                *error = file.errorString();
            return result;
        }
        result.text = decodeTextFile(file.readAll());
        result.format = suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown")
                            ? QStringLiteral("Markdown")
                            : QStringLiteral("Text");
    } else if (suffix == QStringLiteral("docx")) {
        ZipReader zip;
        if (!zip.read(filePath, error))
            return result;
        QByteArray documentXml = zip.fileData(QStringLiteral("word/document.xml"), error);
        if (documentXml.isEmpty())
            return result;
        result.text = docxText(documentXml);
        result.format = QStringLiteral("Word");
    } else if (suffix == QStringLiteral("pdf")) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            if (error)
                *error = file.errorString();
            return result;
        }
        PdfTextExtractor extractor;
        result.text = extractor.extract(file.readAll(), error);
        result.format = QStringLiteral("PDF");
    } else {
        if (error)
            *error = QStringLiteral("Unsupported file type: .%1").arg(suffix);
        return result;
    }

    result.characterCount = result.text.size();
    return result;
}

