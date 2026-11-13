#include "mediaworkspace.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QUuid>

#include <algorithm>

namespace {
QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString normalized(QString text)
{
    return text.normalized(QString::NormalizationForm_C).simplified().toCaseFolded();
}
bool attached(const QVariantMap &stream)
{
    return stream.value(QStringLiteral("disposition")).toMap()
        .value(QStringLiteral("attached_pic")).toBool();
}
int typeOrder(const QString &type)
{
    return type == QStringLiteral("video") ? 0 : type == QStringLiteral("audio") ? 1 : 2;
}
QString outputName(const QString &path, const QString &container)
{
    const QFileInfo file(path);
    QString stem = file.completeBaseName();
    if (stem.isEmpty()) stem = file.fileName();
    return stem + QLatin1Char('.') + container;
}
}

MediaWorkspace::MediaWorkspace(QObject *parent) : QObject(parent), m_probe(this)
{
    m_probe.setExecutablePath(QStringLiteral("C:/ffmpeg/bin/ffprobe.exe"));
    connect(&m_probe, &MediaProbe::completed, this, &MediaWorkspace::probeCompleted);
}

int MediaWorkspace::count() const { return int(m_files.size()); }
QString MediaWorkspace::selectedId() const { return m_selectedId; }
int MediaWorkspace::selectedIndex() const
{
    for (int i = 0; i < count(); ++i)
        if (m_files[i].id == m_selectedId) return i;
    return -1;
}
bool MediaWorkspace::selectedInSet() const { return selectedIndex() >= 0; }
bool MediaWorkspace::busy() const { return m_processing || !m_requests.isEmpty(); }
QString MediaWorkspace::error() const { return m_error; }
QString MediaWorkspace::message() const { return m_message; }
QString MediaWorkspace::container() const { return m_container; }
QString MediaWorkspace::outputPath() const { return m_outputPath; }
bool MediaWorkspace::outputIsDirectory() const { return m_outputDirectory; }

QVariantMap MediaWorkspace::fileMap(const MediaFile &file) const
{
    return {{QStringLiteral("id"), file.id}, {QStringLiteral("path"), file.path},
        {QStringLiteral("name"), QFileInfo(file.path).fileName()},
        {QStringLiteral("ready"), file.state == Ready},
        {QStringLiteral("loading"), file.state == Loading},
        {QStringLiteral("error"), file.error}, {QStringLiteral("metadata"), file.metadata}};
}
QVariantList MediaWorkspace::files() const
{
    QVariantList result;
    for (const MediaFile &file : m_files) result.append(fileMap(file));
    return result;
}
QVariantMap MediaWorkspace::selectedFile() const
{
    const MediaFile *file = findFile(m_selectedId);
    return file ? fileMap(*file) : QVariantMap();
}
MediaWorkspace::MediaFile *MediaWorkspace::findFile(const QString &id)
{
    for (MediaFile &file : m_files) if (file.id == id) return &file;
    return m_hasPreview && m_preview.id == id ? &m_preview : nullptr;
}
const MediaWorkspace::MediaFile *MediaWorkspace::findFile(const QString &id) const
{
    for (const MediaFile &file : m_files) if (file.id == id) return &file;
    return m_hasPreview && m_preview.id == id ? &m_preview : nullptr;
}
int MediaWorkspace::findStagedPath(const QString &path) const
{
    const QString key = ConversionJob::pathKey(path);
    for (int i = 0; i < count(); ++i)
        if (ConversionJob::pathKey(m_files[i].path) == key) return i;
    return -1;
}

void MediaWorkspace::openFile(const QUrl &url)
{
    if (!url.isLocalFile()) {
        m_error = tr("Choose a local file.");
        emit statusChanged();
        return;
    }
    const QString path = QFileInfo(url.toLocalFile()).absoluteFilePath();
    const int staged = findStagedPath(path);
    if (staged >= 0) { selectFile(m_files[staged].id); return; }
    m_error.clear();
    m_preview = MediaFile();
    m_preview.id = newId();
    m_preview.path = path;
    m_hasPreview = true;
    m_selectedId = m_preview.id;
    m_requests.enqueue({m_preview.id, path});
    emit selectedChanged();
    emit statusChanged();
    QTimer::singleShot(0, this, &MediaWorkspace::dispatchProbe);
}

void MediaWorkspace::addFiles(const QList<QUrl> &urls)
{
    m_error.clear();
    int added = 0;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            m_error = tr("Only local files can be added.");
            continue;
        }
        const QString path = QFileInfo(url.toLocalFile()).absoluteFilePath();
        const int existing = findStagedPath(path);
        if (existing >= 0) {
            m_selectedId = m_files[existing].id;
            continue;
        }
        MediaFile file;
        if (m_hasPreview && ConversionJob::pathKey(m_preview.path) == ConversionJob::pathKey(path)) {
            file = m_preview;
            m_hasPreview = false;
        } else {
            file.id = newId();
            file.path = path;
            m_requests.enqueue({file.id, path});
        }
        m_selectedId = file.id;
        m_files.append(file);
        ++added;
    }
    normalizeOutputMode();
    rebuildRows();
    m_message = tr("Files added: %1.").arg(added);
    emit filesChanged();
    emit selectedChanged();
    emit statusChanged();
    QTimer::singleShot(0, this, &MediaWorkspace::dispatchProbe);
}

void MediaWorkspace::addFolder(const QUrl &url)
{
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isDir()) {
        m_error = tr("Choose an existing local directory.");
        emit statusChanged();
        return;
    }
    const QStringList extensions {
        QStringLiteral("mkv"), QStringLiteral("mp4"), QStringLiteral("avi"), QStringLiteral("mov"),
        QStringLiteral("webm"), QStringLiteral("m4v"), QStringLiteral("ts"), QStringLiteral("m2ts"),
        QStringLiteral("mts"), QStringLiteral("mpg"), QStringLiteral("mpeg"), QStringLiteral("wmv"),
        QStringLiteral("flv"), QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("m4a"),
        QStringLiteral("aac"), QStringLiteral("ogg"), QStringLiteral("opus"), QStringLiteral("wav"),
        QStringLiteral("wma"), QStringLiteral("srt"), QStringLiteral("ass"), QStringLiteral("ssa"),
        QStringLiteral("vtt"), QStringLiteral("sup"), QStringLiteral("sub")
    };
    QList<QUrl> urls;
    const QDir directory(url.toLocalFile());
    const auto entries = directory.entryInfoList(QDir::Files | QDir::Readable,
                                                QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &file : entries)
        if (extensions.contains(file.suffix().toLower()))
            urls.append(QUrl::fromLocalFile(file.absoluteFilePath()));
    addFiles(urls);
    if (urls.isEmpty()) {
        m_message = tr("No media candidates were found in this directory.");
        emit statusChanged();
    }
}

void MediaWorkspace::addCurrent()
{
    if (!m_hasPreview || m_preview.id != m_selectedId || m_preview.state != Ready)
        return;
    const int existing = findStagedPath(m_preview.path);
    if (existing >= 0) { selectFile(m_files[existing].id); return; }
    m_files.append(m_preview);
    m_hasPreview = false;
    normalizeOutputMode();
    rebuildRows();
    emit filesChanged();
    emit selectedChanged();
}

void MediaWorkspace::dispatchProbe()
{
    if (m_processing) return;
    while (!m_requests.isEmpty()) {
        const Request request = m_requests.dequeue();
        if (!findFile(request.id)) continue; // Cleared/replaced previews are obsolete.
        m_activeId = request.id;
        m_processing = true;
        emit statusChanged();
        m_probe.probe(QUrl::fromLocalFile(request.path));
        return;
    }
    emit statusChanged();
}

void MediaWorkspace::probeCompleted(bool success)
{
    const QString id = m_activeId;
    m_activeId.clear();
    m_processing = false;
    if (MediaFile *file = findFile(id)) {
        file->state = success ? Ready : Error;
        file->metadata = success ? m_probe.metadata() : QVariantMap();
        file->error = success ? QString() : m_probe.error();
        if (!success) m_error = file->path + QStringLiteral("\n") + file->error;
        rebuildRows();
        emit filesChanged();
        emit selectedChanged();
    }
    emit statusChanged();
    QTimer::singleShot(0, this, &MediaWorkspace::dispatchProbe);
}

void MediaWorkspace::selectFile(const QString &id)
{
    if (findFile(id)) { m_selectedId = id; emit selectedChanged(); }
}
void MediaWorkspace::selectIndex(int index)
{
    if (index >= 0 && index < count()) selectFile(m_files[index].id);
}
void MediaWorkspace::normalizeOutputMode()
{
    if (count() > 1 && !m_outputDirectory) {
        m_outputDirectory = true;
        m_outputPath.clear();
        emit settingsChanged();
    }
}
void MediaWorkspace::removeFile(const QString &id)
{
    for (int i = 0; i < count(); ++i) {
        if (m_files[i].id != id) continue;
        if (m_selectedId == id) { m_preview = m_files[i]; m_hasPreview = true; }
        m_files.removeAt(i);
        rebuildRows();
        emit filesChanged();
        emit selectedChanged();
        return;
    }
}
void MediaWorkspace::clearSet()
{
    if (const MediaFile *file = findFile(m_selectedId)) {
        m_preview = *file;
        m_hasPreview = true;
    }
    m_files.clear();
    m_rows.clear();
    m_error.clear();
    if (!m_outputDirectory) {
        m_outputDirectory = true;
        m_outputPath.clear();
        emit settingsChanged();
    }
    emit filesChanged();
    emit rowsChanged();
    emit selectedChanged();
    emit statusChanged();
}

QString MediaWorkspace::defaultCodec(const Row &row) const
{
    if (row.type == QStringLiteral("video"))
        return row.attached ? QStringLiteral("copy") : QStringLiteral("libx264");
    if (row.type == QStringLiteral("audio")) return QStringLiteral("aac");
    return m_container == QStringLiteral("mp4") ? QStringLiteral("mov_text")
        : row.seedCodec == QStringLiteral("mov_text") ? QStringLiteral("srt") : QStringLiteral("copy");
}
QVariantList MediaWorkspace::codecOptions(const Row &row) const
{
    QVariantList result;
    const auto add = [&result](const QString &label, const QString &value) {
        result.append(QVariantMap {{QStringLiteral("label"), label}, {QStringLiteral("value"), value}});
    };
    add(tr("Copy"), QStringLiteral("copy"));
    if (row.type == QStringLiteral("video")) add(QStringLiteral("H.264"), QStringLiteral("libx264"));
    else if (row.type == QStringLiteral("audio")) add(QStringLiteral("AAC"), QStringLiteral("aac"));
    else if (m_container == QStringLiteral("mp4")) add(QStringLiteral("mov_text"), QStringLiteral("mov_text"));
    else add(QStringLiteral("SRT"), QStringLiteral("srt"));
    return result;
}

void MediaWorkspace::rebuildRows()
{
    QHash<QString, Row> old;
    for (const Row &row : m_rows) old.insert(row.id, row);
    QList<Row> rebuilt;
    QHash<QString, int> positions;
    for (const MediaFile &file : m_files) {
        if (file.state != Ready) continue;
        QHash<QString, int> occurrences;
        for (const QVariant &value : file.metadata.value(QStringLiteral("streams")).toList()) {
            const QVariantMap stream = value.toMap();
            const QString type = stream.value(QStringLiteral("codec_type")).toString();
            if (type != QStringLiteral("video") && type != QStringLiteral("audio")
                && type != QStringLiteral("subtitle")) continue;
            const QVariantMap tags = stream.value(QStringLiteral("tags")).toMap();
            const QString title = tags.value(QStringLiteral("title")).toString();
            QString language = normalized(tags.value(QStringLiteral("language")).toString());
            if (language == QStringLiteral("und")) language.clear();
            const bool picture = attached(stream);
            const QString base = QString::fromUtf8(QJsonDocument(QJsonArray {
                type, normalized(title), language, picture}).toJson(QJsonDocument::Compact));
            const int occurrence = ++occurrences[base];
            const QString key = base + QLatin1Char(':') + QString::number(occurrence);
            if (!positions.contains(key)) {
                Row row = old.value(key);
                if (row.id.isEmpty()) {
                    row.id = key;
                    row.type = type;
                    row.title = title;
                    row.language = language;
                    row.occurrence = occurrence;
                    row.attached = picture;
                    row.defaultIncluded = !picture;
                    row.seedCodec = stream.value(QStringLiteral("codec_name")).toString();
                    row.codec = defaultCodec(row);
                }
                row.cells.clear();
                positions.insert(key, int(rebuilt.size()));
                rebuilt.append(row);
            }
            Row &row = rebuilt[positions.value(key)];
            const bool wasPresent = old.contains(key) && old.value(key).cells.contains(file.id);
            Cell cell;
            cell.stream = stream;
            cell.checked = wasPresent ? old.value(key).cells.value(file.id).checked : row.defaultIncluded;
            row.cells.insert(file.id, cell);
        }
    }
    std::stable_sort(rebuilt.begin(), rebuilt.end(), [](const Row &a, const Row &b) {
        return typeOrder(a.type) < typeOrder(b.type);
    });
    m_rows = rebuilt;
    emit rowsChanged();
}

QVariantList MediaWorkspace::rows() const
{
    QVariantList result;
    for (const Row &row : m_rows) {
        QVariantMap cells;
        int enabled = 0;
        for (auto i = row.cells.cbegin(); i != row.cells.cend(); ++i) {
            const Cell &cell = i.value();
            if (cell.checked) ++enabled;
            QVariantMap view = cell.stream;
            view.insert(QStringLiteral("checked"), cell.checked);
            view.insert(QStringLiteral("title"), cell.stream.value(QStringLiteral("tags")).toMap().value(QStringLiteral("title")));
            view.insert(QStringLiteral("language"), cell.stream.value(QStringLiteral("tags")).toMap().value(QStringLiteral("language")));
            cells.insert(i.key(), view);
        }
        const QString kind = row.type == QStringLiteral("video") ? tr("Video")
            : row.type == QStringLiteral("audio") ? tr("Audio") : tr("Subtitles");
        QString label = normalized(row.title).isEmpty() ? tr("%1 %2").arg(kind).arg(row.occurrence)
                                          : kind + QStringLiteral(" — ") + row.title.simplified();
        if (!row.language.isEmpty()) label += QStringLiteral(" [") + row.language + QLatin1Char(']');
        if (row.attached) label += tr(" (cover art)");
        if (!normalized(row.title).isEmpty() && row.occurrence > 1)
            label += QStringLiteral(" #%1").arg(row.occurrence);
        result.append(QVariantMap {
            {QStringLiteral("id"), row.id}, {QStringLiteral("label"), label},
            {QStringLiteral("type"), row.type}, {QStringLiteral("codec"), row.codec},
            {QStringLiteral("codecOptions"), codecOptions(row)}, {QStringLiteral("bitrate"), row.bitrate},
            {QStringLiteral("cells"), cells}, {QStringLiteral("checkState"), enabled == 0 ? 0 : enabled == row.cells.size() ? 2 : 1},
            {QStringLiteral("matchNote"), normalized(row.title).isEmpty() ? tr("Matched by type, language and order") : QString()}
        });
    }
    return result;
}

void MediaWorkspace::setIncluded(const QString &rowId, const QString &fileId, bool checked)
{
    for (Row &row : m_rows)
        if (row.id == rowId && row.cells.contains(fileId)) {
            row.cells[fileId].checked = checked;
            emit rowsChanged();
            return;
        }
}
void MediaWorkspace::setRowIncluded(const QString &rowId, bool checked)
{
    for (Row &row : m_rows) if (row.id == rowId) {
        row.defaultIncluded = checked;
        for (Cell &cell : row.cells) cell.checked = checked;
        emit rowsChanged();
        return;
    }
}
void MediaWorkspace::setRowCodec(const QString &rowId, const QString &codec)
{
    for (Row &row : m_rows) if (row.id == rowId) {
        bool allowed = false;
        for (const QVariant &option : codecOptions(row))
            allowed |= option.toMap().value(QStringLiteral("value")).toString() == codec;
        if (allowed && row.codec != codec) { row.codec = codec; emit rowsChanged(); }
        return;
    }
}
void MediaWorkspace::setRowBitrate(const QString &rowId, int bitrate)
{
    if (bitrate <= 0) return;
    for (Row &row : m_rows) if (row.id == rowId) {
        if (row.bitrate != bitrate) { row.bitrate = bitrate; emit rowsChanged(); }
        return;
    }
}
void MediaWorkspace::setContainer(const QString &container)
{
    if (container == m_container || (container != QStringLiteral("mp4") && container != QStringLiteral("mkv"))) return;
    m_container = container;
    for (Row &row : m_rows)
        if (row.type == QStringLiteral("subtitle")) row.codec = defaultCodec(row);
    if (!m_outputDirectory && !m_outputPath.isEmpty()) {
        const QFileInfo file(m_outputPath);
        m_outputPath = file.dir().filePath(outputName(m_outputPath, container));
    }
    emit settingsChanged();
    emit rowsChanged();
}
void MediaWorkspace::setOutputPath(const QString &path)
{
    if (m_outputPath == path) return;
    m_outputPath = QDir::fromNativeSeparators(path);
    emit settingsChanged();
}
void MediaWorkspace::setOutputIsDirectory(bool directory)
{
    if ((!directory && count() > 1) || directory == m_outputDirectory) return;
    m_outputDirectory = directory;
    m_outputPath.clear();
    emit settingsChanged();
}
void MediaWorkspace::chooseOutputDirectory(const QUrl &url)
{
    if (!url.isLocalFile()) return;
    m_outputDirectory = true;
    m_outputPath = url.toLocalFile();
    emit settingsChanged();
}
void MediaWorkspace::chooseOutputFile(const QUrl &url)
{
    if (count() > 1 || !url.isLocalFile()) return;
    m_outputDirectory = false;
    m_outputPath = url.toLocalFile();
    emit settingsChanged();
}
QString MediaWorkspace::destinationFor(const MediaFile &file) const
{
    return m_outputDirectory ? QDir(m_outputPath).filePath(outputName(file.path, m_container)) : m_outputPath;
}
ConversionJob MediaWorkspace::jobFor(const MediaFile &file, const QString &output) const
{
    ConversionJob job;
    job.inputPath = file.path;
    job.outputPath = output;
    job.program = QStringLiteral("C:/ffmpeg/bin/ffmpeg.exe");
    job.container = m_container;
    job.durationSeconds = file.metadata.value(QStringLiteral("format")).toMap()
        .value(QStringLiteral("duration")).toDouble();
    for (const Row &row : m_rows) {
        if (!row.cells.contains(file.id) || !row.cells.value(file.id).checked) continue;
        const QVariantMap stream = row.cells.value(file.id).stream;
        const QVariantMap tags = stream.value(QStringLiteral("tags")).toMap();
        const QVariantMap disposition = stream.value(QStringLiteral("disposition")).toMap();
        StreamSettings settings;
        settings.type = row.type;
        settings.inputSpecifier = QStringLiteral("0:%1").arg(stream.value(QStringLiteral("index")).toInt());
        settings.codec = row.codec;
        settings.bitrateKbps = row.bitrate;
        settings.sourceCodec = stream.value(QStringLiteral("codec_name")).toString();
        settings.title = tags.value(QStringLiteral("title")).toString();
        settings.language = tags.value(QStringLiteral("language")).toString();
        settings.defaultDisposition = disposition.value(QStringLiteral("default")).toBool();
        settings.forcedDisposition = disposition.value(QStringLiteral("forced")).toBool();
        settings.attachedPicture = attached(stream);
        job.streams.append(settings);
    }
    return job;
}

bool MediaWorkspace::enqueue(ConversionQueue *queue)
{
    m_error.clear();
    if (!queue || m_files.isEmpty()) m_error = tr("Add files to the preparation set first.");
    else if (busy()) m_error = tr("Wait until file analysis has finished.");
    else if (m_outputPath.trimmed().isEmpty()) m_error = tr("Choose an output directory or file.");
    else if (m_outputDirectory && !QFileInfo(m_outputPath).isDir()) m_error = tr("The output directory does not exist.");
    if (!m_error.isEmpty()) { emit statusChanged(); return false; }
    QList<ConversionJob> jobs;
    for (const MediaFile &file : m_files) {
        if (file.state != Ready) {
            m_error = tr("Remove or fix files whose analysis failed.") + QStringLiteral("\n") + file.path;
            emit statusChanged();
            return false;
        }
        jobs.append(jobFor(file, destinationFor(file)));
    }
    if (!queue->enqueueJobs(jobs)) {
        m_error = queue->error();
        emit statusChanged();
        return false;
    }
    m_message = tr("Jobs added to queue: %1.").arg(jobs.size());
    clearSet();
    emit statusChanged();
    return true;
}
QString MediaWorkspace::commandPreview(const QString &fileId) const
{
    const MediaFile *file = findFile(fileId);
    if (!file || file->state != Ready) return tr("Wait for successful file analysis.");
    if (findStagedPath(file->path) < 0) return tr("Add this file to the preparation set first.");
    ConversionJob job = jobFor(*file, m_outputPath.isEmpty() ? tr("<output file>") : destinationFor(*file));
    if (job.streams.isEmpty()) return tr("No streams are selected for this file.");
    return job.preview();
}
