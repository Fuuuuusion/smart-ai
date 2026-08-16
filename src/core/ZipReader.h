#pragma once

#include <QByteArray>
#include <QString>
#include <QList>

struct ZipEntry
{
    QString name;
    bool directory = false;
    quint32 compressedSize = 0;
    quint32 uncompressedSize = 0;
    quint16 compressionMethod = 0;
    quint32 localHeaderOffset = 0;
};

class ZipReader
{
public:
    bool read(const QString &filePath, QString *error = nullptr);
    QByteArray fileData(const QString &entryName, QString *error = nullptr) const;
    QList<ZipEntry> entries() const;

private:
    QByteArray m_raw;
    QList<ZipEntry> m_entries;
    QString m_error;
};

