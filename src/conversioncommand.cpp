#include "conversioncommand.h"

#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>

namespace {

QString comparablePath(const QFileInfo &file)
{
    const QString canonical = file.canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(file.absoluteFilePath()) : canonical;
}

QString quoteForPowerShell(QString value)
{
    // Single-quoted PowerShell strings do not expand $, backticks or expressions.
    value.replace(QStringLiteral("'"), QStringLiteral("''"));
    // PowerShell also recognizes typographic single quotation marks.
    for (const QChar quote : {QChar(0x2018), QChar(0x2019)})
        value.replace(quote, QString(2, quote));
    return QStringLiteral("'") + value + QStringLiteral("'");
}

} // namespace

ConversionCommand::ConversionCommand(QObject *parent)
    : QObject(parent)
{
    rebuild();
}

QString ConversionCommand::executablePath() const { return m_executablePath; }
QUrl ConversionCommand::inputFile() const { return m_inputFile; }
QUrl ConversionCommand::outputFile() const { return m_outputFile; }
QString ConversionCommand::outputPath() const { return m_outputFile.toLocalFile(); }
QString ConversionCommand::container() const { return m_container; }
QString ConversionCommand::videoCodec() const { return m_videoCodec; }
QString ConversionCommand::audioCodec() const { return m_audioCodec; }
bool ConversionCommand::valid() const { return m_error.isEmpty() && !m_arguments.isEmpty(); }
QString ConversionCommand::error() const { return m_error; }
QString ConversionCommand::program() const { return m_program; }
QStringList ConversionCommand::arguments() const { return m_arguments; }
QString ConversionCommand::commandPreview() const { return m_commandPreview; }

void ConversionCommand::setExecutablePath(const QString &path)
{
    if (m_executablePath == path)
        return;
    m_executablePath = path;
    rebuild();
    emit optionsChanged();
}

void ConversionCommand::setInputFile(const QUrl &url)
{
    if (m_inputFile == url)
        return;
    m_inputFile = url;
    // A destination chosen for a previous source must not silently carry over.
    m_outputFile = QUrl();
    rebuild();
    emit optionsChanged();
}

void ConversionCommand::setOutputFile(const QUrl &url)
{
    if (m_outputFile == url)
        return;
    m_outputFile = url;
    rebuild();
    emit optionsChanged();
}

void ConversionCommand::setContainer(const QString &container)
{
    if (m_container == container)
        return;
    m_container = container;
    rebuild();
    emit optionsChanged();
}

void ConversionCommand::setVideoCodec(const QString &codec)
{
    if (m_videoCodec == codec)
        return;
    m_videoCodec = codec;
    rebuild();
    emit optionsChanged();
}

void ConversionCommand::setAudioCodec(const QString &codec)
{
    if (m_audioCodec == codec)
        return;
    m_audioCodec = codec;
    rebuild();
    emit optionsChanged();
}

QString ConversionCommand::validationError() const
{
    if (!m_inputFile.isLocalFile())
        return tr("Select a local input file.");

    const QFileInfo input(m_inputFile.toLocalFile());
    if (!input.isFile() || !input.isReadable())
        return tr("The input file is unavailable or unreadable.");

    if (!m_outputFile.isLocalFile())
        return tr("Choose an output file.");

    const QFileInfo output(m_outputFile.toLocalFile());
    if (!output.isAbsolute() || output.fileName().isEmpty())
        return tr("Choose a full path for the output file.");

#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity pathCase = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathCase = Qt::CaseSensitive;
#endif
    if (QString::compare(comparablePath(input), comparablePath(output), pathCase) == 0)
        return tr("Input and output must be different files.");

    if (output.exists() || output.isSymLink())
        return tr("The output file already exists. Choose a new name.");
    if (!output.dir().exists())
        return tr("The output directory does not exist.");

    if (m_container != QStringLiteral("mp4") && m_container != QStringLiteral("mkv"))
        return tr("Unsupported container. Choose MP4 or MKV.");
    if (output.suffix().compare(m_container, Qt::CaseInsensitive) != 0)
        return tr("The output file extension must be .%1.").arg(m_container);

    if (m_videoCodec != QStringLiteral("copy") && m_videoCodec != QStringLiteral("libx264"))
        return tr("Unsupported video mode.");
    if (m_audioCodec != QStringLiteral("copy") && m_audioCodec != QStringLiteral("aac"))
        return tr("Unsupported audio mode.");

    const QFileInfo executable(m_executablePath.trimmed());
    if (!executable.isAbsolute() || !executable.isFile())
        return tr("The ffmpeg executable was not found. Check its full path.");

    return {};
}

void ConversionCommand::rebuild()
{
    // Invalid or changed options must never leave the old command available.
    m_program.clear();
    m_arguments.clear();
    m_commandPreview.clear();
    m_error = validationError();

    if (m_error.isEmpty()) {
        m_program = QFileInfo(m_executablePath.trimmed()).absoluteFilePath();
        m_arguments = {
            QStringLiteral("-hide_banner"),
            QStringLiteral("-n"),
            QStringLiteral("-i"), QFileInfo(m_inputFile.toLocalFile()).absoluteFilePath(),
            QStringLiteral("-map"), QStringLiteral("0:V:0?"),
            QStringLiteral("-map"), QStringLiteral("0:a:0?"),
            QStringLiteral("-c:v"), m_videoCodec,
            QStringLiteral("-c:a"), m_audioCodec,
            QStringLiteral("-f"), m_container == QStringLiteral("mkv")
                ? QStringLiteral("matroska") : QStringLiteral("mp4"),
            QFileInfo(m_outputFile.toLocalFile()).absoluteFilePath()
        };

        // Rendering is separate from the unquoted arguments used by QProcess later.
        QStringList tokens {quoteForPowerShell(m_program)};
        for (const QString &argument : m_arguments)
            tokens.append(quoteForPowerShell(argument));
        m_commandPreview = QStringLiteral("& ") + tokens.join(QLatin1Char(' '));
    }

    emit commandChanged();
}

void ConversionCommand::copyPreview() const
{
    if (valid()) {
        if (QClipboard *clipboard = QGuiApplication::clipboard())
            clipboard->setText(m_commandPreview);
    }
}
