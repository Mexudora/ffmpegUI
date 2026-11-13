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
            if (stream.attachedPicture && stream.codec != QStringLiteral("copy"))
                return tr("Cover art supports stream copying only in this version.");
            if (stream.codec != QStringLiteral("copy") && stream.codec != QStringLiteral("libx264"))
                return tr("Unsupported video mode.");
        } else if (stream.type == QStringLiteral("audio")) {
            if (stream.codec != QStringLiteral("copy") && stream.codec != QStringLiteral("aac"))
                return tr("Unsupported audio mode.");
            if (stream.codec != QStringLiteral("copy") && stream.bitrateKbps <= 0)
                return tr("Audio bitrate must be positive.");
        } else if (stream.type == QStringLiteral("subtitle")) {
            if (stream.codec != QStringLiteral("copy") && !textSubtitle(stream.sourceCodec))
                return tr("This subtitle cannot be converted to text. Disable it or use compatible stream copying.");
            if (container == QStringLiteral("mp4")) {
                if (stream.codec != QStringLiteral("mov_text") && stream.codec != QStringLiteral("copy"))
                    return tr("MP4 subtitles require mov_text in this version.");
                if (stream.codec == QStringLiteral("copy") && stream.sourceCodec != QStringLiteral("mov_text"))
                    return tr("MP4 can copy only mov_text subtitles here. Convert, choose MKV or disable the stream.");
            } else {
                if (stream.codec != QStringLiteral("copy") && stream.codec != QStringLiteral("srt"))
                    return tr("Choose subtitle copying or SRT for MKV.");
                if (stream.codec == QStringLiteral("copy") && stream.sourceCodec == QStringLiteral("mov_text"))
                    return tr("Convert mov_text subtitles to SRT for MKV.");
                const QStringList copyCodecs {
                    QStringLiteral("subrip"), QStringLiteral("srt"), QStringLiteral("ass"),
                    QStringLiteral("ssa"), QStringLiteral("webvtt"), QStringLiteral("text"),
                    QStringLiteral("dvd_subtitle"), QStringLiteral("dvb_subtitle"),
                    QStringLiteral("hdmv_pgs_subtitle"), QStringLiteral("hdmv_text_subtitle"),
                    QStringLiteral("arib_caption")
                };
                if (stream.codec == QStringLiteral("copy") && !copyCodecs.contains(stream.sourceCodec))
                    return tr("This subtitle codec is not enabled for MKV copying. Convert a supported text subtitle or disable it.");
            }
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

    int videoIndex = 0, audioIndex = 0, subtitleIndex = 0;
    for (const StreamSettings &stream : streams) {
        QString spec;
        if (stream.type == QStringLiteral("video"))
            spec = QStringLiteral("v:%1").arg(videoIndex++);
        else if (stream.type == QStringLiteral("audio"))
            spec = QStringLiteral("a:%1").arg(audioIndex++);
        else if (stream.type == QStringLiteral("subtitle"))
            spec = QStringLiteral("s:%1").arg(subtitleIndex++);
        else
            continue;
        result << QStringLiteral("-c:") + spec << stream.codec;
        if (stream.type == QStringLiteral("audio") && stream.codec != QStringLiteral("copy"))
            result << QStringLiteral("-b:") + spec
                   << QString::number(stream.bitrateKbps) + QLatin1Char('k');
        if (!stream.title.isEmpty())
            result << QStringLiteral("-metadata:s:") + spec << QStringLiteral("title=") + stream.title;
        if (!stream.language.isEmpty())
            result << QStringLiteral("-metadata:s:") + spec << QStringLiteral("language=") + stream.language;
        QStringList dispositions;
        if (stream.defaultDisposition) dispositions << QStringLiteral("default");
        if (stream.forcedDisposition) dispositions << QStringLiteral("forced");
        if (stream.attachedPicture) dispositions << QStringLiteral("attached_pic");
        // Legacy draft selectors are optional and do not carry source dispositions.
        if (!stream.sourceCodec.isEmpty())
            result << QStringLiteral("-disposition:") + spec
                   << (dispositions.isEmpty() ? QStringLiteral("0") : dispositions.join(QLatin1Char('+')));
    }
    result << QStringLiteral("-f")
           << (container == QStringLiteral("mkv") ? QStringLiteral("matroska") : QStringLiteral("mp4"))
           << outputPath;
    return result;
}

bool ConversionJob::textSubtitle(const QString &codec)
{
    return QStringList {QStringLiteral("subrip"), QStringLiteral("srt"),
        QStringLiteral("ass"), QStringLiteral("ssa"), QStringLiteral("webvtt"),
        QStringLiteral("mov_text"), QStringLiteral("text")}.contains(codec);
}

QString ConversionJob::preview() const
{
    auto quote = [](QString value) {
        value.replace(QStringLiteral("'"), QStringLiteral("''"));
        for (const QChar mark : {QChar(0x2018), QChar(0x2019)})
            value.replace(mark, QString(2, mark));
        return QStringLiteral("'") + value + QStringLiteral("'");
    };
    QStringList tokens {quote(program)};
    for (const QString &argument : arguments())
        tokens.append(quote(argument));
    return QStringLiteral("& ") + tokens.join(QLatin1Char(' '));
}
