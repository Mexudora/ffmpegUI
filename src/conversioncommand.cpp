#include "conversioncommand.h"

#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>

#include <cmath>

namespace {

QUrl withExtension(const QString &path, const QString &extension)
{
    const QFileInfo file(path);
    QString stem = file.completeBaseName();
    if (stem.isEmpty())
        stem = file.fileName();
    return QUrl::fromLocalFile(file.dir().filePath(stem + QLatin1Char('.') + extension));
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
int ConversionCommand::audioBitrateKbps() const { return m_audioBitrateKbps; }
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
    m_outputFile = url.isLocalFile()
        ? withExtension(url.toLocalFile(), m_container) : QUrl();
    rebuild();
    emit optionsChanged();
}

void ConversionCommand::setOutputFile(const QUrl &url)
{
    if (m_outputFile == url) {
        // The file may have been removed since the previous validation.
        rebuild();
        return;
    }
    m_outputFile = url;
    rebuild();
    emit optionsChanged();
}

void ConversionCommand::setContainer(const QString &container)
{
    if (m_container == container)
        return;
    m_container = container;
    if (m_outputFile.isLocalFile())
        m_outputFile = withExtension(m_outputFile.toLocalFile(), m_container);
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

void ConversionCommand::setOutputPath(const QString &path)
{
    setOutputFile(path.isEmpty() ? QUrl()
        : QUrl::fromLocalFile(QDir::fromNativeSeparators(path)));
}

void ConversionCommand::setAudioBitrateKbps(int bitrate)
{
    if (m_audioBitrateKbps == bitrate)
        return;
    m_audioBitrateKbps = bitrate;
    rebuild();
    emit optionsChanged();
}

ConversionJob ConversionCommand::createJob(double durationSeconds) const
{
    ConversionJob job;
    job.inputPath = m_inputFile.isLocalFile() ? m_inputFile.toLocalFile() : QString();
    job.outputPath = m_outputFile.isLocalFile() ? m_outputFile.toLocalFile() : QString();
    job.program = m_executablePath.trimmed();
    job.container = m_container;
    job.durationSeconds = std::isfinite(durationSeconds) && durationSeconds > 0
        ? durationSeconds : 0;
    job.streams = {
        {QStringLiteral("video"), QStringLiteral("0:V:0?"), m_videoCodec, 0},
        {QStringLiteral("audio"), QStringLiteral("0:a:0?"), m_audioCodec, m_audioBitrateKbps}
    };
    return job;
}

void ConversionCommand::rebuild()
{
    // Invalid or changed options must never leave the old command available.
    m_program.clear();
    m_arguments.clear();
    m_commandPreview.clear();
    const ConversionJob job = createJob(0);
    m_error = job.validationError();

    if (m_error.isEmpty()) {
        m_program = job.program;
        m_arguments = job.arguments();

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

bool ConversionCommand::refresh()
{
    rebuild();
    return valid();
}
