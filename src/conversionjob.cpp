#include "conversionjob.h"

#include <QDir>
#include <QFileInfo>

QString ConversionJob::pathKey(const QString &path)
{
    const QFileInfo file(path);
    QString result = file.canonicalFilePath();
    if (result.isEmpty()) {
        QString parent = file.dir().canonicalPath();
        if (parent.isEmpty())
            parent = file.absolutePath();
        result = QDir(parent).filePath(file.fileName());
    }
    result = QDir::cleanPath(result);
#ifdef Q_OS_WIN
    result = result.toCaseFolded();
#endif
    return result;
}

QString ConversionJob::validationError() const
{
    const QFileInfo input(inputPath);
    if (inputPath.isEmpty() || !input.isAbsolute() || !input.isFile() || !input.isReadable())
        return tr("The input file is unavailable or unreadable.");

    const QFileInfo output(outputPath);
    if (outputPath.isEmpty() || !output.isAbsolute() || output.fileName().isEmpty())
        return tr("Choose a full path for the output file.");
    if (pathKey(inputPath) == pathKey(outputPath))
        return tr("Input and output must be different files.");
    if (output.exists() || output.isSymLink())
        return tr("The output file already exists. Choose a new name.");
    if (!output.dir().exists())
        return tr("The output directory does not exist.");
    if (container != QStringLiteral("mp4") && container != QStringLiteral("mkv"))
        return tr("Unsupported container. Choose MP4 or MKV.");
    if (output.suffix().compare(container, Qt::CaseInsensitive) != 0)
        return tr("The output file extension must be .%1.").arg(container);

    if (streams.isEmpty())
        return tr("Select at least one output stream.");
    for (const StreamSettings &stream : streams) {
        if (stream.inputSpecifier.isEmpty())
            return tr("A stream selector is missing.");
        if (stream.type == QStringLiteral("video")) {
            if (stream.codec != QStringLiteral("copy") && stream.codec != QStringLiteral("libx264"))
                return tr("Unsupported video mode.");
        } else if (stream.type == QStringLiteral("audio")) {
            if (stream.codec != QStringLiteral("copy") && stream.codec != QStringLiteral("aac"))
                return tr("Unsupported audio mode.");
            if (stream.codec != QStringLiteral("copy") && stream.bitrateKbps <= 0)
                return tr("Audio bitrate must be positive.");
        } else {
            return tr("This stream type is not supported yet.");
        }
    }

    const QFileInfo executable(program);
    if (program.isEmpty() || !executable.isAbsolute() || !executable.isFile())
        return tr("The ffmpeg executable was not found. Check its full path.");
    return {};
}

QStringList ConversionJob::arguments() const
{
    QStringList result {
        QStringLiteral("-hide_banner"), QStringLiteral("-n"),
        QStringLiteral("-i"), inputPath
    };
    for (const StreamSettings &stream : streams)
        result << QStringLiteral("-map") << stream.inputSpecifier;

    int videoIndex = 0;
    int audioIndex = 0;
    for (const StreamSettings &stream : streams) {
        if (stream.type == QStringLiteral("video")) {
            result << QStringLiteral("-c:v:%1").arg(videoIndex++) << stream.codec;
        } else if (stream.type == QStringLiteral("audio")) {
            const QString index = QString::number(audioIndex++);
            result << QStringLiteral("-c:a:") + index << stream.codec;
            if (stream.codec != QStringLiteral("copy"))
                result << QStringLiteral("-b:a:") + index
                       << QString::number(stream.bitrateKbps) + QLatin1Char('k');
        }
    }
    result << QStringLiteral("-f")
           << (container == QStringLiteral("mkv") ? QStringLiteral("matroska") : QStringLiteral("mp4"))
           << outputPath;
    return result;
}
