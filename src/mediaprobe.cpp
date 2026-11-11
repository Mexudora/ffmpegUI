#include "mediaprobe.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

MediaProbe::MediaProbe(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::finished,
            this, &MediaProbe::handleFinished);

    connect(&m_process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError processError) {
        // A failed start does not emit finished(). Crashes are handled there.
        if (processError == QProcess::FailedToStart) {
            finishFailure(tr("Could not start ffprobe: %1")
                              .arg(m_process.errorString()));
        }
    });
}

MediaProbe::~MediaProbe()
{
    // QProcess may emit signals while its destructor stops a running child.
    // Disconnect before the other members are destroyed.
    disconnect(&m_process, nullptr, this, nullptr);
}

QString MediaProbe::executablePath() const
{
    return m_executablePath;
}

void MediaProbe::setExecutablePath(const QString &path)
{
    if (m_executablePath == path)
        return;

    m_executablePath = path;
    emit executablePathChanged();
}

bool MediaProbe::busy() const
{
    return m_busy;
}

QString MediaProbe::error() const
{
    return m_error;
}

QString MediaProbe::sourcePath() const
{
    return m_sourcePath;
}

QVariantMap MediaProbe::metadata() const
{
    return m_metadata;
}

void MediaProbe::probe(const QUrl &fileUrl)
{
    // This stage supports one analysis at a time; conversion queues come later.
    if (m_busy)
        return;

    setError({});
    m_metadata.clear();
    emit metadataChanged();

    m_sourcePath = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : QString();
    emit sourcePathChanged();

    if (!fileUrl.isLocalFile()) {
        setError(tr("Select a local media file."));
        return;
    }

    const QFileInfo source(m_sourcePath);
    if (!source.isFile() || !source.isReadable()) {
        setError(tr("The selected media file is unavailable or unreadable."));
        return;
    }

    const QFileInfo executable(m_executablePath.trimmed());
    if (!executable.isAbsolute() || !executable.isFile()) {
        setError(tr("Enter the full path to the ffprobe executable, without quotes."));
        return;
    }

    // Each argument is a separate element. Do not quote paths or invoke a shell.
    const QStringList arguments {
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-print_format"), QStringLiteral("json"),
        QStringLiteral("-show_format"), QStringLiteral("-show_streams"),
        QStringLiteral("-show_entries"),
        QStringLiteral("format=format_name,duration:"
                       "stream=index,codec_type,codec_name,width,height,"
                       "avg_frame_rate,sample_rate,channels"),
        QStringLiteral("-i"), source.absoluteFilePath()
    };

    setBusy(true);
    m_process.start(executable.absoluteFilePath(), arguments);
}

void MediaProbe::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const QByteArray output = m_process.readAllStandardOutput();
    const QString details = QString::fromUtf8(m_process.readAllStandardError())
                                .trimmed().left(4096);

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        QString message = exitStatus == QProcess::CrashExit
            ? tr("ffprobe crashed.")
            : tr("ffprobe exited with code %1.").arg(exitCode);
        if (!details.isEmpty())
            message += QStringLiteral("\n") + details;
        finishFailure(message);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(output, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        finishFailure(tr("ffprobe returned invalid JSON."));
        return;
    }

    const QJsonObject root = document.object();
    if (!root.value(QStringLiteral("format")).isObject()
        || !root.value(QStringLiteral("streams")).isArray()) {
        finishFailure(tr("ffprobe returned no valid format or stream information."));
        return;
    }

    m_metadata = root.toVariantMap();
    emit metadataChanged();
    setBusy(false);
}

void MediaProbe::setBusy(bool busy)
{
    if (m_busy == busy)
        return;

    m_busy = busy;
    emit busyChanged();
}

void MediaProbe::setError(const QString &error)
{
    if (m_error == error)
        return;

    m_error = error;
    emit errorChanged();
}

void MediaProbe::finishFailure(const QString &message)
{
    setError(message);
    setBusy(false);
}