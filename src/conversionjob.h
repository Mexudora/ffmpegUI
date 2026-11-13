#pragma once

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>

// One output stream. Selectors belong to this input, not to rows of the UI.
struct StreamSettings
{
    QString type;
    QString inputSpecifier;
    QString codec;
    int bitrateKbps = 0;
};

// A value snapshot: the queue owns its copy and does not expose mutable fields.
struct ConversionJob
{
    Q_DECLARE_TR_FUNCTIONS(ConversionJob)

public:
    QString id;
    QString inputPath;
    QString outputPath;
    QString program;
    QString container;
    double durationSeconds = 0;
    QList<StreamSettings> streams;

    QString validationError() const;
    QStringList arguments() const;
    static QString pathKey(const QString &path);
};
