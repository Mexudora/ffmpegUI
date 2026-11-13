#pragma once

#include "conversionjob.h"

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringDecoder>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class ConversionRunner : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool cancelling READ cancelling NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString error READ error NOTIFY resultChanged)
    Q_PROPERTY(QString outputPath READ outputPath NOTIFY resultChanged)
    Q_PROPERTY(QString log READ log NOTIFY logChanged)

public:
    enum State { Idle, Starting, Running, Cancelling, Succeeded, Failed, Cancelled };
    Q_ENUM(State)

    explicit ConversionRunner(QObject *parent = nullptr);
    ~ConversionRunner() override;

    State state() const;
    bool busy() const;
    bool cancelling() const;
    QString statusText() const;
    double progress() const;
    QString error() const;
    QString outputPath() const;
    QString log() const;

    void start(const ConversionJob &job);
    Q_INVOKABLE void cancel();

signals:
    void stateChanged();
    void progressChanged();
    void resultChanged();
    void logChanged();
    void completed(ConversionRunner::State state);

private:
    void setState(State state);
    void readProgress(bool finalChunk = false);
    void parseProgressLine(const QByteArray &line);
    void readDiagnostics();
    void appendLog(const QString &text);
    void requestGracefulStop();
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void complete(State state);

    State m_state = Idle;
    double m_progress = -1.0;
    double m_durationSeconds = 0.0;
    bool m_cancelRequested = false;
    bool m_quitSent = false;
    bool m_forcedStop = false;
    QString m_error;
    QString m_outputPath;
    QString m_log;
    QByteArray m_progressBuffer;
    QStringDecoder m_stderrDecoder {QStringDecoder::Utf8};

    QTimer m_cancelTimer;
    QProcess m_process;
};
