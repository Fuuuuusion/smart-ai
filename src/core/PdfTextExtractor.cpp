#include "PdfTextExtractor.h"

#include <QRegularExpression>
#include <QStringList>

#include <zlib.h>

namespace {
QByteArray inflateBytes(const QByteArray &compressed, QString *error)
{
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    if (inflateInit(&stream) != Z_OK) {
        if (error)
            *error = QStringLiteral("Unable to initialize zlib.");
        return {};
    }

    QByteArray output;
    QByteArray chunk;
    chunk.resize(16384);
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
    stream.avail_in = static_cast<uInt>(compressed.size());

    int result = Z_OK;
    while (result == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef *>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());
        result = inflate(&stream, Z_NO_FLUSH);
        output.append(chunk.constData(), chunk.size() - stream.avail_out);
        if (result == Z_STREAM_END)
            break;
        if (result != Z_OK) {
            if (error)
                *error = QStringLiteral("Unable to decompress PDF stream.");
            inflateEnd(&stream);
            return {};
        }
    }
    inflateEnd(&stream);
    return output;
}

int findByte(const QByteArray &haystack, const QByteArray &needle, int from)
{
    if (from < 0)
        from = 0;
    return static_cast<int>(haystack.indexOf(needle, from));
}

QByteArray sliceBetween(const QByteArray &data, int start, int end)
{
    if (start < 0 || end <= start || end > data.size())
        return {};
    return data.mid(start, end - start);
}
}

QString PdfTextExtractor::extract(const QByteArray &pdfData, QString *error) const
{
    if (pdfData.isEmpty()) {
        if (error)
            *error = QStringLiteral("PDF data is empty.");
        return {};
    }

    QStringList pages;
    int cursor = 0;
    const QByteArray streamToken("stream");
    const QByteArray endstreamToken("endstream");

    while (cursor < pdfData.size()) {
        const int streamPos = findByte(pdfData, streamToken, cursor);
        if (streamPos < 0)
            break;
        const int dictStart = qMax(0, streamPos - 2048);
        const QByteArray dictionary = pdfData.mid(dictStart, streamPos - dictStart);
        const int dataStart = streamPos + streamToken.size();
        if (dataStart < pdfData.size() && (pdfData.at(dataStart) == '\r' || pdfData.at(dataStart) == '\n')) {
            int offset = dataStart;
            while (offset < pdfData.size() && (pdfData.at(offset) == '\r' || pdfData.at(offset) == '\n'))
                ++offset;
            const int endStream = findByte(pdfData, endstreamToken, offset);
            if (endStream < 0)
                break;
            int streamEnd = endStream;
            while (streamEnd > offset && (pdfData.at(streamEnd - 1) == '\r' || pdfData.at(streamEnd - 1) == '\n'))
                --streamEnd;
            const QByteArray encoded = sliceBetween(pdfData, offset, streamEnd);
            QString localError;
            const QByteArray decoded = decodeStream(encoded, dictionary, &localError);
            if (!decoded.isEmpty())
                pages.append(extractTextCommands(decoded));
            cursor = endStream + endstreamToken.size();
        } else {
            cursor = dataStart;
        }
    }

    return pages.join('\n').simplified();
}

QByteArray PdfTextExtractor::decodeStream(const QByteArray &stream, const QByteArray &dictionary, QString *error) const
{
    if (dictionary.contains("/FlateDecode") || dictionary.contains("/Fl")) {
        return inflateBytes(stream, error);
    }
    return stream;
}

QString PdfTextExtractor::decodePdfString(const QByteArray &bytes) const
{
    QString result;
    result.reserve(bytes.size());
    for (int i = 0; i < bytes.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(bytes.at(i));
        if (ch == '\\' && i + 1 < bytes.size()) {
            const char next = bytes.at(i + 1);
            if (next == 'n') {
                result.append('\n');
                ++i;
            } else if (next == 'r') {
                result.append('\r');
                ++i;
            } else if (next == 't') {
                result.append('\t');
                ++i;
            } else if (next == '(' || next == ')' || next == '\\') {
                result.append(QLatin1Char(next));
                ++i;
            } else if (next >= '0' && next <= '7') {
                int value = 0;
                int count = 0;
                while (i + 1 < bytes.size() && count < 3 && bytes.at(i + 1) >= '0' && bytes.at(i + 1) <= '7') {
                    value = value * 8 + (bytes.at(i + 1) - '0');
                    ++i;
                    ++count;
                }
                result.append(QChar(value));
            } else {
                result.append(QLatin1Char(next));
                ++i;
            }
        } else {
            result.append(QLatin1Char(ch));
        }
    }
    return result;
}

QString PdfTextExtractor::extractTextCommands(const QByteArray &content) const
{
    QString text;
    int position = 0;
    while (position < content.size()) {
        const int bt = findByte(content, "BT", position);
        if (bt < 0)
            break;
        const int et = findByte(content, "ET", bt + 2);
        if (et < 0)
            break;

        const QByteArray block = content.mid(bt, et - bt);
        int i = 0;
        while (i < block.size()) {
            if (block.at(i) == '(') {
                int depth = 1;
                int j = i + 1;
                bool escaped = false;
                while (j < block.size() && depth > 0) {
                    const char ch = block.at(j);
                    if (escaped) {
                        escaped = false;
                    } else if (ch == '\\') {
                        escaped = true;
                    } else if (ch == '(') {
                        ++depth;
                    } else if (ch == ')') {
                        --depth;
                    }
                    ++j;
                }
                if (depth == 0) {
                    const QByteArray value = block.mid(i + 1, j - i - 2);
                    int after = j;
                    while (after < block.size() && (block.at(after) == ' ' || block.at(after) == '\r' || block.at(after) == '\n' || block.at(after) == '\t'))
                        ++after;
                    if (block.mid(after, 2) == "Tj") {
                        const QString decoded = decodePdfString(value);
                        if (!decoded.isEmpty())
                            text.append(decoded).append('\n');
                        i = after + 2;
                    } else {
                        i = j;
                    }
                } else {
                    i = j;
                }
            } else if (block.at(i) == '[') {
                const int close = findByte(block, "]", i + 1);
                if (close < 0)
                    break;
                QStringList pieces;
                int k = i + 1;
                while (k < close) {
                    const int open = findByte(block, "(", k);
                    if (open < 0 || open >= close)
                        break;
                    int depth = 1;
                    int j = open + 1;
                    bool escaped = false;
                    while (j < close && depth > 0) {
                        const char ch = block.at(j);
                        if (escaped) {
                            escaped = false;
                        } else if (ch == '\\') {
                            escaped = true;
                        } else if (ch == '(') {
                            ++depth;
                        } else if (ch == ')') {
                            --depth;
                        }
                        ++j;
                    }
                    if (depth != 0)
                        break;
                    const QByteArray value = block.mid(open + 1, j - open - 2);
                    pieces.append(decodePdfString(value));
                    k = j;
                }
                int after = close + 1;
                while (after < block.size() && (block.at(after) == ' ' || block.at(after) == '\r' || block.at(after) == '\n' || block.at(after) == '\t'))
                    ++after;
                if (block.mid(after, 2) == "TJ")
                    text.append(pieces.join(QString())).append('\n');
                i = close + 1;
            } else {
                ++i;
            }
        }
        position = et + 2;
    }
    return text;
}
