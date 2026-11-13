#include "conversionrunner.h"

#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <cmath>

ConversionRunner::ConversionRunner(QObject *parent)
    : QObject(parent)
{
    m_cancelTimer.setSingleShot(true);
    m_cancelTimer.setInterval(5000);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.setInputChannelMode(QProcess::ManagedInputChannel);

    connect(&m_process, &QProcess::started, this, [this]() {
        if (m_cancelRequested)
            requestGracefulStop();
        else
            setState(Running);
    });
    connect(&m_process, &QProcess::readyReadStandardOutput,
            this, [this]() { readProgress(); });
    connect(&m_process, &QProcess::readyReadStandardError,
            this, &ConversionRunner::readDiagnostics);
    connect(&m_process, &QProcess::finished,
            this, &ConversionRunner::handleFinished);

    connect(&m_process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError processError) {
        if (!busy())
            return;
        if (processError == QProcess::FailedToStart) {
            m_error = tr("Could not start FFmpeg: %1").arg(m_process.errorString());
            complete(Failed);
        } else if (processError != QProcess::Crashed && !m_cancelRequested) {
            m_error = tr("FFmpeg I/O error: %1").arg(m_process.errorString());
            emit resultChanged();
            m_process.kill();
        }
        // A crash also emits finished(); classify it there after draining output.
    });

    connect(&m_cancelTimer, &QTimer::timeout, this, [this]() {
        if (busy() && m_process.state() != QProcess::NotRunning) {
            m_forcedStop = true;
            m_process.kill();
        }
    });
}

ConversionRunner::~ConversionRunner()
{
    m_cancelTimer.stop();
    disconnect(&m_process, nullptr, this, nullptr);
    if (m_process.state() != QProcess::NotRunning)
        m_process.kill();
}

ConversionRunner::State ConversionRunner::state() const { return m_state; }
bool ConversionRunner::busy() const
{
    return m_state == Starting || m_state == Running || m_state == Cancelling;
}
bool ConversionRunner::cancelling() const { return m_state == Cancelling; }
double ConversionRunner::progress() const { return m_progress; }
QString ConversionRunner::error() const { return m_error; }
QString ConversionRunner::outputPath() const { return m_outputPath; }
QString ConversionRunner::log() const { return m_log; }

QString ConversionRunner::statusText() const
{
    switch (m_state) {
    case Idle: return tr("Ready to convert.");
    case Starting: return tr("Starting FFmpeg…");
    case Running: return tr("Converting…");
    case Cancelling: return tr("Stopping FFmpeg…");
    case Succeeded: return tr("Conversion completed.");
    case Failed: return tr("Conversion failed. Any created output file was kept.");
    case Cancelled:
        return m_forcedStop
            ? tr("Conversion forcibly stopped. Any created output file was kept and may be unreadable.")
            : tr("Conversion cancelled. Any created output file was kept.");
    }
    return {};
}

void ConversionRunner::start(const ConversionJob &job)
{
    if (busy())
        return;

    const ConversionJob snapshot = job;
    m_cancelTimer.stop();
    m_error.clear();
    m_log.clear();
    m_outputPath.clear();
    m_progressBuffer.clear();
    m_stderrDecoder.resetState();
    m_cancelRequested = false;
    m_quitSent = false;
    m_forcedStop = false;
    m_durationSeconds = std::isfinite(snapshot.durationSeconds) && snapshot.durationSeconds > 0
        ? snapshot.durationSeconds : 0;
    m_progress = m_durationSeconds > 0 ? 0 : -1;
    emit progressChanged();
    emit logChanged();

    m_error = snapshot.validationError();
    if (!m_error.isEmpty()) {
        complete(Failed);
        return;
    }

    const QString program = snapshot.program;
    QStringList arguments {
        QStringLiteral("-progress"), QStringLiteral("pipe:1"),
        QStringLiteral("-nostats")
    };
    arguments.append(snapshot.arguments());
    m_outputPath = snapshot.outputPath;
    emit resultChanged();
    setState(Starting);
    m_process.start(program, arguments);
}

void ConversionRunner::cancel()
{
    if (!busy() || m_cancelRequested)
        return;
    // If termination is already observable, let finished() report its actual result.
    if (m_process.state() == QProcess::NotRunning)
        return;

    m_cancelRequested = true;
    setState(Cancelling);
    m_cancelTimer.start();
    requestGracefulStop();
}

void ConversionRunner::requestGracefulStop()
{
    if (!m_quitSent && m_process.state() == QProcess::Running) {
        m_quitSent = true;
        // FFmpeg's interactive quit request allows it to finalize the container.
        // Do not add -nostdin: this pipe is intentionally used for cancellation.
        m_process.write("q\n");
    }
}

void ConversionRunner::readProgress(bool finalChunk)
{
    m_progressBuffer.append(m_process.readAllStandardOutput());
    qsizetype newline = -1;
    while ((newline = m_progressBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_progressBuffer.left(newline).trimmed();
        m_progressBuffer.remove(0, newline + 1);
        parseProgressLine(line);
    }
    if (finalChunk && !m_progressBuffer.isEmpty()) {
        parseProgressLine(m_progressBuffer.trimmed());
        m_progressBuffer.clear();
    }
    // FFmpeg progress lines are short; cap unexpected unterminated output.
    if (m_progressBuffer.size() > 65536)
        m_progressBuffer.clear();
}

void ConversionRunner::parseProgressLine(const QByteArray &line)
{
    if (m_durationSeconds <= 0)
        return;
    const qsizetype separator = line.indexOf('=');
    if (separator < 0)
        return;
    const QByteArray key = line.left(separator);
    const QByteArray value = line.mid(separator + 1);
    double seconds = -1;

    if (key == "out_time_us") {
        bool ok = false;
        const qint64 microseconds = value.toLongLong(&ok);
        if (ok && microseconds >= 0)
            seconds = microseconds / 1000000.0;
    } else if (key == "out_time" && !value.startsWith('-')) {
        // Fallback for versions without out_time_us. Do not assume out_time_ms is ms.
        const QList<QByteArray> parts = value.split(':');
        if (parts.size() == 3) {
            bool hoursOk = false, minutesOk = false, secondsOk = false;
            const double hours = parts[0].toDouble(&hoursOk);
            const double minutes = parts[1].toDouble(&minutesOk);
            const double remainder = parts[2].toDouble(&secondsOk);
            if (hoursOk && minutesOk && secondsOk)
                seconds = hours * 3600 + minutes * 60 + remainder;
        }
    }

    if (std::isfinite(seconds) && seconds >= 0) {
        // End-of-progress does not imply successful process exit. Reserve 100% for it.
        const double next = std::max(m_progress,
            std::clamp(seconds / m_durationSeconds, 0.0, 0.99));
        if (next != m_progress) {
            m_progress = next;
            emit progressChanged();
        }
    }
}

void ConversionRunner::readDiagnostics()
{
    const QByteArray bytes = m_process.readAllStandardError();
    if (!bytes.isEmpty()) {
        const QString text = m_stderrDecoder(bytes);
        appendLog(text);
    }
}

void ConversionRunner::appendLog(const QString &text)
{
    if (text.isEmpty())
        return;
    m_log.append(text);
    if (m_log.size() > 65536)
        m_log.remove(0, m_log.size() - 65536);
    emit logChanged();
}

void ConversionRunner::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    readProgress(true);
    readDiagnostics();

    if (m_cancelRequested) {
        complete(Cancelled);
    } else if (exitStatus == QProcess::CrashExit) {
        if (m_error.isEmpty())
            m_error = tr("FFmpeg crashed. See the log for details.");
        complete(Failed);
    } else if (exitCode != 0 || !m_error.isEmpty()) {
        if (m_error.isEmpty())
            m_error = tr("FFmpeg exited with code %1. See the log for details.").arg(exitCode);
        complete(Failed);
    } else if (!QFileInfo(m_outputPath).isFile()) {
        m_error = tr("FFmpeg exited without creating the output file.");
        complete(Failed);
    } else {
        m_progress = 1;
        emit progressChanged();
        complete(Succeeded);
    }
}

void ConversionRunner::complete(State state)
{
    m_cancelTimer.stop();
    emit resultChanged();
    // Publish the terminal state last: its observers may prepare the next task.
    setState(state);
    emit completed(state);
}

void ConversionRunner::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}
