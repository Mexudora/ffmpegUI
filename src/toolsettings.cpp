#include "toolsettings.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QVariant>

namespace {
const QString directoryKey = QStringLiteral("tools/ffmpegDirectory");
QString toolPath(const QString &directory, const QString &name)
{
    // An empty setting must never turn into a path relative to the working directory.
    return directory.isEmpty() ? QString() : QDir(directory).filePath(name);
}
}

ToolSettings::ToolSettings(QObject *parent) : QObject(parent)
{
    m_directory = QSettings().value(directoryKey).toString();
}

QString ToolSettings::directory() const { return m_directory; }
QString ToolSettings::error() const { return m_error; }
QString ToolSettings::ffmpegPath() const
{
    return toolPath(m_directory, QStringLiteral("ffmpeg.exe"));
}
QString ToolSettings::ffprobePath() const
{
    return toolPath(m_directory, QStringLiteral("ffprobe.exe"));
}
QString ToolSettings::pathFromUrl(const QUrl &url) const
{
    return url.isLocalFile() ? QDir::toNativeSeparators(url.toLocalFile()) : QString();
}
QString ToolSettings::validateDirectory(const QString &directory)
{
    if (directory.isEmpty()) return tr("Select the FFmpeg directory in Settings first.");
    if (!QFileInfo(directory).isAbsolute() || !QFileInfo(directory).isDir())
        return tr("Choose an existing absolute directory path.");
    for (const QString &name : {QStringLiteral("ffmpeg.exe"), QStringLiteral("ffprobe.exe")}) {
        const QFileInfo file(QDir(directory).filePath(name));
        if (!file.isFile() || !file.isReadable())
            return tr("The directory must contain a readable %1 file.").arg(name);
    }
    return {};
}
QString ToolSettings::validationError() const { return validateDirectory(m_directory); }

bool ToolSettings::saveDirectory(const QString &directory)
{
    const QString normalized = QDir::fromNativeSeparators(directory.trimmed());
    m_error = validateDirectory(normalized);
    if (!m_error.isEmpty()) { emit errorChanged(); return false; }
    const QString clean = QDir::cleanPath(normalized);
    QSettings settings;
    const QVariant previous = settings.value(directoryKey);
    settings.setValue(directoryKey, clean);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        // Keep the active configuration unchanged when persistence fails.
        if (previous.isValid()) settings.setValue(directoryKey, previous);
        else settings.remove(directoryKey);
        m_error = tr("Could not save settings. Check access permissions.");
        emit errorChanged();
        return false;
    }
    m_directory = clean;
    emit errorChanged();
    // Also notify after revalidating the same path (tools may have been restored).
    emit directoryChanged();
    return true;
}
void ToolSettings::clearError()
{
    if (m_error.isEmpty()) return;
    m_error.clear();
    emit errorChanged();
}
