#include "ZipReader.h"

#include <QFile>
#include <QDataStream>
#include <QIODevice>

#include <zlib.h>

namespace {
quint16 readU16(const QByteArray &data, int offset)
{
    if (offset < 0 || offset + 2 > data.size())
        return 0;
    return static_cast<quint16>(static_cast<unsigned char>(data.at(offset)))
           | static_cast<quint16>(static_cast<unsigned char>(data.at(offset + 1)) << 8);
}

quint32 readU32(const QByteArray &data, int offset)
{
    if (offset < 0 || offset + 4 > data.size())
        return 0;
    return static_cast<quint32>(static_cast<unsigned char>(data.at(offset)))
           | static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1)) << 8)
           | static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2)) << 16)
           | static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3)) << 24);
}

bool inflateData(const QByteArray &compressed, quint32 expectedSize, QByteArray &out)
{
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return false;

    QByteArray buffer;
    buffer.resize(qMax<quint32>(expectedSize, static_cast<quint32>(compressed.size() * 4 + 1024)));
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = reinterpret_cast<Bytef *>(buffer.data());
    stream.avail_out = static_cast<uInt>(buffer.size());

    int result = inflate(&stream, Z_FINISH);
    if (result != Z_STREAM_END) {
        inflateEnd(&stream);
        return false;
    }
    out = buffer.left(buffer.size() - stream.avail_out);
    inflateEnd(&stream);
    return true;
}
}

bool ZipReader::read(const QString &filePath, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = file.errorString();
        if (error)
            *error = m_error;
        return false;
    }
    m_raw = file.readAll();
    file.close();
    m_entries.clear();

    if (m_raw.size() < 22) {
        m_error = QStringLiteral("File is too small to be a ZIP archive.");
        if (error)
            *error = m_error;
        return false;
    }

    int eocd = -1;
    const int searchStart = qMax(0, m_raw.size() - 65557);
    for (int i = m_raw.size() - 22; i >= searchStart; --i) {
        if (readU32(m_raw, i) == 0x06054b50) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        m_error = QStringLiteral("End of central directory record was not found.");
        if (error)
            *error = m_error;
        return false;
    }

    const quint16 entryCount = readU16(m_raw, eocd + 10);
    const quint32 centralDirectoryOffset = readU32(m_raw, eocd + 16);
    int cursor = static_cast<int>(centralDirectoryOffset);

    for (quint16 i = 0; i < entryCount; ++i) {
        if (cursor + 46 > m_raw.size() || readU32(m_raw, cursor) != 0x02014b50)
            break;
        ZipEntry entry;
        entry.compressionMethod = readU16(m_raw, cursor + 10);
        entry.compressedSize = readU32(m_raw, cursor + 20);
        entry.uncompressedSize = readU32(m_raw, cursor + 24);
        const quint16 nameLength = readU16(m_raw, cursor + 28);
        const quint16 extraLength = readU16(m_raw, cursor + 30);
        const quint16 commentLength = readU16(m_raw, cursor + 32);
        entry.localHeaderOffset = readU32(m_raw, cursor + 42);
        const int nameStart = cursor + 46;
        if (nameStart + nameLength > m_raw.size())
            break;
        entry.name = QString::fromUtf8(m_raw.mid(nameStart, nameLength));
        entry.directory = entry.name.endsWith('/');
        m_entries.append(entry);
        cursor = nameStart + nameLength + extraLength + commentLength;
    }

    return true;
}

QByteArray ZipReader::fileData(const QString &entryName, QString *error) const
{
    const ZipEntry *match = nullptr;
    for (const ZipEntry &entry : m_entries) {
        if (entry.name.compare(entryName, Qt::CaseInsensitive) == 0) {
            match = &entry;
            break;
        }
    }
    if (!match) {
        if (error)
            *error = QStringLiteral("Entry not found: %1").arg(entryName);
        return {};
    }

    const int localOffset = static_cast<int>(match->localHeaderOffset);
    if (localOffset + 30 > m_raw.size() || readU32(m_raw, localOffset) != 0x04034b50) {
        if (error)
            *error = QStringLiteral("Invalid local file header for %1").arg(entryName);
        return {};
    }
    const quint16 nameLength = readU16(m_raw, localOffset + 26);
    const quint16 extraLength = readU16(m_raw, localOffset + 28);
    const int dataOffset = localOffset + 30 + nameLength + extraLength;
    if (dataOffset + match->compressedSize > static_cast<quint32>(m_raw.size())) {
        if (error)
            *error = QStringLiteral("Compressed data for %1 is truncated.").arg(entryName);
        return {};
    }

    const QByteArray compressed = m_raw.mid(dataOffset, match->compressedSize);
    if (match->compressionMethod == 0)
        return compressed;
    if (match->compressionMethod == 8) {
        QByteArray out;
        if (!inflateData(compressed, match->uncompressedSize, out)) {
            if (error)
                *error = QStringLiteral("Unable to decompress %1.").arg(entryName);
            return {};
        }
        return out;
    }

    if (error)
        *error = QStringLiteral("Unsupported compression method %1.").arg(match->compressionMethod);
    return {};
}

QList<ZipEntry> ZipReader::entries() const
{
    return m_entries;
}

