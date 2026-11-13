#include "conversionqueue.h"

#include <QTimer>
#include <QSet>
#include <QUuid>

ConversionQueue::ConversionQueue(QObject *parent)
    : QAbstractListModel(parent), m_runner(this)
{
    connect(&m_runner, &ConversionRunner::stateChanged, this, [this]() {
        updateActiveState();
        emit queueChanged();
    });
    connect(&m_runner, &ConversionRunner::progressChanged, this, [this]() {
        const int row = findRow(m_activeId);
        if (row >= 0) {
            m_entries[row].progress = m_runner.progress();
            emit dataChanged(index(row, 0), index(row, 0), {ProgressRole});
        }
    });
    connect(&m_runner, &ConversionRunner::logChanged, this, [this]() {
        const int row = findRow(m_activeId);
        if (row >= 0) {
            m_entries[row].log = m_runner.log();
            if (m_selectedId == m_activeId)
                emit detailsChanged();
        }
    });
    connect(&m_runner, &ConversionRunner::completed,
            this, &ConversionQueue::onCompleted);
}

int ConversionQueue::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant ConversionQueue::data(const QModelIndex &modelIndex, int role) const
{
    if (!modelIndex.isValid() || modelIndex.column() != 0
        || modelIndex.row() < 0 || modelIndex.row() >= count())
        return {};
    const Entry &entry = m_entries[modelIndex.row()];
    switch (role) {
    case IdRole: return entry.job.id;
    case InputRole: return entry.job.inputPath;
    case OutputRole: return entry.job.outputPath;
    case StateRole: return int(entry.state);
    case StatusRole: return entry.status;
    case ProgressRole: return entry.progress;
    case ErrorRole: return entry.error;
    default: return {};
    }
}

QHash<int, QByteArray> ConversionQueue::roleNames() const
{
    return {{IdRole, "jobId"}, {InputRole, "inputPath"}, {OutputRole, "outputPath"},
            {StateRole, "jobState"}, {StatusRole, "statusText"},
            {ProgressRole, "jobProgress"}, {ErrorRole, "jobError"}};
}

int ConversionQueue::count() const { return int(m_entries.size()); }
int ConversionQueue::pendingCount() const
{
    int count = 0;
    for (const Entry &entry : m_entries)
        if (entry.state == Waiting)
            ++count;
    return count;
}
bool ConversionQueue::running() const { return m_running; }
bool ConversionQueue::busy() const { return m_runner.busy(); }
bool ConversionQueue::stopRequested() const { return m_stopRequested; }
QString ConversionQueue::error() const { return m_error; }
ConversionRunner *ConversionQueue::runner() { return &m_runner; }
QString ConversionQueue::selectedLog() const
{
    const int row = findRow(m_selectedId);
    return row >= 0 ? m_entries[row].log : QString();
}

bool ConversionQueue::enqueue(ConversionCommand *command, double durationSeconds)
{
    m_error.clear();
    if (!command || !command->refresh()) {
        m_error = command ? command->error() : tr("No conversion settings were provided.");
        emit queueChanged();
        return false;
    }

    return enqueueJobs({command->createJob(durationSeconds)});
}

bool ConversionQueue::enqueueJobs(const QList<ConversionJob> &jobs)
{
    m_error.clear();
    if (jobs.isEmpty()) {
        m_error = tr("Add at least one waiting job.");
        emit queueChanged();
        return false;
    }
    QSet<QString> outputs;
    for (const Entry &entry : m_entries)
        if (!terminal(entry.state))
            outputs.insert(ConversionJob::pathKey(entry.job.outputPath));
    QList<Entry> prepared;
    for (ConversionJob job : jobs) {
        const QString reason = job.validationError();
        if (!reason.isEmpty()) {
            m_error = job.inputPath + QStringLiteral("\n") + reason;
            emit queueChanged();
            return false;
        }
        const QString key = ConversionJob::pathKey(job.outputPath);
        if (outputs.contains(key)) {
            m_error = tr("A waiting or running job already uses this output path.")
                + QStringLiteral("\n") + job.outputPath;
            emit queueChanged();
            return false;
        }
        outputs.insert(key);
        job.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        Entry entry;
        entry.job = job;
        entry.status = tr("Waiting.");
        prepared.append(entry);
    }
    // Commit only after every item and every destination passed validation.
    const int first = count();
    beginInsertRows(QModelIndex(), first, first + int(prepared.size()) - 1);
    m_entries.append(prepared);
    endInsertRows();
    if (m_selectedId.isEmpty()) {
        m_selectedId = prepared.first().job.id;
        emit detailsChanged();
    }
    emit queueChanged();
    return true;
}

void ConversionQueue::start()
{
    if (m_running || busy())
        return;
    m_error.clear();
    if (pendingCount() == 0) {
        m_error = tr("Add at least one waiting job.");
        emit queueChanged();
        return;
    }
    m_running = true;
    m_stopRequested = false;
    emit queueChanged();
    QTimer::singleShot(0, this, &ConversionQueue::dispatchNext);
}

void ConversionQueue::dispatchNext()
{
    if (!m_running || busy())
        return;
    if (m_stopRequested) {
        m_running = false;
        m_stopRequested = false;
        emit queueChanged();
        return;
    }
    int row = -1;
    for (int i = 0; i < count(); ++i) {
        if (m_entries[i].state == Waiting) {
            row = i;
            break;
        }
    }
    if (row < 0) {
        m_running = false;
        emit queueChanged();
        return;
    }

    const ConversionJob job = m_entries[row].job;
    m_activeId = job.id;
    m_selectedId = job.id;
    m_entries[row].state = Starting;
    m_entries[row].status = tr("Starting FFmpeg…");
    emit dataChanged(index(row, 0), index(row, 0));
    emit detailsChanged();
    emit queueChanged();
    m_runner.start(job);
}

void ConversionQueue::updateActiveState()
{
    const int row = findRow(m_activeId);
    if (row < 0 || !busy())
        return;
    switch (m_runner.state()) {
    case ConversionRunner::Starting: m_entries[row].state = Starting; break;
    case ConversionRunner::Running: m_entries[row].state = Running; break;
    case ConversionRunner::Cancelling: m_entries[row].state = Cancelling; break;
    default: return;
    }
    m_entries[row].status = m_runner.statusText();
    emit dataChanged(index(row, 0), index(row, 0), {StateRole, StatusRole});
}

void ConversionQueue::onCompleted(ConversionRunner::State state)
{
    const int row = findRow(m_activeId);
    if (row < 0)
        return;
    Entry &entry = m_entries[row];
    entry.state = state == ConversionRunner::Succeeded ? Succeeded
        : state == ConversionRunner::Cancelled ? Cancelled : Failed;
    entry.status = m_runner.statusText();
    entry.error = m_runner.error();
    entry.log = m_runner.log();
    entry.progress = m_runner.progress();
    m_activeId.clear();
    emit dataChanged(index(row, 0), index(row, 0));
    emit detailsChanged();

    if (m_stopRequested) {
        m_running = false;
        m_stopRequested = false;
    }
    emit queueChanged();
    if (m_running)
        QTimer::singleShot(0, this, &ConversionQueue::dispatchNext);
}

void ConversionQueue::cancelCurrent()
{
    if (busy())
        m_runner.cancel();
}

void ConversionQueue::requestStopAfterCurrent()
{
    if (!m_running && !busy())
        return;
    m_stopRequested = true;
    if (!busy()) {
        m_running = false;
        m_stopRequested = false;
    }
    emit queueChanged();
}

void ConversionQueue::removeWaiting(const QString &id)
{
    const int row = findRow(id);
    if (row < 0 || m_entries[row].state != Waiting)
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_entries.removeAt(row);
    endRemoveRows();
    if (m_selectedId == id) {
        m_selectedId.clear();
        emit detailsChanged();
    }
    emit queueChanged();
}

void ConversionQueue::clearFinished()
{
    for (int row = count() - 1; row >= 0; --row) {
        if (terminal(m_entries[row].state)) {
            if (m_selectedId == m_entries[row].job.id)
                m_selectedId.clear();
            beginRemoveRows(QModelIndex(), row, row);
            m_entries.removeAt(row);
            endRemoveRows();
        }
    }
    emit detailsChanged();
    emit queueChanged();
}

void ConversionQueue::selectJob(const QString &id)
{
    if (findRow(id) >= 0) {
        m_selectedId = id;
        emit detailsChanged();
    }
}

int ConversionQueue::findRow(const QString &id) const
{
    if (id.isEmpty())
        return -1;
    for (int row = 0; row < count(); ++row)
        if (m_entries[row].job.id == id)
            return row;
    return -1;
}

bool ConversionQueue::terminal(JobState state)
{
    return state == Succeeded || state == Failed || state == Cancelled;
}
